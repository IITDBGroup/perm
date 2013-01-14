/*-------------------------------------------------------------------------
 *
 * prov_how_spj.c
 *	  PERM C 
 *
 * Portions Copyright (c) 2010 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $Perm: pgsql//SF_Perm/src/backend/provrewrite/howsem/prov_how_spj.c,v 1.542 Nov 18, 2010 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/pg_type.h"
#include "nodes/parsenodes.h"
#include "nodes/makefuncs.h"
#include "parser/parsetree.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "utils/fmgroids.h"

#include "provrewrite/prov_util.h"
#include "provrewrite/prov_how_main.h"
#include "provrewrite/prov_how_spj.h"

/* macros */
#define MULT_HOW(result, left, right) \
	do { \
		if (left) \
			result = (Node *) makeFuncExpr(F_HOWPROV_MULTIPLY, HOWPROVOID, \
					list_make2(left,right), COERCE_IMPLICIT_CAST); \
		else \
			result = right; \
	} while (0)

#define CASTOID_TO_HOW(result,rtindex) \
	do { \
		Var *var = makeVar(rtindex, -2, OIDOID, -1, 0); \
		result = (Node *) makeFuncExpr(F_OID_TO_HOWPROV, HOWPROVOID, \
				list_make1(var), COERCE_EXPLICIT_CALL); \
	} while (0)

/* function declarations */
static Node *createFromHowExpr (Node *fromItem, Query *query);

/*
 *
 */

Query *
rewriteHowSPJ (Query *query)
{
	ListCell *lc;
	RangeTblEntry *rte;
	Node *fromItem;
	Node *howExpr = NULL;

	/* rewrite subqueries */
	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		if (rte->rtekind == RTE_SUBQUERY)
		{
			rte->subquery = rewriteHowQueryNode(rte->subquery);
			correctRTEAlias(rte);
		}
	}

	foreach(lc, query->jointree->fromlist)
	{
		fromItem = (Node *) lfirst(lc);
		MULT_HOW(howExpr, howExpr, createFromHowExpr(fromItem, query));
	}

	createHowAttr(query, howExpr);

	return query;
}

/*
 *
 */

void
addHowAgg (Query *query)
{
	ListCell *lc;
	TargetEntry *te;
	GroupClause *groupBy;
	Index curGroupRef = 1;
	Aggref *agg;
	Oid argTypes[1] = { HOWPROVOID };

	/* add group by on result attributes */
	for(lc = list_head(query->targetList); lc->next != NULL; lc = lc->next)
	{
		te = (TargetEntry *) lfirst(lc);
		te->ressortgroupref = curGroupRef;

		// create group by for original attribute
		groupBy = makeNode(GroupClause);
		groupBy->tleSortGroupRef = curGroupRef++;
		groupBy->sortop = ordering_oper_opid(exprType((Node *) te->expr));
		query->groupClause = lappend(query->groupClause, groupBy);
	}

	/* aggregate how attribute */
	te = (TargetEntry *) llast(query->targetList);

	agg = makeNode(Aggref);
	agg->args = list_make1(te->expr);
	agg->agglevelsup = 0;
	agg->aggstar = false;
	agg->aggdistinct = false;
	agg->aggtype = HOWPROVOID;
	agg->aggfnoid = LookupFuncName(
			list_make1(makeString("how_sum")), 1, argTypes, false);

	te->expr = (Expr *) agg;

	query->hasAggs = true;

	/* remove distinct */
	query->distinctClause = NULL;
}


/*
 *
 */

static Node *
createFromHowExpr (Node *fromItem, Query *query)
{
	Node *result;

	if (IsA(fromItem, JoinExpr))
	{
		JoinExpr *join = (JoinExpr *) fromItem;

		MULT_HOW(result, createFromHowExpr(join->larg, query),
				createFromHowExpr(join->rarg, query));
	}
	else
	{
		RangeTblRef *rtRef = (RangeTblRef *) fromItem;
		RangeTblEntry *rte;

		rte = rt_fetch(rtRef->rtindex, query->rtable);

		/* base relation create a cast of the oid attribute of this relation */
		if (rte->rtekind == RTE_RELATION)
			CASTOID_TO_HOW(result, rtRef->rtindex);
		/* subquery, use the howprov attribute of this subquery */
		else
			result = (Node *) makeVar(rtRef->rtindex, list_length(rte->eref->colnames),
					HOWPROVOID, -1, 0);
	}

	return result;
}

/*
 *
 */

void
createHowAttr (Query *query, Node *howExpr)
{
	TargetEntry *newTe;

	newTe = makeTargetEntry((Expr *) howExpr,
			list_length(query->targetList) + 1, "howprov", false);

	query->targetList = lappend(query->targetList, newTe);
}
