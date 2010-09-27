/*-------------------------------------------------------------------------
 *
 * prov_aggr.c
 *	  PERM C  - I-CS data-data provenance rewrites for queries with aggregation.
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/prov_aggr.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
 *
 * NOTES
 *	Aggregation is rewritten in Perm by joining the original aggregation with its rewritten input on the
 *	group by attributes (see ICDE '09 paper). Therefore, a new top query node is introduced that performs
 *	this join. Group by attributes are not necessarily present in the target list of a query, but to be able
 *	to join on them we have to add them there. This doesn't change the result schema because group by attributes
 *	that did not belong to the original target list are projected out by the new top query node.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "catalog/pg_operator.h"		// pg_operator system table for operator lookup
#include "nodes/makefuncs.h"			// needed to create new nodes
#include "nodes/print.h"				// pretty print node (trees)
#include "optimizer/clauses.h"			// tools for expression clauses
#include "parser/parse_expr.h"			// expression transformation used for expression type calculation
#include "parser/parse_oper.h"			// defintion of Operator type and convience routines for operator lookup
#include "utils/syscache.h"				// used to release heap tuple references
#include "provrewrite/provlog.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_aggr.h"
#include "provrewrite/prov_spj.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/prov_sublink.h"
#include "provrewrite/prov_sublink_agg.h"
#include "provrewrite/prov_sublink_util_analyze.h"
#include "provrewrite/prov_sublink_util_mutate.h"
#include "provrewrite/prov_sublink_util_search.h"

/* Function declarations */
static bool isAggrExprWalker (Node *node, bool* context);
static TargetEntry *getTEforSortgroupref (Query *query, Index sortGroupRef);
static List *sortTargetListOnGroupBy (List *targetList);

/*
 * Rewrite a Query node that uses aggregate functions (and/or group by).
 */

Query *
rewriteAggregateQuery (Query *query)
{
	Query *newTopQuery;
	Query *newRewriteQuery;
	List *pList;
	List *groupByTLEs;
	Index rtIndex;
	int *context;
	RangeTblEntry *rte;

	pList = NIL;

	/* Check if the query contains sublinks in HAVING and if so apply sublink rewrite.
	 * If provSublinksRewritten is true we have already rewritten the having sublinks
	 * before, so don't do it again.
	 */
	if (!IsSublinkRewritten(query) && hasHavingOrTargetListOrGroupBySublinks(query))
	{
		query = transformAggWithSublinks (query);

		/* if normalized query is a SPJ query use rewriteSPJQuery */
		if (!query->hasAggs)
			return rewriteSPJQuery(query);
	}

	/* get group by attributes */
	groupByTLEs = getGroupByTLEs (query);

	/* copy query node and strip off aggregation and limit-clause */
	newRewriteQuery = copyObject (query);
	newRewriteQuery->limitCount = NULL;
	newRewriteQuery->limitOffset = NULL;
	SetSublinkRewritten(newRewriteQuery, false);

	/* if aggregation query has no sublinks or only sublinks in HAVING newRewriteQuery has no sublinks */
	if (!hasNonHavingSublink (query))
		newRewriteQuery->hasSubLinks = false;

	rewriteAggrSubqueryForRewrite (newRewriteQuery, false);

	/* create new top query node */
	newTopQuery = makeQuery();

	/* copy ORDER BY clause */
	checkOrderClause(newTopQuery, query);

	/* add original query to rtable of new top query */
	addSubqueryToRT (newTopQuery, query, appendIdToString("originalAggr", &curUniqueRelNum));

	/* add result attributes of original query to targetList */
	rtIndex = 1;
	addSubqueryTargetListToTargetList (newTopQuery, rtIndex);

	/* add join on group by attributes */
	addJoinOnAttributes (newTopQuery, groupByTLEs, newRewriteQuery);

	/* rewrite new subquery without aggregation */
	newRewriteQuery = rewriteQueryNode (newRewriteQuery);

	addSubqueryToRT (newTopQuery, newRewriteQuery, appendIdToString("rewrittenAggrSubquery", &curUniqueRelNum));

	/* correct eref for sub query entries */
	correctSubQueryAlias (newTopQuery);

	/* add range table entry for join */
	rte = makeRte(RTE_JOIN);
	rte->jointype = JOIN_LEFT;
	newTopQuery->rtable = lappend(newTopQuery->rtable, rte);

	SetProvRewrite(newRewriteQuery,false);
	SetProvRewrite(query,false);
	adaptRTEsForJoins(list_make1(linitial(newTopQuery->jointree->fromlist)), newTopQuery, "joinAggAndRewrite");

	/* add provenance attributes of sub queries to targetlist */
	pList = addProvenanceAttrs (newTopQuery, list_make1_int (2), pList);


	/* push list of provenance attributes to pStack */
	pStack = lcons(pList, pStack);

	/* correct sublevelsup if we are rewritting a sublink query */
	//OPTIMIZE mark sublink queries so we don't have to run increaseSublevelsUpMutator for non sublink queries
	context = (int *) palloc(sizeof(int));

	*context = -1;
	increaseSublevelsUpMutator((Node *) newRewriteQuery, context);
	increaseSublevelsUpMutator((Node *) query, context);

	pfree(context);

	return newTopQuery;
}

