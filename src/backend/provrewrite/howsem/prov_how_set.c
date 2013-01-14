/*-------------------------------------------------------------------------
 *
 * prov_how_set.c
 *	  PERM C 
 *
 * Portions Copyright (c) 2010 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $Perm: pgsql//SF_Perm/src/backend/provrewrite/howsem/prov_how_set.c,v 1.542 Nov 19, 2010 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "nodes/parsenodes.h"

#include "provrewrite/prov_set.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_how_main.h"
#include "provrewrite/prov_how_spj.h"
#include "provrewrite/prov_how_set.h"

/* function declarations */
static void rewriteHowUnion(Query *query);
static void adaptSetOps (Node *setOp);
static void rewriteHowIntersect(Query *query);

/*
 *
 */

Query *
rewriteHowSet (Query *query)
{
	Query *newTop;
	SetOperation setOp = ((SetOperationStmt *) query->setOperations)->op;
	RangeTblRef *rtRef;

	/* restructure set operation tree if necessary */
	replaceSetOperationSubTrees (query, query->setOperations,
			&(query->setOperations),
			setOp);

	/* check if the query is a UNION or a INTERSECTION */
	if (setOp == SETOP_UNION)
		rewriteHowUnion(query);
	else
		rewriteHowIntersect(query);

	/* create wrapper query that aggregates the how provenance */
	newTop = makeQuery();
	newTop->targetList = copyObject(query->targetList);
	addSubqueryToRT(newTop, query, "rewritten_setop");
	MAKE_RTREF(rtRef, 1);
	newTop->jointree->fromlist = list_make1(rtRef);
	correctRTEAlias(linitial(newTop->rtable));

	//addHowAgg(newTop);

	return newTop;
}

/*
 *
 */

static void
rewriteHowUnion(Query *query)
{
	ListCell *lc;
	RangeTblEntry *rte;
	Var *var;

	/* rewrite all the subqueries */
	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		rewriteHowQueryNode(rte->subquery);
		correctRTEAlias(rte);
	}

	/* adapt colnames, coltypes for the SetOperationStmt nodes */
	adaptSetOps(query->setOperations);

	/* add provenance attribute */
	rte = (RangeTblEntry *) linitial(query->rtable);
	var = makeVar(1, list_length(rte->subquery->targetList), HOWPROVOID, -1, 0);
	createHowAttr(query, (Node *) var);
}

/*
 *
 */

static void
adaptSetOps (Node *setOp)
{
	if (IsA(setOp, SetOperationStmt))
	{
		SetOperationStmt *set = (SetOperationStmt *) setOp;

		set->all = true;
		set->colTypes = lappend_oid(set->colTypes, HOWPROVOID);
		set->colTypmods = lappend_int(set->colTypmods, -1);
		adaptSetOps(set->larg);
		adaptSetOps(set->rarg);
	}
}

/*
 *
 */

static void
rewriteHowIntersect(Query *query)
{
	//TODO
}
