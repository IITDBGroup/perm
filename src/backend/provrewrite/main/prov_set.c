/*-------------------------------------------------------------------------
 *
 * prov_set.c
 *	  PERM C - I-CS provenance rewrites for set queries.
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/prov_set.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
 *
 * NOTES
 *	Set operation rewrite generates a new top query node that joins the original set operation results
 *	with the rewritten inputs of the set operations (see ICDE'09 paper for explanation of the algebraic
 *	rewrite rules that determine this rewrite).
 *
 *-------------------------------------------------------------------------
 */

/*TODO the whole join generation should make use of the helper method
 * instead of reproducing similar functionality here.
 */

#include "postgres.h"
#include "catalog/pg_operator.h"		// pg_operator system table for operator lookup
#include "nodes/makefuncs.h"			// needed to create new nodes
#include "nodes/print.h"				// pretty print node (trees)
#include "optimizer/clauses.h"
#include "parser/parse_expr.h"			// expression transformation used for expression type calculation
#include "parser/parse_oper.h"			// for lookup of operators
#include "parser/parsetree.h"
#include "parser/parse_relation.h"
#include "utils/guc.h"
#include "utils/syscache.h"				// used to release heap tuple references
#include "provrewrite/provlog.h"
#include "provrewrite/provattrname.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_sublink_util_analyze.h"
#include "provrewrite/prov_sublink_util_mutate.h"
#include "provrewrite/prov_sublink_util_search.h"
#include "provrewrite/prov_spj.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_set.h"


/* Function declarations */
static void addSetSubqueryRTEs (Query *top, Query *orig);
static void createNullSetDiffProvAttrs(Query *newTop, Query *query,
		List **pList);
static void rewriteUnionWithWLCS (Query *query);
static void addDummyProvAttrs (RangeTblEntry *rte, List *subProv, int pos);
static void adaptSetProvenanceAttrs (Query *query);
static void adaptSetStmtCols (SetOperationStmt *stmt, List *colTypes,
		List *colTypmods);
static void replaceSetOperationSubTrees (Query *query, Node *node,
		Node **parentPointer, SetOperation rootType);
static void replaceSetOperatorSubtree (Query *query, SetOperationStmt *setOp,
		Node **parent);
static void rewriteSetRTEs (Query *newTop);
static void makeRTEVars (RangeTblEntry *rte, RangeTblEntry *newRte,
		Index rtindex);
static void createJoinsForSetOp (Query *query, SetOperation setType);
static void createSetDiffJoin (Query *query);
static void createRTEforJoin (Query *query, JoinExpr *join, Index leftIndex,
		Index rightIndex);
static Node *createEqualityCondition (List* leftAttrs, List* rightAttrs,
		Index leftIndex, Index rightIndex, BoolExprType boolOp, bool neq);
static List *getAttributesForJoin (RangeTblEntry *rte);
static bool substractRangeTblRefValuesWalker (Node *node, void *context);

/*
 *	Rewrites a set operation query node . Depending on the type of set
 *	operations this query node represents, it can be rewritten in one pass or
 *	has to be restructured into several query node before applying the
 *	rewrites. E.g., if the set operations are only union no restructuring is
 *	necessary (this is derived from the fact that applying the algebraic
 *	rewrite rules on which derived Perm is based to a list of unions results
 *	in a fixed form). See ICDE'09 paper for details.
 */