/*
 * Stripes of the aggregation from a query node. This is used to produce the input of
 * the aggregation that can be rewritten using SPJ-query rewrite. If returnMapping is
 * true the method returns a mapping between the original positions of a target entry and
 * the new positon after removing target entries that contain aggregation functions.
 */

List *
rewriteAggrSubqueryForRewrite (Query *query, bool returnMapping)
{
	AttrNumber curResno;
	List *newTargetList;
	ListCell *lc;
	TargetEntry *te;
	List *result;

	newTargetList = NIL;
	curResno = 1;
	result = NIL;

	/* create new target list containing group by attributes only */
	foreach (lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);

		if (!isAggrExpr((Node *) te->expr))
		{
			if (returnMapping)
				result = lappend_int(result, te->resno);

			te->resno = curResno;
			if (te->resjunk)
				te->resname = appendIdToString("skippedGroupBy", &curUniqueAttrNum);
			te->resjunk = false;

			newTargetList = lappend(newTargetList, te);

			curResno++;
		}

	}

	/* remove aggregation */
	query->hasAggs = false;
	query->groupClause = NIL;
	query->havingQual = NULL;
	query->distinctClause = NIL;
	query->sortClause = NIL;

	query->targetList = newTargetList;

	return result;
}

/*
 * Checks if an expr contains an aggregation.
 */

bool
isAggrExpr (Node *node)
{
	bool result;

	result = false;
	isAggrExprWalker (node, &result);
	return result;
}

/*
 * Helper function for isAggrExpr that walks an expression tree and
 * searches for aggregation functions.
 */

static bool
isAggrExprWalker (Node *node, bool* context)
{
	if (node == NULL)
		return false;

	if (IsA(node, Aggref))
	{
		*context = true;
		return false;
	}
	return expression_tree_walker(node, isAggrExprWalker, (void *) context);
}



/*
 * Adds the target list entries from subquery at the position rtindex in the range table of query
 * to the target list of query. Works only for query nodes with solely subquery RTE's.
 */

void
addSubqueryTargetListToTargetList (Query *query, Index rtindex)
{
	Query *subquery;
	ListCell *lc;
	List *newTargetList;
	TargetEntry *subTe;
	TargetEntry *newTe;
	Expr *expr;
	Index curResno;

	newTargetList = query->targetList;
	subquery = ((RangeTblEntry *) list_nth(query->rtable, rtindex - 1))->subquery;
	curResno = list_length(query->targetList) + 1;

	foreach (lc, subquery->targetList)
	{
		subTe = (TargetEntry *) lfirst(lc);

		/* add only target list entries that are not resjunc */
		if (!subTe->resjunk)
		{
			expr = (Expr *) makeVar (rtindex,
						subTe->resno,
						exprType ((Node *) subTe->expr),
						exprTypmod ((Node *) subTe->expr),
						0);
			newTe = makeTargetEntry (expr, curResno, subTe->resname, subTe->resjunk);

			/* copy ressortgroupref only if this target entry is used in order by */
			if (isUsedInOrderBy(subquery, subTe))
				newTe->ressortgroupref = subTe->ressortgroupref;
			else
				newTe->ressortgroupref = 0;

			newTargetList = lappend(newTargetList, newTe);
			curResno++;
		}
		/*
		 * if an target list entry is resjunc set rejunc to false in original aggregation.
		 * That enables us to use the resjunc entry (a group by) in the join between
		 * original aggregation and the rewritten query without aggregation.
		 */
		else
		{
			subTe->resjunk = false;
			subTe->resname = appendIdToString("skippedGroupBy", &curUniqueAttrNum);
		}
	}

	/* set new TargetList */
	query->targetList = newTargetList;
}

/*
 * Creates a join condition with equality comparisons for attributes from
 * joinAttrsLeft and joinAttrsRight.
 */

