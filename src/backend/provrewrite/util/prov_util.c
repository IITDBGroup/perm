/*-------------------------------------------------------------------------
 *
 * prov_util.c
 *	  POSTGRES C utitlity funcions for provenance module
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/prov_util.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "access/heapam.h"
#include "catalog/pg_operator.h"		// pg_operator system table for operator lookup
#include "nodes/makefuncs.h"
#include "parser/parsetree.h"			// rt_fetch
#include "parser/parse_expr.h"
#include "parser/parse_oper.h"			// defintion of Operator type and convience routines for operator lookup
#include "parser/parse_relation.h"
#include "parser/parse_coerce.h"
#include "optimizer/clauses.h"
#include "utils/syscache.h"				// used to release heap tuple references
#include "utils/lsyscache.h"

#include "provrewrite/provlog.h"
#include "provrewrite/provstack.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_nodes.h"



/* prototypes */
static void getViewRelations (Oid relid, List **result);
static void findBaseRelationsForProvenanceRTERecurse (RangeTblEntry *rte, List **result, bool viewCheck);
static bool isRuleRTE (RangeTblEntry *rte);
static bool getSublinkBaseRelationsWalker (Node *node, List **context);
static void findSublinkBaseRelations (Query *query, List **result);
static bool aggrExprWalker (Node *node, List** context);
static void getJoinForJoinQualWalker (Node *node, JoinQualWalkerContext *context);
static void recreateJoinRTE (JoinExpr *join, Query *query);
static Node *createComparison (Node *left, Node *right, ComparisonType type);
static void fromItemWalker (Node *node, GetFromItemWalkerContext *context);
static Node *replaceSubExpressionsMutator (Node *node, ReplaceSubExpressionsContext *context);
static bool findSubExpressionWalker (Node *node, FindSubExpressionWalkerContext *context);
static bool removeProvInfoNodesWalker (Node *node, void *context);
static bool queryHasRewriteChildrenWalker (Node *node, bool *context);
static Node *getJoinTreeNodeWalker (Node *node, Index rtindex);
static Node *getCastedVarOrConst (Node *node);
static bool containsOuterJoins (Node *join);
static bool correctSublinkAliasWalker (Node *node, void *context);

/*
 *
 */
bool
ignoreRTE(RangeTblEntry *rte)
{
	if (list_length(rte->provAttrs) == 0 || list_length(rte->provAttrs) > 1)
		return false;

	return (rte->provAttrs->head->data.ptr_value == NULL);
}

/*
 *
 */

void
setIgnoreRTE (RangeTblEntry *rte)
{
	rte->provAttrs = list_make1(NULL);
}

/*
 * Adds provenance attributes for the range table entries of query starting from element min and ending
 * with element max.
 */

List *
addProvenanceAttrsForRange (Query *query, int min, int max, List *pList)
{
	List *subList;
	int i;

	subList = NIL;
	for(i = min; i < max; i++)
	{
		subList = lappend_int(subList, i);
	}

	return addProvenanceAttrs (query, subList, pList);
}

/*
 * For a given query node this methods pops the provenance attributes
 * of each subquery from pStack and appends them to the target list
 * and to pList of the query.
 */

List *
addProvenanceAttrs (Query *query, List *subList, List *pList)
{
	ListCell *subqLc;
	ListCell *pTeLc;
	List *targetList;		/* new targetlist for query */
	List *subPStack;
	List *curPSet;			/* currently processed pSet */
	TargetEntry *te;		/* a TargetEntry from current pSet */
	TargetEntry *newTe;		/* new TargetEntry derived from te */
	Expr *expr;
	Index curSubquery;
	AttrNumber curResno;

	targetList = query->targetList;
	curResno = list_length(query->targetList) + 1;

	subPStack = popListAndReverse (&pStack, list_length(subList));

	/* for each subquery of query ... */
	foreach (subqLc, subList)
	{
		curSubquery = (Index) lfirst_int(subqLc);

		/* pull next pSet from pStack */
		curPSet = (List *) pop (&subPStack);
		logPList(curPSet);

		/* add each element of pSet to targetList and pList */
		foreach (pTeLc, curPSet)
		{
			te = (TargetEntry *) lfirst(pTeLc);

			/* create new TE */
			expr = (Expr *) makeVar (curSubquery,
						te->resno,
						exprType ((Node *) te->expr),
						exprTypmod ((Node *) te->expr),
						0);
			newTe = makeTargetEntry(expr, curResno, te->resname, false);

			/* adapt varno und varattno if referenced rte is used in a join-RTE */
			getRTindexForProvTE (query, (Var *) expr);

			/* append to targetList and pList */
			targetList = lappend (targetList, newTe);
			pList = lappend (pList, newTe);

			/* increase current resno */
			curResno++;
		}

	}

	/* replace old targetList of query */
	query->targetList = targetList;

	/* return changed pList */
	return pList;
}

/*
 * 	Appends a subquery to a queries range table.
 */

void
addSubqueryToRT (Query *query, Query* subQuery, char *aliasName)
{
	addSubqueryToRTWithParam(query, subQuery, aliasName, true, ACL_SELECT, true);
}

/*
 * Appends a subquery to a queries range table and sets the inFrom and
 * requiredPerms fields of the RTE to the values supplied by the caller.
 */

void
addSubqueryToRTWithParam (Query *query, Query *subQuery, char *aliasName,
		bool inFrom, AclMode reqPerms, bool append)
{
	RangeTblEntry *newRte;

	/* create new range table entry */
	newRte = makeRte(RTE_SUBQUERY);

	newRte->alias->aliasname = aliasName;
	newRte->eref->aliasname = aliasName;
	newRte->inFromCl = inFrom;
	newRte->requiredPerms = reqPerms;
	newRte->subquery = subQuery;

	/* append to range table */
	if (append)
		query->rtable = lappend (query->rtable, newRte);
	else
		query->rtable = lcons (newRte, query->rtable);
}

/*
 * Recursively adapt all subquery aliases.
 */

