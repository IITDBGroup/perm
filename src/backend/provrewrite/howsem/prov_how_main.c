/*-------------------------------------------------------------------------
 *
 * prov_how_main.c
 *	  PERM C 
 *
 * Portions Copyright (c) 2010 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $Perm: pgsql//SF_Perm/src/backend/provrewrite/howsem/prov_how_main.c,v 1.542 Oct 4, 2010 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "nodes/parsenodes.h"

#include "provrewrite/prov_how_set.h"
#include "provrewrite/prov_how_spj.h"

#include "provrewrite/prov_how_main.h"

/* function declarations */

static void checkOkForHowRewrite (Query *query);
static void checkHowJoinTree (Node *fromItem, Query *query);
static void checkHowSetOp (Node *setOp, Query *query);

/*
 *
 */

Query *
rewriteQueryHow (Query *query)
{
	checkOkForHowRewrite(query);

	query = rewriteHowQueryNode (query);
	addHowAgg (query);

	return query;
}

/*
 *
 */

Query *
rewriteHowQueryNode (Query *query)
{
	if (query->setOperations)
		return rewriteHowSet(query);
	return rewriteHowSPJ(query);
}

/*
 * Checks if the input query uses only features that are supported by How-provenance. Disallowed
 * features are:
 *
 * 	-Aggregations
 * 	-Sublinks
 * 	-Set difference
 */

static void
checkOkForHowRewrite (Query *query)
{
	RangeTblEntry *rte;
	ListCell *lc;
	Node *fromItem;

	if (query->hasSubLinks)
		ereport (ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("How-CS does not support sublinks.")));
	if (query->hasAggs)
		ereport (ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("How-CS does not support aggregation.")));

	if (query->setOperations)
		checkHowSetOp (query->setOperations, query);

	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		switch(rte->rtekind)
		{
		case RTE_JOIN:
		case RTE_RELATION:
			break;
		case RTE_SUBQUERY:
			checkOkForHowRewrite(rte->subquery);
			break;
		default:
			ereport (ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
							errmsg("How-CS does not support set returning "
									"functions")));
		}
	}

	foreach(lc, query->jointree->fromlist)
	{
		fromItem = (Node *) lfirst(lc);
		checkHowJoinTree(fromItem, query);
	}
}

/*
 *
 */

static void
checkHowJoinTree (Node *fromItem, Query *query)
{
	if (IsA(fromItem, JoinExpr))
	{
		JoinExpr *join = (JoinExpr *) fromItem;

		switch(join->jointype)
		{
		case JOIN_INNER:
			break;
		default:
			break;//TODO outer supported?
		}

		checkHowJoinTree(join->larg, query);
		checkHowJoinTree(join->rarg, query);
	}
}

/*
 *
 */

static void
checkHowSetOp (Node *setOp, Query *query)
{
	if (IsA(setOp, SetOperationStmt))
	{
		SetOperationStmt *set = (SetOperationStmt *) setOp;

		if (set->op == SETOP_EXCEPT)
			ereport (ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
							errmsg("How-CS does not support set difference")));
	}
}