Query *
rewriteSetQuery (Query * query)
{
	Query *newTop;
	List *pList;
	int numSubs;
	int *context;
	Query *orig;

	pList = NIL;
	orig = copyObject(query);

	/* remove dummy RTEs produced by postgres view expansion */
	removeDummyRewriterRTEs(query);

	/* restructure set operation tree if necessary */
	replaceSetOperationSubTrees (query, query->setOperations,
			&(query->setOperations),
			((SetOperationStmt *) query->setOperations)->op);

	/* check if the alternative union semantics is activated and we are
	 * rewriting a union node. If so use all the join stuff falls apart and we
	 * just have to rewrite the original query */
	if (prov_use_wl_union_semantics
			&& ((SetOperationStmt *) query->setOperations)->op == SETOP_UNION)
	{
		rewriteUnionWithWLCS (query);

		return query;
	}

	/* create new top query node */
	newTop = makeQuery();
	addSetSubqueryRTEs(newTop, query);
	newTop->targetList = copyObject(query->targetList);
	newTop->intoClause = copyObject(query->intoClause);
	SetSublinkRewritten(newTop, true);

	/* add original query as first range table entry */
	addSubqueryToRTWithParam (newTop, orig, "originalSet", false, ACL_NO_RIGHTS, false);

	/* rewrite the subqueries used in the set operation */
	rewriteSetRTEs (newTop);
	numSubs = list_length(newTop->rtable) - 1;

	LOGNODE(newTop, "replaced different set ops and rewrite sub queries");

	/* correct alias of rewritten subqueries */
	correctSubQueryAlias (newTop);

	/* join the original query with all rewritten subqueries */
	createJoinsForSetOp (newTop, ((SetOperationStmt *) query->setOperations)->op);

	/* add provenance attributes from the subqueries to the top query target list */
	pList = addProvenanceAttrsForRange (newTop, 2, 2 + numSubs, pList);
	if (prov_use_wl_union_semantics && ((SetOperationStmt *)
			query->setOperations)->op == SETOP_EXCEPT)
		createNullSetDiffProvAttrs(newTop, query, &pList);
	push(&pStack, pList);

	/* increase sublevelsup of vars in case we are rewriting in an sublink query */
	context = (int *) palloc(sizeof(int));
	*context = -1;
	increaseSublevelsUpMutator((Node *) orig, context);
	pfree(context);

	return newTop;
}

/*
 *
 */

static void
addSetSubqueryRTEs (Query *top, Query *orig)
{
	SetOperationStmt *setOp = (SetOperationStmt *) orig->setOperations;
	List *rte = NIL;

	if (!prov_use_wl_union_semantics || setOp->op != SETOP_EXCEPT)
	{
		top->rtable = copyObject(orig->rtable);
		return;
	}

	findSetOpRTEs(orig->rtable, (Node *) setOp->larg, &rte, NULL);
	top->rtable = copyObject(rte);
}


/*
 *
 */

static void
createNullSetDiffProvAttrs(Query *newTop, Query *query, List **pList)
{
	List *rtes = NIL;
	List *provRtes = NIL;
	List *attrNames;
	List *vars;
	Node *rightSet;
	ListCell *lc;
	ListCell *innerLc;
	ListCell *attrLc;
	ListCell *varLc;
	RangeTblEntry *rte;
	RangeTblEntry *provRte;
	TargetEntry *newTe;
	Node *nullConst;
	Value *colName;
	Var *var;
	char *provName;
	Index curResno = list_length(newTop->targetList);

	/* get all base relations under right input of set difference and
	 * generate NULL provenance attributes for them.
	 */
	rightSet = ((SetOperationStmt *) query->setOperations)->rarg;
	findSetOpRTEs(query->rtable, rightSet, &rtes, NULL);

	/* for each rte accessed by the subtree under the right input get the
	 * base relations accessed by this rte and create null constants as
	 * provenance attributes for them. */
	foreach(lc, rtes)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		provRtes = NIL;

		findBaseRelationsForProvenanceRTE(rte, &provRtes);

		foreach(innerLc, provRtes)
		{
			provRte = (RangeTblEntry *) lfirst(innerLc);
			attrNames = NIL;
			vars = NIL;

			/* check if is an RTE with specified provenance attributes */
			if (provRte->provAttrs != NIL)
			{
				List *tes;

				rewriteRTEwithProvenance(1, provRte);
				tes = pop(&pStack);

				foreach(attrLc, tes)
				{
					newTe = (TargetEntry *) lfirst(attrLc);
					var = (Var *) newTe->expr;

					newTe->resno = ++curResno;
					newTe->expr = (Expr *) makeNullConst(var->vartype,
							var->vartypmod);
					newTop->targetList = lappend(newTop->targetList, newTe);
					*pList = lappend(*pList, newTe);
				}
			}
			else
			{
				/* increment rel ref counter */
				getQueryRefNum(provRte, true);

				/* get RTE attributes */
				expandRTEWithParam(provRte, 1, 0, false, false, &attrNames, &vars);

				/* generate provenance attributes */
				forboth(attrLc, attrNames, varLc, vars)
				{
					colName = (Value *) lfirst(attrLc);
					var = (Var *) lfirst(varLc);

					nullConst = (Node *) makeNullConst(var->vartype,
							var->vartypmod);
					provName = createProvAttrName(provRte, strVal(colName));

					newTe = makeTargetEntry((Expr *) nullConst, ++curResno,
							provName, false);
					newTop->targetList = lappend(newTop->targetList, newTe);
					*pList = lappend(*pList, newTe);
				}
			}
		}
	}
}