void
correctRecurSubQueryAlias (Query *query)
{
	ListCell *lc;
	RangeTblEntry *rte;

	/* first recurse then correct top level alias*/
	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		if (rte->rtekind == RTE_SUBQUERY)
			correctRecurSubQueryAlias(rte->subquery);
	}

	if (query->hasSubLinks)
		query_tree_walker(query, correctSublinkAliasWalker, NULL, 0);

	correctSubQueryAlias(query);
}

/*
 * Find sublinks in query and adapt their column names.
 */

static bool
correctSublinkAliasWalker (Node *node, void *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, SubLink))
	{
		SubLink *sublink;

		sublink = (SubLink *) node;
		correctRecurSubQueryAlias((Query *) sublink->subselect);

		return expression_tree_walker(sublink->testexpr,
				correctSublinkAliasWalker, context);
	}

	return expression_tree_walker(node, correctSublinkAliasWalker, context);
}

/*
 * Creates the eref colnames list for all subqueries in an query's range table.
 */

void
correctSubQueryAlias (Query *query)
{
	ListCell *lc;
	RangeTblEntry *rte;

	foreach (lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		correctRTEAlias (rte);
	}
}

/*
 * Creates an the eref column name list for a subquery range table entry.
 */

void
correctRTEAlias (RangeTblEntry *rte)
{
	ListCell *lc;
	Query *subQuery;
	TargetEntry *te;
	List *colnames;

	colnames = NIL;

	if (rte->rtekind == RTE_SUBQUERY)
	{
		subQuery = rte->subquery;

		foreach (lc, subQuery->targetList)
		{
			te = (TargetEntry *) lfirst(lc);
			colnames = lappend(colnames, makeString(te->resname)); //TODO copy String
		}

		rte->eref->colnames = colnames;

		if (!rte->alias)
			rte->alias = copyObject(rte->eref);

		if (rte->alias && rte->alias->colnames)
			rte->alias->colnames = copyObject(colnames);
	}
}

/*
 * Adapt a provenance attribute var. If the Var's RTE is used in a join expr
 * change the varno and varattno values to the corresponding var of the top
 * level join expression.
 */

void
getRTindexForProvTE (Query *query, Var* var)
{
	List *from;
	ListCell *lc;
	ListCell *aliasLc;
	JoinExpr *joinExpr;
	Index newRtindex;
	Index rtindex;
	AttrNumber newAttrNumber;
	AttrNumber foundAttrNum;
	RangeTblEntry *rte;
	Var *joinVar;
	List *joinRTElist;

	from = query->jointree->fromlist;
	rtindex = var->varno;
	joinRTElist = NIL;


	/* search in all from items for a reference to the RTE of var */
	foreach(lc, from)
	{
		if (IsA(lfirst(lc), JoinExpr))
		{
			joinExpr = (JoinExpr *) lfirst(lc);
			if (findRTindexInJoin (rtindex, joinExpr, &joinRTElist, NULL))
				break;
		}
	}

	ereport (DEBUG1,
			(errcode(ERRCODE_DIVISION_BY_ZERO),
					errmsg("joinlistlength: %i", list_length(joinRTElist))));

	/*
	 * resolve the RT indirections induced by join operations.
	 * for each join-RTE adapt var to point to the join-RTE.
	 * Start with the original RTE at the bottom of the jointree.
	 */
	foreach(lc,joinRTElist)
	{
		newRtindex = lfirst_int(lc);
		rte = rt_fetch(newRtindex, query->rtable);

		newAttrNumber = 1;
		foreach (aliasLc, rte->joinaliasvars)
		{
			joinVar = (Var *) lfirst(aliasLc);
			if ((joinVar->varno == var->varno)
					&& (joinVar->varattno == var->varattno))
			{
				foundAttrNum = newAttrNumber;
			}
			newAttrNumber++;
		}

		var->varno = newRtindex;
		var->varattno = foundAttrNum;
	}
}

/*
 *
 */

bool
findRTindexInFrom (Index rtindex, Query *query, List** rtList, List **joinPath)
{
	ListCell *lc;
	Node *fromItem;

	foreach(lc, query->jointree->fromlist)
	{
		fromItem = (Node *) lfirst(lc);

		if (IsA(fromItem, JoinExpr))
		{
			if(findRTindexInJoin(rtindex, (JoinExpr *) fromItem, rtList,
					joinPath))
				return true;
		}
		else
		{
			RangeTblRef *rtRef;

			rtRef = (RangeTblRef *) fromItem;

			if(rtRef->rtindex == rtindex)
			{
				*rtList = list_make1_int(rtindex);
				if(joinPath)
					*joinPath = list_make1_int(JCHILD_RTREF);
				return true;
			}
		}
	}

	return false;
}

/*
 * Searches for a reference of a RTE in a Jointree. If a  reference to rtindex
 * is found the rtindex values of all parent nodes are appended to the rtList.
 */

bool
findRTindexInJoin (Index rtindex, JoinExpr *joinExpr, List** rtList,
		List **joinPath)
{
	RangeTblRef *rtRef;

	/* left arg is range table reference ? */
	if (IsA(joinExpr->larg,RangeTblRef))
	{
		rtRef = (RangeTblRef *) joinExpr->larg;
		if (rtRef->rtindex == rtindex)
		{
			*rtList = lappend_int(*rtList, joinExpr->rtindex);
			if(joinPath)
				*joinPath = lappend_int(*joinPath, JCHILD_LEFT);

			return true;
		}
	}
	else {
		if (findRTindexInJoin (rtindex, (JoinExpr *) joinExpr->larg,
				rtList, joinPath))
		{
			*rtList = lappend_int(*rtList, joinExpr->rtindex);
			if(joinPath)
				*joinPath = lappend_int(*joinPath, JCHILD_LEFT);

			return true;
		}
	}

	/* right arg is range table reference ? */
	if (IsA(joinExpr->rarg,RangeTblRef))
	{
		rtRef = (RangeTblRef *) joinExpr->rarg;
		if (rtRef->rtindex == rtindex)
		{
			*rtList = lappend_int(*rtList, joinExpr->rtindex);
			if(joinPath)
				*joinPath = lappend_int(*joinPath, JCHILD_RIGHT);

			return true;
		}
	}
	else {
		if (findRTindexInJoin (rtindex, (JoinExpr *) joinExpr->rarg, rtList,
				joinPath))
		{
			*rtList = lappend_int(*rtList, joinExpr->rtindex);
			if(joinPath)
				*joinPath = lappend_int(*joinPath, JCHILD_RIGHT);

			return true;
		}
	}

	return false;
}

