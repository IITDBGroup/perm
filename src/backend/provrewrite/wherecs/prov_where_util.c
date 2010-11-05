/*-------------------------------------------------------------------------
 *
 * prov_where_util.c
 *	  PERM C 
 *
 * Portions Copyright (c) 2010 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $Perm: pgsql//SF_Perm/src/backend/provrewrite/wherecs/prov_where_util.c,v 1.542 Oct 28, 2010 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "optimizer/clauses.h"
#include "nodes/makefuncs.h"

#include "provrewrite/provstack.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_sublink_util_mutate.h"
#include "provrewrite/prov_where_util.h"

/* prototypes */
static void addSubqueriesAsBase(Query *root, Query *sub, int subIndex,
		List *vars, List **subFroms);
static List *fetchAllRTEVars (Query *query, List **vars, int rtIndex);
static bool fetchAllRTEVarsWalker (Node *node, FetchVarsContext *context);
static bool addToVarnoWalker (Node *node, int *context);
static void adaptJoinTreeWithSubTrees (Query *query, List *subIndex,
		List *rtindexMap, List *subFroms);
static void adaptFromItem (Node **fromItem, List *subIndex, List *rtindexMap,
		List *subFroms);

/*
 *
 */

Query *
pullUpSubqueries (Query *query)
{
	List *rtindexMap = NIL;
	List *subIndex = NIL;
	List *subqueries = NIL;
	List *relAccess = NIL;
	List *subFroms = NIL;
	List **subqueryVars;
	ListCell *lc, *innerLc;
	Query *sub;
	RangeTblEntry *rte;
	Var *var;
	int i;

	/* check for a simple wrapper query around a union */
	if (list_length(query->rtable) == 1 && !query->setOperations)
	{
		rte = (RangeTblEntry *) linitial(query->rtable);

		if (rte->rtekind == RTE_SUBQUERY)
		{
			sub = rte->subquery;

			if (sub->setOperations)
				return sub;
		}
	}

	/* in case of a UNION try to pull up the subqueries of inputs to the union */
	if (query->setOperations)
	{
		foreach(lc, query->rtable)
		{
			rte = (RangeTblEntry *) lfirst(lc);

			if (rte->rtekind == RTE_SUBQUERY)
				pullUpSubqueries(rte->subquery);
		}

		return query;
	}

	/* partition query into relation accesses/joins and subqueries */
	foreachi(lc, i, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		Assert(rte->rtekind == RTE_SUBQUERY
				|| rte->rtekind == RTE_RELATION
				|| rte->rtekind == RTE_JOIN);

		if (rte->rtekind == RTE_SUBQUERY)
		{
			subqueries = lappend(subqueries, rte->subquery);
			subIndex = lappend_int(subIndex, i + 1);
		}
		else
		{
			relAccess = lappend(relAccess, rte);
			rtindexMap = lappend_int(rtindexMap, i + 1);
		}
	}

	/* preserve the vars of subqueries and basertes */
	subqueryVars = (List **) palloc(sizeof(List *) * list_length(query->rtable));

	for(i = 0; i < list_length(query->rtable); i++)
		subqueryVars[i] = NIL;

	for(i = 1; i <= list_length(query->rtable); i++)
		fetchAllRTEVars(query, &(subqueryVars[i - 1]), i);

	/* map the rtindex for base relation accesses to new order */
	foreachi(lc, i, rtindexMap)
	{
		foreach(innerLc, subqueryVars[lfirst_int(lc) - 1])
		{
			var = (Var *) lfirst(innerLc);
			var->varno = i + 1;
		}
	}

	/* start of with only the base relation accesses */
	query->rtable = relAccess;

	/* retrieve all the base relation accesses of each subqueries, possibly
	 * calling ourselves.
	 */

	forboth(lc, subqueries, innerLc, subIndex)
	{
		sub = (Query *) lfirst(lc);
		addSubqueriesAsBase(query, sub, lfirst_int(innerLc),
				subqueryVars[lfirst_int(innerLc) - 1], &subFroms);
	}

	/* adapt join tree */
	adaptJoinTreeWithSubTrees (query, subIndex, rtindexMap, subFroms);

	/* recreate join RT entries */
	recreateJoinRTEs(query);

	return query;
}

/*
 *
 */