/*
 * Removes any superficial RTEs produced by postgres rewriter (view expansion), because this RTE's mess up the set rewrite.
 * CHECK if these things aren't needed by the optimizer or SQL reconstructor
 * TODO do this for the whole query before provenance rewrite?
 */

void
removeDummyRewriterRTEs (Query *query)
{
	RangeTblEntry *rte;
	ListCell *lc;
	TargetEntry *te;
	Var *var;

	/* check if first range table entry is a dummy entry */
	rte = (RangeTblEntry *) linitial (query->rtable);
	if (strcmp(rte->alias->aliasname, "*OLD*") == 0)
	{
		/* remove RTEs from range table */
		pfree(rte);

		rte = (RangeTblEntry *) lsecond (query->rtable);
		pfree(rte);

		lc = query->rtable->head;
		query->rtable->head = query->rtable->head->next->next;
		query->rtable->length = query->rtable->length - 2;

		pfree(lc->next);
		pfree(lc);

		/* adapt Target list */
		foreach(lc, query->targetList)
		{
			te = (TargetEntry *) lfirst(lc);
			var = (Var *) te->expr;
			var->varno -= 2;
		}

		/* adapt set operation tree */
		substractRangeTblRefValuesWalker (query->setOperations, NULL);
	}
}

/*
 *
 */

static void
rewriteUnionWithWLCS (Query *query)
{
	List *subProvs = NIL;
	List *pList = NIL;
	RangeTblEntry *rte;
	ListCell *lc;
	int i;

	// rewrite the range table entries and fetch their provenance attrs
	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		rte->subquery = rewriteQueryNode(rte->subquery);
		subProvs = lappend(subProvs, linitial(pStack));
	}

	// add projections on NULL to each subquery to generate
	// the same schema for each subquery.
	foreachi(lc, i, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		addDummyProvAttrs(rte, subProvs, i);
	}

	correctSubQueryAlias(query);

	// push provenance list on stack and add provenance attrs to query
	pList = addProvenanceAttrsForRange(query, 1, list_length(query->rtable) + 1, pList);
	push(&pStack, pList);

	// adapt set operation query provenance attributes
	adaptSetProvenanceAttrs(query);
}

/*
 *
 */

static void
addDummyProvAttrs (RangeTblEntry *rte, List *subProv, int pos)
{
	List *result = NIL;
	Query *query;
	int i;
	ListCell *lc;
	ListCell *innerLc;
	List *curProvList;
	List *ownProv;
	TargetEntry *newTe;
	TargetEntry *te;
	Expr *newExpr;
	int numSelfProvAttrs;
	int curResno;

	query = rte->subquery;

	// get number of provenance attributes for the rte
	// and remove the provenance attributes from target list
	curProvList = (List *) list_nth(subProv, pos);
	numSelfProvAttrs = list_length(curProvList);
	ownProv = list_copy_tail(query->targetList,
							list_length(query->targetList) - numSelfProvAttrs);
	removeAfterPos(query->targetList, list_length(query->targetList) - numSelfProvAttrs);
	result = query->targetList;
	curResno = list_length(result) + 1;

	foreachi(lc,i,subProv)
	{
		curProvList = (List *) lfirst(lc);

		// for own provenance use the original provenance attrs
		if (i == pos)
		{
			foreach(innerLc, ownProv)
			{
				te = (TargetEntry *) lfirst(innerLc);
				te->resno = curResno++;
				result = lappend(result, te);
			}
		}
		// create null constants for provenance of other subqueries
		else
		{
			foreach(innerLc, curProvList)
			{
				te = (TargetEntry *) lfirst(innerLc);

				newExpr = (Expr *) makeNullConst (exprType ((Node *) te->expr),
						exprTypmod ((Node *) te->expr));
				newTe = makeTargetEntry(newExpr, curResno++, te->resname, false);

				result = lappend(result, newTe);
			}
		}
	}

	query->targetList = result;

	list_free(ownProv);
}