/*
 * For a given query returns the a List of the base relations accessed by the
 * query.
 */

List *
findBaseRelationsForProvenanceQuery (Query *query)
{
	List *result;
	ListCell *lc;
	RangeTblEntry *rte;

	result = NIL;

	findSublinkBaseRelations(query, &result);
	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lc;
		findBaseRelationsForProvenanceRTE (rte, &result);
	}

	return result;
}

/*
 *
 */

void
findBaseRelationsForProvenanceRTE (RangeTblEntry *rte, List **result)
{
	findBaseRelationsForProvenanceRTERecurse (rte, result, true);
}

/*
 * Returns a List of RTEs for the base relations accessed by a provenance
 * query.
 */

static void
findBaseRelationsForProvenanceRTERecurse (RangeTblEntry *rte, List **result,
		bool viewCheck)
{
	ListCell *lc;
	RangeTblEntry *subRTE;

	if (rte->isProvBase)
		*result = lappend(*result, rte);
	else if (rte->rtekind == RTE_SUBQUERY)
	{
		findSublinkBaseRelations (rte->subquery, result);
		foreach(lc, rte->subquery->rtable)
		{
			subRTE = (RangeTblEntry *) lfirst(lc);

			findBaseRelationsForProvenanceRTE (subRTE, result);
		}
	}
	else if (rte->rtekind == RTE_RELATION)
	{
		/* if relation is a view find base relations accessed by the view */
		if (viewCheck && get_rel_relkind(rte->relid) == 'v')
			getViewRelations(rte->relid, result);
		else
			*result = lappend(*result, rte);
	}

}

/*
 *	Calls getSublinkBaseRelationsWalker for all possible locations of sublinks
 *	in a query.
 */

static void
findSublinkBaseRelations (Query *query, List **result)
{
	ListCell *lc;
	TargetEntry *te;
	List *groupByTes;

	Assert(query != NULL && IsA(query, Query));

	/* search for target list sublinks */
	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);

		/* check that target entry is not used in group by, because group by
		 * is handled below */
		if (!isUsedInGroupBy(query,te))
		{
			getSublinkBaseRelationsWalker ((Node *) te->expr, result);
		}
	}
	/* search for qual sublinks */
	if (query->jointree)
		getSublinkBaseRelationsWalker ((Node *) query->jointree, result);

	/* search for group by sublinks */
	groupByTes = getGroupByTLEs (query);
	foreach(lc, groupByTes)
	{
		getSublinkBaseRelationsWalker ((Node *)((TargetEntry *)
				lfirst(lc))->expr, result);
	}

	/* search for target list sublinks */
	if (query->havingQual)
		getSublinkBaseRelationsWalker (query->havingQual, result);

	/* search for order by sublinks */
	if (query->sortClause)
		getSublinkBaseRelationsWalker ((Node *) query->sortClause, result);
}

/*
 * Helper method for findBaseRelationsForProvenanceRTE that searches for
 * sublinks and adds the base relations accessed by a sublink query to the List
 *  "result".
 */

static bool
getSublinkBaseRelationsWalker (Node *node, List **context)
{
	ListCell *lc;
	RangeTblEntry *rte;

	if (node == NULL)
		return false;

	/* if a sublink is found add use findBaseRelationsForProvenanceRTERecurse
	 * to add its base relations to the result */
	if (IsA(node,SubLink))
	{
		SubLink *sublink;

		sublink = (SubLink *) node;
		foreach(lc, ((Query *) sublink->subselect)->rtable)
		{
			rte = (RangeTblEntry *) lfirst(lc);
			findBaseRelationsForProvenanceRTERecurse (rte, context, true);
		}

		return false;
	}

	// recurse
	return expression_tree_walker(node, getSublinkBaseRelationsWalker,
			(void *) context);
}

/*
 * Expands a view and adds all base relations accessed by the view to the List
 * "result".
 */

static void
getViewRelations (Oid relid, List **result)
{
	ListCell *lc;
	RangeTblEntry *rte;
	Relation rel;
	Query *ruleQuery;
	int i;
	RewriteRule *rule;

	rel = heap_open(relid, AccessShareLock);

	/* find SELECT rule for view */
	for(i = 0; i < rel->rd_rules->numLocks; i++)
	{
		rule = rel->rd_rules->rules[i];
		/* consider only SELECT rules */
		if (rule->event == CMD_SELECT)
		{
			ruleQuery = (Query *) linitial(rule->actions);

			/* recursivly search for relation RTE in view query */
			foreach(lc, ruleQuery->rtable)
			{
				rte = (RangeTblEntry *) lfirst(lc);

				/* don't recurse into *OLD* and *NEW* entries! */
				if (!isRuleRTE(rte))
				{
					findBaseRelationsForProvenanceRTE(rte, result);
				}
			}
		}
	}
	heap_close(rel, AccessShareLock);
}

/*
 * Checks if a RangeTblEntry is one of the special RangeTblEntry inserted by
 * postgres rewrite rules.
 */

static bool
isRuleRTE (RangeTblEntry *rte)
{
	if (rte->eref == NULL)
		return false;
	return (strncmp(rte->eref->aliasname, "*OLD*", 5) == 0)
		|| (strncmp(rte->eref->aliasname, "*NEW*", 5) == 0);
}

/*
 * Creates a equality condition for two var nodes of the same type.
 */

Node *
createEqualityConditionForVars (Var *leftChild, Var *rightChild)
{
	return createEqualityConditionForNodes ((Node *) leftChild, (Node *) rightChild);
}

/*
 * creates an equality condition for two nodes.
 */

Node *
createEqualityConditionForNodes (Node *left, Node *right)
{
	return createComparison(left, right, COMP_EQUAL);
}

/*
 * Creates a smallerThan condition for two nodes;
 */