static void
addSubqueriesAsBase(Query *root, Query *sub, int subIndex, List *vars, List **subFroms)
{
	ListCell *lc;
	TargetEntry *te;
	Var *var;
	Var *innerVar;
	int curRtindex = list_length(root->rtable);
	FromExpr *subJoinTree;

	joinQueryRTEs(sub);
	pullUpSubqueries(sub);

	/* set varnos to new varno for the rte for the vars for sub in the target
	 * list of the super query */
	foreach(lc, vars)
	{
		var = (Var *) lfirst(lc);
		te = list_nth(sub->targetList, var->varattno - 1);
		innerVar = getVarFromTeIfSimple((Node *) te->expr);
		Assert(innerVar != NULL);

		var->varno = curRtindex + innerVar->varno;
		var->varattno = innerVar->varoattno;
		var->vartype = innerVar->vartype;
		var->vartypmod = innerVar->vartypmod;
	}

	root->rtable = list_concat(root->rtable, sub->rtable);

	/* get subquery join tree and adapt varnos */
	subJoinTree = sub->jointree;
	addToVarnoWalker((Node *) subJoinTree, &curRtindex);
	*subFroms = lappend(*subFroms, subJoinTree);
}


/*
 * Search for vars from a range table entry in a query node and append them to
 * list vars.
 */

static List *
fetchAllRTEVars (Query *query, List **vars, int rtIndex)
{
	FetchVarsContext *context;

	context = (FetchVarsContext *) palloc(sizeof(FetchVarsContext));
	context->rtindex = rtIndex;
	context->result = vars;

	query_tree_walker(query, fetchAllRTEVarsWalker, (void *) context, QTW_IGNORE_RT_SUBQUERIES);

	return *vars;
}

/*
 *
 */

static bool
fetchAllRTEVarsWalker (Node *node, FetchVarsContext *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, Var))
	{
		Var *var = (Var *) node;

		if (var->varno == context->rtindex)
			*(context->result) = lappend(*(context->result), var);

		return false;
	}

	return expression_tree_walker(node, fetchAllRTEVarsWalker, (void *) context);
}

/*
 *
 */

static bool
addToVarnoWalker (Node *node, int *context)
{
	if (node == NULL)
		return false;

//	if (IsA(node, Var))
//	{
//		Var *var = (Var *) node;
//
//		var->varno += *context;
//
//		return false;
//	}

	if (IsA(node, JoinExpr))
	{
		JoinExpr *join = (JoinExpr *) node;

		join->rtindex += *context;
	}
	else if (IsA(node, RangeTblRef))
	{
		RangeTblRef *rtRef = (RangeTblRef *) node;

		rtRef->rtindex += *context;

		return false;
	}

	return expression_tree_walker (node, addToVarnoWalker, context);
}

/*
 *
 */

static void
adaptJoinTreeWithSubTrees (Query *query, List *subIndex, List *rtindexMap, List *subFroms)
{
	ListCell *lc;
	FromExpr *subFrom;
	Node **fromItem;

	foreach(lc, subFroms)
	{
		subFrom = (FromExpr *) lfirst(lc);

		if (subFrom->quals != NULL) {
			if (query->jointree->quals == NULL)
				query->jointree->quals = subFrom->quals;
			else
				query->jointree->quals = (Node *) makeBoolExpr(AND_EXPR,
						list_make2(query->jointree->quals, subFrom->quals));
		}
	}


	foreach(lc, query->jointree->fromlist)
	{
		fromItem = (Node **) &(lc->data.ptr_value);
		adaptFromItem(fromItem, subIndex, rtindexMap, subFroms);
	}
}

/*
 *
 */

static void
adaptFromItem (Node **fromItem, List *subIndex, List *rtindexMap, List *subFroms)
{
	int pos;
	FromExpr *subFrom;

	if (IsA(*fromItem, JoinExpr))
	{
		JoinExpr *join = (JoinExpr *) *fromItem;

		pos = listPositionInt(rtindexMap, join->rtindex);
		join->rtindex = pos + 1;

		adaptFromItem(&(join->larg), subIndex, rtindexMap, subFroms);
		adaptFromItem(&(join->rarg), subIndex, rtindexMap, subFroms);
	}
	else
	{
		RangeTblRef *rtRef = (RangeTblRef *) *fromItem;

		pos = listPositionInt(subIndex, rtRef->rtindex);

		/* is a subquery */
		if (pos != -1)
		{
			subFrom = (FromExpr *) list_nth(subFroms, pos);

			/* sub query from list should have only one item */
			Assert (list_length(subFrom->fromlist) == 1);

			*fromItem = (Node *) linitial(subFrom->fromlist);
		}
		/* is a base relation rte */
		else
		{
			pos = listPositionInt(rtindexMap, rtRef->rtindex);
			rtRef->rtindex = pos + 1;
		}
	}
}