/*
 *
 */

static void
adaptSetProvenanceAttrs (Query *query)
{
	TargetEntry *te;
	Var *var;
	ListCell *lc;
	int curAttno = 1;
	List *colTypes = NIL;
	List *colTypmods = NIL;

	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);
		var = (Var *) te->expr;
		var->varno = 1;
		var->varattno = curAttno++;
		colTypes = lappend_oid(colTypes, var->vartype);
		colTypmods = lappend_int(colTypmods, var->vartypmod);
	}

	adaptSetStmtCols((SetOperationStmt *) query->setOperations,
			colTypes, colTypmods);
}

/*
 *
 */

static void
adaptSetStmtCols (SetOperationStmt *stmt, List *colTypes, List *colTypmods)
{
	stmt->colTypes = colTypes;
	stmt->colTypmods = colTypmods;
	stmt->all = true;
	if (IsA(stmt->larg, SetOperationStmt))
		adaptSetStmtCols ((SetOperationStmt *) stmt->larg, colTypes, colTypmods);
	if (IsA(stmt->rarg, SetOperationStmt))
		adaptSetStmtCols ((SetOperationStmt *) stmt->rarg, colTypes, colTypmods);
}

/*
 *	Walks a join tree and substract two from every RangeTblRef rtindex.
 *	Needed by the rewrite that changes the structure of the query.
 */

static bool
substractRangeTblRefValuesWalker (Node *node, void *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, RangeTblRef))
	{
		RangeTblRef *rtRef;

		rtRef = (RangeTblRef *) node;
		rtRef->rtindex -= 2;
	}

	return expression_tree_walker(node, substractRangeTblRefValuesWalker,
			context);
}



/*
 * Rewrites the range table entries of a set query. Basically loops over each
 * RTE entries leaving out the first one which is the original set operation
 * query.
 */

static void
rewriteSetRTEs (Query *newTop)
{
	ListCell *lc;
	RangeTblEntry *rte;
	Query *query;

	for(lc = newTop->rtable->head->next; lc != NULL; lc = lc->next)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		query = rewriteQueryNode (rte->subquery);
		rte->subquery = query;
	}
}

/*
 * Create the joins for a rewritten set operation node. We have to join the
 * original query (first RTE of the new top query node introduced by the
 * rewrite rules) with each rewritten input of the set operation (all the other
 * range table entries of the new top node).
 */

static void
createJoinsForSetOp (Query *query, SetOperation setType)
{
	RangeTblRef *rtRef;
	Node *curJoin;
	JoinExpr *newJoin;
	JoinType joinType;
	int i, rtableLength, origRtableLength;

	/* define join type to use in join expression */
	switch (setType)
	{
		case SETOP_UNION:
			joinType = JOIN_LEFT;
		break;
		case SETOP_INTERSECT:
			joinType = JOIN_INNER;
		break;
		/* set difference is a special case with two types of join ops but only
		 * two subqueries */
		case SETOP_EXCEPT:
			createSetDiffJoin (query);
			return;
		default:
			elog(ERROR,
					"Unkown set operation type: %d",
					setType);
		break;
	}

	/* create the leaf join */
	origRtableLength = list_length(query->rtable);
	rtableLength = list_length(query->rtable);

	newJoin = (JoinExpr *) makeNode(JoinExpr);
	newJoin->rtindex = rtableLength + 1;
	newJoin->jointype = joinType;

	MAKE_RTREF(rtRef, 1);
	newJoin->larg = (Node *) rtRef;

	MAKE_RTREF(rtRef, 2);
	newJoin->rarg = (Node *) rtRef;

	createRTEforJoin(query, newJoin, 0, 1);
	createSetJoinCondition (query, newJoin, 0, 1, false);
	curJoin = (Node *) newJoin;


	/* add a join for each range table entry */
	for (i = 2; i < origRtableLength ;i++)
	{
		/* create a new join expression and add join expression created in last step
		 * as right child. Left child is the current range table entry
		 */
		newJoin = (JoinExpr *) makeNode(JoinExpr);
		newJoin->jointype = joinType;
		newJoin->rtindex = rtableLength + 2;

		newJoin->larg = curJoin;

		MAKE_RTREF(rtRef,i + 1);
		newJoin->rarg = (Node *) rtRef;

		/* create join range table entry */
		createRTEforJoin(query, newJoin, rtableLength, i);

		/* create join condition */
		createSetJoinCondition (query, newJoin, rtableLength, i, false);

		/* create join range table entry */
		curJoin = (Node *) newJoin;
		rtableLength++;
	}

	/* set top level join as jointree of top query */
	query->jointree->fromlist = list_make1(curJoin);
}