Node *
createSmallerCondition (Node *left, Node *right)
{
	return createComparison(left, right, COMP_SMALLER);
}

/*
 *
 */

Node *
createSmallerEqCondition (Node *left, Node *right)
{
	return createComparison(left, right, COMP_SMALLEREQ);
}

/*
 * Creates a biggerThan condition for two nodes.
 */

Node *
createBiggerCondition (Node *left, Node *right)
{
	return createComparison(left,right, COMP_BIGGER);
}

/*
 * Create an comparison condition.
 */

static Node *
createComparison (Node *left, Node *right, ComparisonType type)
{
	Form_pg_operator operator;
	OpExpr *equal;
	Operator operTuple;
	Oid leftType;
	Oid rightType;
	List *opName;
	leftType = exprType(left);
	rightType = exprType(right);

        switch(type)
        {
        case COMP_SMALLER:
          opName = list_make1(makeString("<"));
            break;
        case COMP_SMALLEREQ:
          opName = list_make1(makeString("<="));
          break;
        case COMP_BIGGER:
          opName = list_make1(makeString(">"));
          break;
        case COMP_BIGGEREQ:
          opName = list_make1(makeString(">="));
          break;
        case COMP_EQUAL:
          opName = list_make1(makeString("="));
          break;
        default:
          //TODO error
            break;
        }

        // both types are the same use simple approach
	if (leftType == rightType)
        {
          switch(type)
          {
          case COMP_SMALLER:
            operTuple = ordering_oper (leftType, false);
              break;
          case COMP_BIGGER:
            operTuple = reverse_ordering_oper (leftType, false);
            break;
          case COMP_EQUAL:
            operTuple = equality_oper (leftType, false);
            break;
          case COMP_SMALLEREQ:
          case COMP_BIGGEREQ:
            operTuple = compatible_oper(NULL, opName, leftType, rightType,
            		false, -1);
            break;
          default:
              break;
          }

          operator = (Form_pg_operator) GETSTRUCT(operTuple);
        }
        // different types, try first to get an oper if they are binary
        // otherwise the nodes have be coerced first.
	else
	{
	  operTuple = compatible_oper(NULL, opName, leftType, rightType, true, -1);

	  // try to find operator that would work with type coercion
	  if (operTuple == NULL)
	  {
		  operTuple = oper(NULL, opName, leftType, rightType, true, -1);

		  // did not work out
		  if (operTuple == NULL)
              elog(ERROR,
                  "Could not find an operator %s for types %d, %d",
                  strVal(((Value *) linitial(opName))),
                  leftType,
                  rightType);
	  }

	  // add cast if necessary
	  operator = (Form_pg_operator) GETSTRUCT(operTuple);
	  left = coerce_to_target_type(NULL, left, leftType, operator->oprleft,
			  -1, COERCION_EXPLICIT, COERCE_DONTCARE);
	  right = coerce_to_target_type(NULL, right, rightType, operator->oprright,
			  -1, COERCION_EXPLICIT, COERCE_DONTCARE);
	}

	// create a node for the operator


	equal = makeNode (OpExpr);
	equal->args = list_make2(left, right);
	equal->opfuncid = operator->oprcode;
	equal->opno = HeapTupleGetOid(operTuple);
	equal->opresulttype = operator->oprresult;
	equal->opretset = false;

	ReleaseSysCache (operTuple);

	return (Node *) equal;
}


/*
 * Creates a IS NOT DISTINCT FROM condition for two var nodes of the same type.
 */

Node *
createNotDistinctConditionForVars (Var *leftChild, Var *rightChild)
{
	Form_pg_operator operator;
	DistinctExpr *equal;
	Expr *notExpr;
	Operator operTuple;

	operTuple = equality_oper (leftChild->vartype, true);
	operator = (Form_pg_operator) GETSTRUCT(operTuple);

	equal = makeNode (DistinctExpr);
	equal->args = list_make2(leftChild, rightChild);
	equal->opfuncid = operator->oprcode;
	equal->opno = HeapTupleGetOid(operTuple);
	equal->opresulttype = operator->oprresult;
	equal->opretset = false;

	ReleaseSysCache (operTuple);

	notExpr = makeBoolExpr(NOT_EXPR, list_make1(equal));

	return (Node *) notExpr;
}

/*
 * Create a new top level AND/OR between the queries jointree->qual and the
 * condition. If the jointree qualification is NULL it is replaced by
 * condition.
 */

void
addConditionToQualWithAnd (Query *query, Node *condition, bool and)
{
	if (query->jointree->quals == NULL)
		query->jointree->quals = condition;
	else
	{
		if (and)
			query->jointree->quals = (Node *) makeBoolExpr(AND_EXPR,
					list_make2(query->jointree->quals, condition));
		else
			query->jointree->quals = (Node *) makeBoolExpr(OR_EXPR,
					list_make2(query->jointree->quals, condition));
	}
}

/*
 * Creates a logical AND expression for a list of expressions.
 */

Node *
createAndFromList (List *exprs)
{
	ListCell *lc;
	Node *node;
	Node *result;

	if (list_length(exprs) == 0)
		return NULL;
	if (list_length(exprs) == 1)
		return (Node *) linitial(exprs);

	result = (Node *) linitial(exprs);
	for(lc = exprs->head->next; lc != NULL; lc = lc->next)
	{
		node = (Node *) lfirst(lc);
		result = (Node *) makeBoolExpr(AND_EXPR, list_make2(node, result));
	}

	return result;
}


/*
 * Returns the target list entries that are used in the group by clause of a
 * query.
 */

List *
getGroupByTLEs (Query *query)
{
	List *result;
	ListCell *lc;
	ListCell *teLc;
	TargetEntry *te;
	GroupClause *groupby;

	result = NIL;

	foreach (lc, query->groupClause)
	{
		groupby = (GroupClause *) lfirst (lc);

		foreach(teLc, query->targetList)
		{
			te = (TargetEntry *) lfirst(teLc);
			if (te->ressortgroupref == groupby->tleSortGroupRef)
			{
				result = lappend(result, te);
			}
		}
	}

	return result;
}