//CHECK can be replaced by generic function from prov_util?
void
addJoinOnAttributes (Query *query, List *joinAttrsLeft, Query *rightSub)
{
	FromExpr *newFrom;
	JoinExpr *joinExpr;
	List *joinAttrsRight;
	ListCell *leftLc;
	ListCell *rightLc;
	TargetEntry *curLeft;
	TargetEntry *curRight;
	Node *equal;
	List *equalConds;
	Var *leftOp;
	Var *rightOp;
	Node *curRoot;
	BoolExpr *boolNode;
	RangeTblRef *rtRef;

	equalConds = NIL;
	joinAttrsRight = copyObject(rightSub->targetList);
	joinAttrsLeft = copyObject(joinAttrsLeft);

	Assert (list_length(joinAttrsLeft) == list_length(joinAttrsRight));

	/* sort right attrs and left attrs on groupby */ //CHECK can break if more than one attr for one groupby ref --is this possible
	sortTargetListOnGroupBy(joinAttrsRight);
	sortTargetListOnGroupBy(joinAttrsLeft);

	/* set ressortgroupref = 0 for rewritten subquery without aggregation */
	foreach(rightLc, rightSub->targetList)
	{
		curRight = (TargetEntry *) lfirst(rightLc);
		curRight->ressortgroupref = 0;
	}

	/* create List of OpExpr nodes for equality conditions */
	forboth (leftLc, joinAttrsLeft, rightLc, joinAttrsRight)
	{
		curLeft = (TargetEntry *) lfirst(leftLc);
		curRight = (TargetEntry *) lfirst(rightLc);

		/* create Var for left operand of equality expr */
		leftOp = makeVar (1,
				curLeft->resno,
				exprType ((Node *) curLeft->expr),
				exprTypmod ((Node *) curLeft->expr),
				0);

		/* create Var for right operand of equality expr */
		rightOp = makeVar (2,
				curRight->resno,
				exprType ((Node *) curRight->expr),
				exprTypmod ((Node *) curRight->expr),
				0);

		/* create a not distinct condition (basically equality check but null = null) */
		equal = createNotDistinctConditionForVars (leftOp, rightOp);

		/* append current equality condition to equalConds List */
		equalConds = lappend (equalConds, equal);
	}

	if (list_length(equalConds) > 0)
	{
		curRoot = (Node *) pop (&equalConds);

		/* create complete boolean expression */
		while (equalConds != NIL)
		{
			boolNode = makeNode (BoolExpr);
			boolNode->boolop = AND_EXPR;
			boolNode->args = NIL;
			boolNode->args = lappend (boolNode->args, curRoot);
			boolNode->args = lappend (boolNode->args, (Node *) pop (&equalConds));
			curRoot = (Node *) boolNode;
		}
	}
	else
		curRoot = makeBoolConst(true,false);

	/* create FromExpr node */
	newFrom = makeNode (FromExpr);
	newFrom->fromlist = NIL;

	joinExpr = makeNode(JoinExpr);
	joinExpr->jointype = JOIN_LEFT;
	joinExpr->rtindex = 3;
	joinExpr->quals = curRoot;

	newFrom->fromlist = lappend (newFrom->fromlist, joinExpr);

	rtRef = makeNode (RangeTblRef);
	rtRef->rtindex = 1;
	joinExpr->larg = (Node *) rtRef;

	rtRef = makeNode (RangeTblRef);
	rtRef->rtindex = 2;
	joinExpr->rarg = (Node *) rtRef;

	query->jointree = newFrom;
}

/*
 * Copies the ORDER BY clause from query to newTop. A order by item is skipped if it references a
 * target entry that will not be present in newTop.
 */

void
checkOrderClause (Query *newTop, Query *query)
{
	SortClause *orderBy;
	ListCell *lc;
	TargetEntry *te;

	newTop->sortClause = NIL;

	foreach(lc, query->sortClause)
	{
		orderBy = (SortClause *) lfirst(lc);

		te = getTEforSortgroupref(query, orderBy->tleSortGroupRef);

		if (!te->resjunk)
			newTop->sortClause = lappend(newTop->sortClause,copyObject(orderBy));
	}

}

/*
 * For a given ressortgroupref value returns the target entry that has this value.
 */

static TargetEntry *
getTEforSortgroupref (Query *query, Index sortGroupRef)
{
	ListCell *lc;
	TargetEntry *te;

	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);

		if (te->ressortgroupref == sortGroupRef)
			return te;
	}

	return NULL;
}

/*
 * Sort Target list on group by position (ressortgroupref).
 */

static List *
sortTargetListOnGroupBy (List *targetList)
{
	return sortList(&targetList, compareTeOnRessortgroupref, true);
}