/*
 * Creates the joins for a rewritten set difference operation. The algebraic
 * rewrite rules dictate that each set diff operation has to be placed on its
 * own in a query node. Therefore, we only have to join the original query
 * (first range table entry) with its two rewritten inputs.
 */

static void
createSetDiffJoin (Query *query)
{
	JoinExpr *innerJoin;
	JoinExpr *outerJoin;
	RangeTblRef *rtRef;
	bool all;
	Query *orig;

	orig = ((RangeTblEntry *) linitial(query->rtable))->subquery;
	all = ((SetOperationStmt *) orig->setOperations)->all;

	/* create inner join node */
	innerJoin = (JoinExpr *) makeNode(JoinExpr);
	innerJoin->rtindex = list_length(query->rtable) + 1;
	innerJoin->jointype = JOIN_INNER;

	MAKE_RTREF(rtRef, 1);
	innerJoin->larg = (Node *) rtRef;

	MAKE_RTREF(rtRef, 2);
	innerJoin->rarg = (Node *) rtRef;

	createRTEforJoin(query, innerJoin, 0, 1);
	createSetJoinCondition (query, innerJoin, 0, 1, false);

	if (prov_use_wl_union_semantics)
	{
		query->jointree->fromlist = list_make1(innerJoin);
		return;
	}

	/* create outer join node */
	outerJoin = (JoinExpr *) makeNode(JoinExpr);
	outerJoin->rtindex = 5;
	outerJoin->jointype = JOIN_LEFT;

	outerJoin->larg = (Node *) innerJoin;

	MAKE_RTREF(rtRef, 3);
	outerJoin->rarg = (Node *) rtRef;

	createRTEforJoin(query, outerJoin, 3, 2);
	if (all)
		createSetJoinCondition (query, outerJoin, 3, 2, true);
	else
		outerJoin->quals =  (Node *) makeBoolConst(true, false);

	/* set top level join as jointree of top query */
	query->jointree->fromlist = list_make1(outerJoin);
}

/*
 * Creates the condition to join the original set operation result with
 * one of its rewritten input on equality (or inequality).
 */

void
createSetJoinCondition (Query *query, JoinExpr *join, Index leftIndex, Index rightIndex, bool neq)
{
	List *leftAttrs;
	List *rightAttrs;
	RangeTblEntry *rte;
	Index origTlength;

	origTlength = list_length(query->targetList);

	rte = rt_fetch(leftIndex + 1, query->rtable);
	leftAttrs = getAttributesForJoin(rte);
	leftAttrs = list_truncate (leftAttrs, origTlength);

	rte = rt_fetch(rightIndex + 1, query->rtable);
	rightAttrs = getAttributesForJoin(rte);
	rightAttrs = list_truncate (rightAttrs, origTlength);

	join->quals = createEqualityCondition(leftAttrs, rightAttrs, leftIndex, rightIndex, AND_EXPR, neq);
}

/*
 * Helper method that generates a list of target entries for the join
 * condition created by createSetJoinCondition.
 */

static List *
getAttributesForJoin (RangeTblEntry *rte)
{
	List *result;
	TargetEntry *te;
	ListCell *varLc;
	ListCell *nameLc;
	Value *name;
	Var *var;
	Index curAttno;

	if (rte->rtekind == RTE_SUBQUERY)
		result = copyObject(rte->subquery->targetList);
	else
	{
		result = NIL;

		curAttno = 1;
		forboth(varLc, rte->joinaliasvars, nameLc, rte->eref->colnames)
		{
			var = (Var *) lfirst(varLc);
			name = (Value *) lfirst(nameLc);

			te = makeTargetEntry((Expr *) var,
					curAttno,
					name->val.str,
					false);

			result = lappend(result, te);
			curAttno++;
		}
	}
	return result;
}