/*
 * Returns a list of groupby expressions from a query.
 */

List *
getGroupByExpr (Query *query)
{
	List *result;
	ListCell *lc;
	TargetEntry *te;

	result = getGroupByTLEs (query);

	foreach(lc,result)
	{
		te = (TargetEntry *) lfirst(lc);

		lc->data.ptr_value = te->expr;
	}

	return result;
}

/*
 *
 */

bool
isUsedInOrderBy (Query *query, TargetEntry *te)
{
	GroupClause *groupClause;
		ListCell *lc;

		foreach(lc, query->sortClause)//TODO extract method to iterate over group or sort clause
		{
			groupClause = (GroupClause *) lfirst(lc);

			if (groupClause->tleSortGroupRef == te->ressortgroupref)
			{
				return true;
			}
		}
		return false;
}

/*
 *  Returns true if the target entry is used in the group by clause of the
 *  query.
 */

bool
isUsedInGroupBy(Query *query, TargetEntry *te)
{
	GroupClause *groupClause;
	ListCell *lc;

	foreach(lc, query->groupClause)
	{
		groupClause = (GroupClause *) lfirst(lc);

		if (groupClause->tleSortGroupRef == te->ressortgroupref)
		{
			return true;
		}
	}
	return false;
}


/*
 *	Creates the joinaliasvars and eref fields for each join RTE from list
 *	subJoins.
 */

void
adaptRTEsForJoins(List *subJoins, Query *query, char *joinRTEname)
{
	ListCell *lc;
	JoinExpr *newJoin;
	RangeTblEntry *lRTE;
	RangeTblEntry *rRTE;
	RangeTblEntry *joinRTE;
	List *vars;
	List *resultVars;
	List *colNames;
	List *resultCols;
	Index joinRTIndex;
	Index lRTIndex;
	Index rRTIndex;

    foreach(lc, subJoins)
	{
		vars = NIL;
		colNames = NIL;

		newJoin = (JoinExpr *) lfirst(lc);
		joinRTIndex = newJoin->rtindex;
		joinRTE = rt_fetch(joinRTIndex, query->rtable);

		/* get RT positions and RTEs for arguments of join */
		if (IsA(newJoin->larg,JoinExpr))
		{
			lRTIndex= ((JoinExpr *) newJoin->larg)->rtindex;
		}
		else {
			lRTIndex = ((RangeTblRef *) newJoin->larg)->rtindex;
		}
		if (IsA(newJoin->rarg,JoinExpr))
		{
			rRTIndex= ((JoinExpr *) newJoin->rarg)->rtindex;
		}
		else {
			rRTIndex = ((RangeTblRef *) newJoin->rarg)->rtindex;
		}

		lRTE = rt_fetch(lRTIndex, query->rtable);
		rRTE = rt_fetch(rRTIndex, query->rtable);

		/* fetch names and vars for arguments of join */
		resultCols = NIL;
		resultVars = NIL;

		expandRTEWithParam(lRTE, lRTIndex, 0, false, false, &colNames, &vars);
		resultCols = list_concat(resultCols, colNames);
		resultVars = list_concat(resultVars, vars);
		expandRTEWithParam(rRTE, rRTIndex, 0, false, false, &colNames, &vars);
		resultCols = list_concat(resultCols, colNames);
		resultVars = list_concat(resultVars, vars);

		/* adapt join RTE */
		joinRTE->joinaliasvars = resultVars;
		joinRTE->eref = makeAlias(joinRTEname, resultCols);
		joinRTE->inFromCl = true;
		joinRTE->inh = false;
		joinRTE->isProvBase = false;
		joinRTE->provAttrs = NIL;
		joinRTE->alias = NULL;
	}
}

/*
 * Recreates all var and colname lists of all Join RTEs of a query.
 */

void
recreateJoinRTEs (Query *query)
{
	ListCell *lc;
	Node *node;

	foreach(lc, query->jointree->fromlist)
	{
		node = (Node *) lfirst(lc);

		if (IsA(node, JoinExpr)) {
			recreateJoinRTE ((JoinExpr *) node, query);
		}
	}
}

/*
 * Recursively creates the join alias and vars for an join expression, by first
 * creating these elements for the args of the join and then for the join
 * itself.
 */

static void
recreateJoinRTE (JoinExpr *join, Query *query)
{
	if (IsA(join->larg, JoinExpr))
		recreateJoinRTE ((JoinExpr *) join->larg, query);
	if (IsA(join->rarg, JoinExpr))
		recreateJoinRTE ((JoinExpr *) join->rarg, query);

	if (join->alias && join->alias->aliasname)
		adaptRTEsForJoins(list_make1(join), query, join->alias->aliasname);
	else
		adaptRTEsForJoins(list_make1(join), query, "newJoin");
}

/*
 *	Create a join expression node and a new range table entry for this join.
 */

JoinExpr *
createJoinExpr (Query *query, JoinType joinType)
{
	JoinExpr *newJoin;
	RangeTblEntry *rte;

	newJoin = makeNode(JoinExpr);
	newJoin->isNatural = false;
	newJoin->rtindex = list_length(query->rtable) + 1;
	newJoin->jointype = joinType;
	newJoin->using = NIL;
	newJoin->alias = NULL;

	rte = makeRte(RTE_JOIN);
	rte->jointype = joinType;
	query->rtable = lappend(query->rtable, rte);

	return newJoin;
}

/*
 * Joins two range table entries of a query on the attributes provide as
 * position list leftAttrs and RightAttrs. If parameter userNotDistinct is
 * true NOT DISTINCT conditions are used to compare the attributes. Otherwise
 * simple equality is used.
 */