/*
 * Creates an range table for a new join introduced by the set rewrite rules.
 */

static void
createRTEforJoin (Query *query, JoinExpr *join, Index leftIndex, Index rightIndex)
{
	RangeTblEntry *newRte;
	RangeTblEntry *rte;

	/* create new RTE */
	newRte = makeRte (RTE_JOIN);
	newRte->jointype = join->jointype;
	newRte->alias->aliasname = "LEFT_JOIN_RIGHT";
	newRte->eref->aliasname = "LEFT_JOIN_RIGHT";

	/* add join var aliases for left */
	rte = (RangeTblEntry *) list_nth(query->rtable, leftIndex);
	makeRTEVars (rte, newRte, leftIndex);

	/* add join var aliases for right */
	rte = (RangeTblEntry *) list_nth(query->rtable, rightIndex);
	makeRTEVars (rte, newRte, rightIndex);

	/* append join rte to range table */
	query->rtable = lappend(query->rtable, newRte);
}

/*
 * Create a list of vars for a RTE. Helper method used by createRTEforJoin to
 * generate the joinaliasvars.
 */

static void
makeRTEVars (RangeTblEntry *rte, RangeTblEntry *newRte, Index rtindex)
{
	ListCell *lc;
	ListCell *nameLc;
	TargetEntry *te;
	Var *var;
	Value *name;
	Alias *alias;
	Index curAttno;
	int counter;

	alias = newRte->eref;
	/* is subquery RTE */
	if (rte->rtekind == RTE_SUBQUERY) {
		counter = 1;
		foreach (lc, rte->subquery->targetList)
		{
			te = (TargetEntry *) lfirst(lc);
			var = makeVar (rtindex + 1,
					te->resno,
					exprType ((Node *) te->expr),
					exprTypmod ((Node *) te->expr),
					0);
			newRte->joinaliasvars = lappend(newRte->joinaliasvars, var);
			alias->colnames =  lappend(alias->colnames, makeString(te->resname));
			counter++;
		}
	}
	/* is join RTE */
	else {
		curAttno = 1;
		forboth(lc, rte->joinaliasvars, nameLc, rte->eref->colnames)
		{
			var = (Var *) lfirst(lc);
			name = (Value *) lfirst(nameLc);
			var = makeVar (rtindex + 1,
						curAttno,
						var->vartype,
						var->vartypmod,
						0);
			newRte->joinaliasvars = lappend(newRte->joinaliasvars, var);
			alias->colnames =  lappend(alias->colnames, copyObject(name));
			curAttno++;
		}
	}
}


/*
 * Walks through a set operation tree of a query. If only one type of operators
 * (union or intersection) is found nothing is done. If a different operator or
 * a set difference operator is found, the whole subtree under this operator is extracted into
 * a new query node. E.g.:
 * 		SELECT * FROM r UNION (SELECT * FROM s UNION SELECT * FROM t); would be left unchanged.
 * 		SELECT * FROM r UNION (SELECT * FROM s INTERSECT SELECT * FROM t); would be changed into
 * 		SELECT * FROM r UNION (SELECT * FROM (SELECT * FROM s INTERSECT SELECT * FROM t)) AS sub;
 *
 * If the GUC parameter prov_use_set_optimization is not set, then each set operator is placed in
 * its own query node regardless of its type.
 */

static void
replaceSetOperationSubTrees (Query *query, Node *node, Node **parentPointer, SetOperation rootType)
{
	SetOperationStmt *setOp;

	/* RangeTblRef are ignored */
	if (!IsA(node,SetOperationStmt))
		return;

	/* cast to set operation stmt */
	setOp = (SetOperationStmt *) node;

	/* if the user deactivated the optimized set operation rewrites we outsource each set operation into
	 * a separate query.
	 */
	if (!prov_use_set_optimization)
	{
		if (IsA(setOp->larg, SetOperationStmt))
			replaceSetOperatorSubtree (query, (SetOperationStmt *) setOp->larg, &(setOp->larg));

		if (IsA(setOp->rarg, SetOperationStmt))
			replaceSetOperatorSubtree (query, (SetOperationStmt *) setOp->rarg, &(setOp->rarg));
	}

	/*
	 *  Optimization is activated. Keep original set operation tree, if it just contains only union or only intersection operations.
	 *  For a set difference operation both operands are outsourced into a separate query (If they are not simple range table references).
	 *  For a mixed unions and intersections we have to outsource sub trees under set operations differend from the root set operation.
	 */

	switch (setOp->op)
	{
		/* union or intersect, do not change anything */
		case SETOP_UNION:
		case SETOP_INTERSECT:
			/* check if of the same type as parent */
			if (setOp->op == rootType)
			{
				replaceSetOperationSubTrees (query, setOp->larg, &(setOp->larg), rootType);
				replaceSetOperationSubTrees (query, setOp->rarg, &(setOp->rarg), rootType);
			}
			/* another type replace subtree */
			else
				replaceSetOperatorSubtree(query, setOp, parentPointer);
		break;
		/* set difference, replace subtree with new query node */
		case SETOP_EXCEPT:
			/* if is root set operation replace left and right sub trees */
			if (rootType == SETOP_EXCEPT) {
				if (IsA(setOp->larg, SetOperationStmt))
					replaceSetOperatorSubtree (query, (SetOperationStmt *)
							setOp->larg, &(setOp->larg));

				/* is wl semantics is used the right subtree can be left
				 * untouched */
				if (IsA(setOp->rarg, SetOperationStmt)
						&& !prov_use_wl_union_semantics)
					replaceSetOperatorSubtree (query, (SetOperationStmt *)
							setOp->rarg, &(setOp->rarg));
			}
			/* is not root operation process as for operator change */
			else
				replaceSetOperatorSubtree(query, setOp, parentPointer);
		break;
		default:
			elog(ERROR,
					"Unknown set operation type: %d",
					setOp->op);
		break;
	}
}

/*
 * Replaces a subtree in an set operation tree with a new subquery that represents the
 * set operations performed by the sub tree.
 */

static void
replaceSetOperatorSubtree (Query *query, SetOperationStmt *setOp, Node **parent)
{
	ListCell *lc;
	List *subTreeRTEs;
	List *subTreeRTindex;
	List *subTreeRTrefs;
	List *queryRTrefs;
	List *newRtable;
	Query *newSub;
	RangeTblEntry *rte;
	RangeTblRef *rtRef;
	int counter;
	int *context;

	subTreeRTEs = NIL;
	subTreeRTindex = NIL;

	/* find all range table entries referenced from the subtree under setOp */
	findSetOpRTEs(query->rtable,(Node *) setOp, &subTreeRTEs, &subTreeRTindex);

	/* create new query node for subquery */
	newSub = (Query *) copyObject(query);
	newSub->rtable = NIL;
	newSub->setOperations = (Node *) setOp; //CHECK ok to not copy?

	/* create range table entries for range table entries referenced from set operation in subtree */
	foreach(lc,subTreeRTEs)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		newSub->rtable = lappend(newSub->rtable, (RangeTblEntry *) copyObject(rte));
	}

	/* adapt RTErefs in sub tree */
	subTreeRTrefs = getSetOpRTRefs((Node *) newSub->setOperations);

	counter = 1;
	foreach(lc, subTreeRTrefs)
	{
		rtRef = (RangeTblRef *) lfirst(lc);
		rtRef->rtindex = counter;
		counter++;
	}

	/* add new sub query to range table */
	addSubqueryToRTWithParam (query, newSub, "newSub", false, ACL_NO_RIGHTS, true);

	/* replace subtree with RTE reference */
	MAKE_RTREF(rtRef, list_length(query->rtable));
	*parent = (Node *) rtRef;

	LOGNODE(query, "before range table adapt");

	/* adapt range table and rteRefs for query */
	newRtable = NIL;
	queryRTrefs = getSetOpRTRefs(query->setOperations);

	foreach(lc, queryRTrefs)
	{
		rtRef = lfirst(lc);
		rte = rt_fetch(rtRef->rtindex, query->rtable);
		newRtable = lappend(newRtable, rte);
		rtRef->rtindex = list_length(newRtable);
	}

	query->rtable = newRtable;

	/* increase sublevelsup of newSub if we are rewritting a sublink query */
	context = (int *) palloc(sizeof(int));
	*context = -1;
	increaseSublevelsUpMutator ((Node *) newSub, context);
	pfree(context);

	logNode(query, "after replace of subtree");
}