//TODO currently only for subquery rtes!
JoinExpr *
createJoinOnAttrs (Query *query, JoinType joinType, Index leftRT,
		Index rightRT, List *leftAttrs, List *rightAttrs, bool useNotDistinct)
{
	JoinExpr *join;
	List *comparisons;
	Var *leftVar;
	Var *rightVar;
	RangeTblRef *rtRef;
	ListCell *leftLc;
	ListCell *rightLc;
	List *leftVars;
	List *rightVars;

	comparisons = NIL;

	/* create join node */
	join = createJoinExpr(query, joinType);

	MAKE_RTREF(rtRef, leftRT);
	join->larg = (Node *) rtRef;
	MAKE_RTREF(rtRef, rightRT);
	join->rarg = (Node *) rtRef;

	/* get left and right vars */
	expandRTE(rt_fetch(leftRT, query->rtable), leftRT, 0, false, NULL,
			&leftVars);
	expandRTE(rt_fetch(rightRT, query->rtable), rightRT, 0, false, NULL,
			&rightVars);

	leftVars = getAllInList(leftVars, leftAttrs);
	rightVars = getAllInList(rightVars, rightAttrs);

	/* */
	forboth(leftLc, leftVars, rightLc, rightVars)
	{
		leftVar = (Var *) lfirst(leftLc);
		rightVar = (Var *) lfirst(rightLc);

		comparisons = lappend(comparisons,
				createNotDistinctConditionForVars(leftVar, rightVar));
	}

	join->quals = (Node *) makeBoolExpr(AND_EXPR,comparisons);

	return join;
}

/*
 * Searches in expression with root "node" for sub expressions from search list
 * and replaces them by expressions from replace list. Flags can be used to
 * determine if the method should recurse into certain node types:
 *
 *				REPLACE_SUB_EXPR_QUERY:			do not recurse into queries
 *				REPLACE_SUB_EXPR_SUBLINK:		do not recurse into sublinks
 *
 */
Node *
replaceSubExpression (Node *node, List *searchList, List *replaceList,
		int flags)
{
	ReplaceSubExpressionsContext *context;
	Node *result;

	/* create context */
	context = (ReplaceSubExpressionsContext *)
			palloc(sizeof(ReplaceSubExpressionsContext));
	context->flags = flags;
	context->searchList = searchList;
	context->replaceList = replaceList;

	result = replaceSubExpressionsMutator(node, context);

	pfree(context);
	return result;
}

/*
 * Walks an expression and replaces sub expressions from the given searchList
 * with the expressions from the replaceList.
 */

static Node *
replaceSubExpressionsMutator (Node *node,
		ReplaceSubExpressionsContext *context)
{
	ListCell *lc;
	Node *searchExpr;
	Index listPos;

	if (node == NULL)
		return node;

	/* check if node is an element from the replace list */
	if (context->flags && REPLACE_CHECK_REPLACERS)
	{
		foreach(lc, context->replaceList)
		{
			searchExpr = (Node *) lfirst(lc);

			if (equal(node, searchExpr))
				return node;
		}
	}

	/* check if the current node is in the search list */
	listPos = 0;
	foreach(lc, context->searchList)
	{
		searchExpr = (Node *) lfirst(lc);

		if (equal(node, searchExpr))
			return (Node *) list_nth(context->replaceList, listPos);

		listPos++;
	}

	/* check for special types and stop recursion if this node types where
	 * deactived by the flags */
	if (IsA(node, Query) && !(context->flags & REPLACE_SUB_EXPR_QUERY))
		return copyObject(node);
	if (IsA(node, SubLink) && !(context->flags & REPLACE_SUB_EXPR_SUBLINK))
		return copyObject(node);

	return expression_tree_mutator(node, replaceSubExpressionsMutator,
			(void *) context);
}

/*
 *	Returns a list of aggregates used in an expression.
 */

List *
getAggrExprs (Node *node)
{
	List *result;

	result = NIL;
	aggrExprWalker (node, &result);

	return result;
}

/*
 * Searches for aggregates in an expression.
 */

static bool
aggrExprWalker (Node *node, List** context)
{
	if (node == NULL)
		return false;

	if (IsA(node, Aggref))
	{
		*context = lappend(*context, node);
		return false;
	}
	return expression_tree_walker(node, aggrExprWalker, (void *) context);
}

/*
 *
 */

Node **
getLinkToFromItem (Query *query, Index rtIndex)
{
	GetFromItemWalkerContext *context;
	ListCell *lc;
	Node *fromItem;

	context = (GetFromItemWalkerContext *)
			palloc(sizeof(GetFromItemWalkerContext));
	context->result = NULL;
	context->rtIndex = rtIndex;
	context->curParent = NULL;

	foreach(lc, query->jointree->fromlist)
	{
		fromItem = (Node *) lfirst(lc);

		if (IsA(fromItem, JoinExpr))
		{
			context->curParent = (Node **) &(lc->data.ptr_value);
			fromItemWalker (fromItem, context);

			if (context->result)
				return context->result;
		}
	}

	return context->result;
}

/*
 *
 */

static void
fromItemWalker (Node *node, GetFromItemWalkerContext *context)
{
	if (node == NULL)
		return;

	if (IsA(node, RangeTblRef))
	{
		RangeTblRef *rtRef;

		rtRef = (RangeTblRef *) node;
		if (rtRef->rtindex == context->rtIndex)
		{
			context->result = context->curParent;
		}
	}

	if (IsA(node, JoinExpr))
	{
		JoinExpr *join;

		join = (JoinExpr *) node;

		if (join->rtindex == context->rtIndex)
		{
			context->result = context->curParent;
		}
		else {
			context->curParent = &(join->larg);
			fromItemWalker (join->larg, context);

			context->curParent = &(join->rarg);
			fromItemWalker (join->rarg, context);
		}
	}
}

/*
 * If node is the qual of an JoinExpr of query, then return this JoinExpr,
 * otherwise return NULL.
 */

Node **
getJoinForJoinQual (Query *query, Node *node)
{
	JoinQualWalkerContext *context;
	ListCell *lc;
	Node *fromItem;

	context = (JoinQualWalkerContext *) palloc(sizeof(JoinQualWalkerContext));
	context->result = NULL;
	context->expr = node;
	context->curParent = NULL;

	foreach(lc, query->jointree->fromlist)
	{
		fromItem = (Node *) lfirst(lc);

		if (IsA(fromItem, JoinExpr))
		{
			context->curParent = (Node **) &(lc->data.ptr_value);
			getJoinForJoinQualWalker (fromItem, context);

			if (context->result)
				return context->result;
		}
	}

	return context->result;
}