/*
 *	Returns lists with RTindexes and RangeTblEntries that are accessed by set
 *	operation tree under setTreeNode. If rtes or rtindex is null only the
 *	other list is filled.
 */

void
findSetOpRTEs (List *rtable, Node *setTreeNode, List **rtes, List **rtindex)
{
	SetOperationStmt *setOp;
	Index rtIndex;
	RangeTblEntry *rtEntry;

	if (IsA(setTreeNode, SetOperationStmt))
	{
		setOp = (SetOperationStmt *) setTreeNode;
		findSetOpRTEs (rtable, setOp->larg, rtes, rtindex);
		findSetOpRTEs (rtable, setOp->rarg, rtes, rtindex);
	}
	else if (IsA(setTreeNode, RangeTblRef))
	{
		rtIndex = ((RangeTblRef *) setTreeNode)->rtindex;
		rtEntry = (RangeTblEntry *) list_nth(rtable, (rtIndex - 1));
		if (rtes)
			*rtes = lappend(*rtes, rtEntry);
		if (rtindex)
			*rtindex = lappend_int(*rtindex, rtIndex);
	}
	else
		elog(ERROR,
				"Unexpected node of type %d in set operation tree",
				setTreeNode->type);
}

/*
 * For a node in a Set operation tree return a list with the
 * range table references stored in the leaf nodes of the sub tree
 * under this node.
 */

List *
getSetOpRTRefs (Node *setTreeNode)
{
	List *result;
	SetOperationStmt *setOp;
	RangeTblRef *rtRef;

	result = NIL;
	if (IsA(setTreeNode, SetOperationStmt))
	{
		setOp = (SetOperationStmt *) setTreeNode;
		result = list_concat(result, getSetOpRTRefs (setOp->larg));
		result = list_concat(result, getSetOpRTRefs (setOp->rarg));
	}
	else if (IsA(setTreeNode, RangeTblRef))
	{
		rtRef = ((RangeTblRef *) setTreeNode);
		result = lappend(result, rtRef);
	}
	else
		elog(ERROR,
				"Unexpected node of type %d in set operation tree",
				setTreeNode->type);

	return result;
}


/*
 * Creates an equality condition expression for two lists of attributes.
 * E.g. A = (a,b,c) and B = (d,e,f) then the following condition would be created:
 * a = d AND b = e AND c = f. If neq is true the whole condition is negated.
 */

static Node *
createEqualityCondition (List* leftAttrs, List* rightAttrs, Index leftIndex, Index rightIndex, BoolExprType boolOp, bool neq)
{
	ListCell *leftLc;
	ListCell *rightLc;
	TargetEntry *curLeft;
	TargetEntry *curRight;
	OpExpr *equal;
	List *equalConds;
	Var *leftOp;
	Var *rightOp;
	Node *curRoot;

	equalConds = NIL;

	Assert (list_length(leftAttrs) == list_length(rightAttrs));

	/* create List of OpExpr nodes for equality conditions */
	forboth (leftLc, leftAttrs, rightLc, rightAttrs)
	{
		curLeft = (TargetEntry *) lfirst(leftLc);
		curRight = (TargetEntry *) lfirst(rightLc);

		/* create Var for left operand of equality expr */
		leftOp = makeVar (leftIndex + 1,
				curLeft->resno,
				exprType ((Node *) curLeft->expr),
				exprTypmod ((Node *) curLeft->expr),
				0);

		/* create Var for right operand of equality expr */
		rightOp = makeVar (rightIndex + 1,
				curRight->resno,
				exprType ((Node *) curRight->expr),
				exprTypmod ((Node *) curRight->expr),
				0);

		/* get equality operator for the var's type */
		equal = (OpExpr *) createNotDistinctConditionForVars (leftOp, rightOp);

		/* append current equality condition to equalConds List */
		equalConds = lappend (equalConds, equal);
	}

	curRoot = (Node *) createAndFromList(equalConds);

	/* negation required */
	if (neq)
		curRoot = (Node *) makeBoolExpr(NOT_EXPR, list_make1(curRoot));

	return curRoot;
}