/*
 *
 */

static void
getJoinForJoinQualWalker (Node *node, JoinQualWalkerContext *context)
{
	if (node == NULL)
		return;

	if (IsA(node, JoinExpr))
	{
		JoinExpr *join;

		join = (JoinExpr *) node;

		if (equal(join->quals, context->expr))
		{
			context->result = context->curParent;
		}
		else {
			context->curParent = &(join->larg);
			getJoinForJoinQualWalker (join->larg, context);

			context->curParent = &(join->rarg);
			getJoinForJoinQualWalker (join->rarg, context);
		}
	}
}

/*
 * Searches for an subexpression in an expression.
 */
Node *
findSubExpression (Node *expr, Node *subExpr, int flags)
{
	FindSubExpressionWalkerContext *context;

	context = (FindSubExpressionWalkerContext *)
			palloc(sizeof(FindSubExpressionWalkerContext));
	context->searchExpr = expr;
	context->result = NULL;

	findSubExpressionWalker(expr, context);

	return context->result;
}

/*
 *
 */

static bool
findSubExpressionWalker (Node *node, FindSubExpressionWalkerContext *context)
{
	if (node == NULL)
		return false;

	if (equal(node, context->searchExpr))
	{
		context->result = node;
		return true;
	}

	if (IsA(node, Query))
	{
		Query *query;

		query = (Query *) node;

		return query_tree_walker(query, findSubExpressionWalker, (void *) context, 0);
	}

	return expression_tree_walker(node, findSubExpressionWalker, (void *) context);
}


/*
 *	Removes the ProvInfo nodes from a query (for all query nodes of the query).
 */

void
removeProvInfoNodes (Query *query)
{
	removeProvInfoNodesWalker ((Node *) query, NULL);
}

/*
 *
 */

static bool
removeProvInfoNodesWalker (Node *node, void *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, Query))
	{
		Query *query;

		query = (Query *) node;
		query->provInfo = NULL;

		return query_tree_walker(query, removeProvInfoNodesWalker,
				(void *) context, 0);
	}
	if (IsA(node, SubLink))
	{
		SubLink *subLink;

		subLink = (SubLink *) node;

		//CHECK
 	}

	return expression_tree_walker(node, removeProvInfoNodesWalker,
			(void *) context);
}

/*
 *
 */

bool
queryHasRewriteChildren (Query *query)
{
	bool result = false;

	queryHasRewriteChildrenWalker((Node *) query, &result);

	return result;
}

/*
 *
 */

static bool
queryHasRewriteChildrenWalker (Node *node, bool *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, Query))
	{
		Query *query;

		query = (Query *) node;

		if (IsProvRewrite(query)) {
			*context = true;
			return true;
		}

		return query_tree_walker(query, queryHasRewriteChildrenWalker,
				(void *) context, 0);
	}
	if (IsA(node, SubLink))
	{
		SubLink *subLink;

		subLink = (SubLink *) node;

		//CHECK
 	}

	return expression_tree_walker(node, queryHasRewriteChildrenWalker,
			(void *) context);
}

/*
 * Checks if a query has any provenance queries as children.
 */

bool
hasProvenanceSubquery (Query *query)
{
	bool result;
	ListCell *lc;
	RangeTblEntry *rte;

	if (IsProvRewrite(query))
		return true;

	result = false;

	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		if (rte->rtekind == RTE_SUBQUERY)
			result = result || hasProvenanceSubquery (rte->subquery);
	}

	return result;
}

/*
 *	Create a query node that encapsulates a base relation range table entry.
 */

Query *
generateQueryFromBaseRelation (RangeTblEntry *rte)
{
	Query *result;
	List *vars;
	List *names;
	ListCell *varLc;
	ListCell *nameLc;
	Var *var;
	Value *name;
	TargetEntry *te;
	Index resno;
	RangeTblRef *rtRef;

	result = makeQuery();
	result->rtable = list_make1 (rte);

	expandRTEWithParam(rte,1,0,false,false, &names, &vars);

	/* create target list */
	resno = 1;
	forboth(varLc, vars, nameLc, names)
	{
		var = (Var *) lfirst(varLc);
		name = (Value *) lfirst(nameLc);

		te = makeTargetEntry((Expr *) var, resno, pstrdup(name->val.str),
				false);
		result->targetList = lappend(result->targetList, te);

		resno++;
	}

	/* create join tree */
	rtRef = makeNode (RangeTblRef);
	rtRef->rtindex = 1;
	result->jointree->fromlist = list_make1(rtRef);

	return result;
}

/*
 *
 */

RangeTblEntry *
generateQueryRTEFromRelRTE (RangeTblEntry *rte)
{
	Query *newSub;
	RangeTblEntry *newRte;

	newSub = generateQueryFromBaseRelation(rte);

	newRte = copyObject(rte);
	newRte->rtekind = RTE_SUBQUERY;
	newRte->subquery = newSub;
	newRte->relid = InvalidOid;

	return newRte;
}

/*
 * Returns the join tree node that references rtindex.
 */

Node *
getJoinTreeNode (Query *query, Index rtindex)
{
	ListCell *lc;
	Node *node;
	Node *result;

	foreach(lc, query->jointree->fromlist)
	{
		node = (Node *) lfirst(lc);

		result = getJoinTreeNodeWalker (node, rtindex);

		if (result != NULL)
			return result;
	}

	return NULL;
}

/*
 *
 */

static Node *
getJoinTreeNodeWalker (Node *node, Index rtindex)
{
	Node *result;

	if (IsA(node, RangeTblRef))
	{
		RangeTblRef *rtRef;

		rtRef = (RangeTblRef *) node;

		if (rtRef->rtindex == rtindex)
			return node;
	}
	else if (IsA(node, JoinExpr))
	{
		JoinExpr *join;

		join = (JoinExpr *) node;
		if (join->rtindex == rtindex)
			return node;

		result = getJoinTreeNodeWalker(join->larg, rtindex);
		if (result != NULL)
			return result;

		result = getJoinTreeNodeWalker(join->rarg, rtindex);
		if (result != NULL)
			return result;
	}

	return NULL;
}

/*
 *	Returns true if a node is a Var or Const (surrounding casts are ok).
 */

bool
isVarOrConstWithCast (Node *node)
{
	return (getCastedVarOrConst(node) != NULL);
}

/*
 * if expr is a VAr or Const surrounded by casts return the Var or Const.
 * Return NULL else.
 */

static Node *
getCastedVarOrConst (Node *node)
{
	FuncExpr *funcExpr;
	ListCell *lc;
	Node *arg;
	Node *newNode;

	if (IsA(node, Var))
		return node;

	if (IsA(node, Const))
		return node;

	else if (IsA(node, FuncExpr))
	{
		funcExpr = (FuncExpr *) node;

		if (funcExpr->funcformat == COERCE_EXPLICIT_CALL)//CHECK does this really define if a function is cast or not
			return NULL;

		foreach(lc,funcExpr->args)
		{
			arg = (Node *) lfirst(lc);

			newNode = getCastedVarOrConst(arg);

			if (newNode == NULL)
				return NULL;
		}

		return newNode;
	}
	return NULL;
}


/*
 * Checks if a node is an inequality OpExpr.
 */

bool
isInequality (Node *node)
{
	ComparisonType type;

	type = getComparisonType(node);

	if (type == COMP_NOCOMP || type == COMP_EQUAL)
		return false;

	return true;
}

/*
 *
 */

ComparisonType
getComparisonType (Node *node)
{
	OpExpr *op;
	Oid argType;
	Operator	optup;
	Oid			opoid;

	/* not an op -> not a comparison */
	if (!IsA(node,OpExpr))
		return COMP_NOCOMP;

	/* get argument type and check if opid is represents an inequality */
	op = (OpExpr *) node;
	argType = exprType((Node *) linitial(op->args));

	/* is smaller than ? */
	if (op->opno == ordering_oper_opid(argType))
		return COMP_SMALLER;

	/* is bigger than ? */
	if (op->opno == reverse_ordering_oper_opid(argType))
		return COMP_BIGGER;

	/* is smaller eq than ? */
	if (op->opno == compatible_oper_opid(list_make1(makeString("<=")), argType,
			argType, false))
		return COMP_SMALLEREQ;

	/* is bigger eq than ? */
	if (op->opno == compatible_oper_opid(list_make1(makeString(">=")), argType,
			argType, false))
		return COMP_BIGGEREQ;

	/* is equality? */
	optup = equality_oper(argType, false);
	opoid = oprid(optup);
	ReleaseSysCache(optup);

	if (op->opno == opoid)
		return COMP_EQUAL;

	/* is no comparison operator */
	return COMP_NOCOMP;
}

/*
 * Checks if an expr is constant (no vars, no aggs, no sublinks, no volatile
 * and immutable functions, no params)
 */

bool
isConstExpr (Node *node)
{
	Node *varOrConst;

	varOrConst = getCastedVarOrConst(node);

	if (varOrConst == NULL)
		return false;

	return (IsA(varOrConst, Const));
}

/*
 * If var is used as the expr field of an traget list entry from targetList,
 * return true. Else return false.
 */

TargetEntry *
findTeForVar (Var *var, List *targetList)
{
	ListCell *lc;
	TargetEntry *te;

	foreach(lc, targetList)
	{
		te = (TargetEntry *) lfirst(lc);

		if(equal(var, te->expr))
			return te;
	}

	return NULL;
}


/*
 * Returns true, if the top level query block contains outer joins.
 */

bool
hasOuterJoins (Query *query)
{
	bool result;
	ListCell *lc;
	Node *fromItem;

	result = false;

	foreach(lc, query->jointree->fromlist)
	{
		fromItem = (Node *) lfirst(lc);

		result = result || containsOuterJoins(fromItem);
	}

	return result;
}

/*
 *
 */

static bool
containsOuterJoins (Node *node)
{
	JoinExpr *join;

	/* is a range table reference return false */
	if(!IsA(node, JoinExpr))
		return false;

	join = (JoinExpr *) node;

	/* join itself is an outer join. Return true */
	switch(join->jointype)
	{
	case JOIN_LEFT:
	case JOIN_RIGHT:
	case JOIN_FULL:
		return true;
	default:
		break;
	}

	/* check if any of the children are outer joins */
	if(containsOuterJoins(join->larg))
		return true;
	if(containsOuterJoins(join->rarg))
		return true;

	return false;
}

/*
 * If the TargetEntry is a single Var possible surrounded by casts then return
 * this Var. Return NULL otherwise.
 */

Var *
getVarFromTeIfSimple (Node *node)
{
	FuncExpr *funcExpr;
	ListCell *lc;
	Node *arg;
	Var *var;

	if (IsA(node, Var)) {
		var = (Var *) node;

		return var;
	}
	else if (IsA(node, FuncExpr))
	{
		funcExpr = (FuncExpr *) node;

		if (funcExpr->funcformat == COERCE_EXPLICIT_CALL)//CHECK does this really define if a function is cast or not
			return NULL;

		if (list_length(funcExpr->args) != 1)
			return NULL;

		foreach(lc,funcExpr->args)
		{
			arg = (Node *) lfirst(lc);

			return getVarFromTeIfSimple(arg);
		}
	}
	return NULL;
}


/*
 * returns the alias for an rte, if no alias exists the eref is return, if no eref exists '' is returned
 */

char *
getAlias (RangeTblEntry *rte)
{
	if (rte->alias && rte->alias->aliasname)
		return rte->alias->aliasname;
	else if (rte->eref && rte->eref->aliasname)
		return rte->eref->aliasname;

	return "";
}

/*
 *
 */

Var *
resolveToRteVar (Var *var, Query *query)
{
	RangeTblEntry *rte;
	List *vars;
	Var *newVar;

	vars = NIL;
	rte = rt_fetch(var->varno, query->rtable);

	if (rte->rtekind != RTE_JOIN)
		return var;

	newVar = (Var *) list_nth(rte->joinaliasvars, var->varattno - 1);

	return resolveToRteVar (newVar, query);
}
