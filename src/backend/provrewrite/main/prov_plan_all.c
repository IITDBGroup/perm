/*-------------------------------------------------------------------------
 *
 * prov_plan_all.c
 *	  PERM C -  Generates rewritten Query trees for all applicable rewrite strategies, uses the optimizer to get a
 *	  			cost estimation for each one and executes the cheapest one.
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/prov_plan_all.c,v 1.322 2008/01/09 08:46:44 bglavic $
 *
 * NOTES
 *		Current implementation generates simply activates and deactivates each rewrite strategy, calls the
 *		the perm module to apply each on/off combination and each generated plan is passed to the optimizer.
 *		The estimated cost of computed by the optimizer for each plan is used to rank these plans. The cheapest
 *		plan is then choosen for execution (so actually we have to store only the current cheapest plan).
 *
 *-------------------------------------------------------------------------
 */

#include <stdlib.h>
#include "postgres.h"
#include "nodes/plannodes.h"
#include "nodes/parsenodes.h"
#include "optimizer/planner.h"
#include "utils/guc.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/prov_plan_all.h"
#include "provrewrite/provattrname.h"
#include "provrewrite/provstack.h"
#include "provrewrite/provlog.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_nodes.h"

/* Macros */
#define NUM_OPTIONS 32
#define STOP_PLANING_THRESHHOLD -1

/* Function declarations */
static void generateCheapestQueryAndPlan (Query *query, int cursorOptions,
										ParamListInfo boundParams, Query **cheapestQuery,
										PlannedStmt **cheapestPlan);
static void setOptions(int flags);
static bool checkIfRewriteNeeded (Query *query);

/*
 * Generate rewritten queries for applicable rewrite methods, plan them to estimate
 * their costs and return the plan for the one that's supposed to be the cheapest.
 */
/*TODO we could add a basic lower bound for the costs where it does not make
 * sense to try other rewrites because planning them would cost more time than
 * just executing the current plan (Have to measure plan times then to know when
 * to apply this pruning)
 */
PlannedStmt *
generateCheapestProvenancePlan (Query *query, int cursorOptions, ParamListInfo boundParams)
{
	PlannedStmt *cheapestPlan;
	Query *cheapestQuery;

	cheapestPlan = NULL;
	cheapestQuery = NULL;

	generateCheapestQueryAndPlan (query, cursorOptions, boundParams, &cheapestQuery, &cheapestPlan);

	return cheapestPlan;
}

/*
 * Generate the cheapest plan and return the rewritten query that was used to generate this plan.
 * This is needed for view definition (There we need a query and not a plan).
 */

Query *
generateCheapestPlanQuery (Query *query, int cursorOptions, ParamListInfo boundParams) {
	PlannedStmt *cheapestPlan;
	Query *cheapestQuery;

	cheapestPlan = NULL;
	cheapestQuery = NULL;

	generateCheapestQueryAndPlan (query, cursorOptions, boundParams, &cheapestQuery, &cheapestPlan);

	return cheapestQuery;
}

/*
 *
 */
static void
generateCheapestQueryAndPlan (Query *query, int cursorOptions, ParamListInfo boundParams, Query **cheapestQuery, PlannedStmt **cheapestPlan)
{
	Query *rewrittenQuery;
	PlannedStmt *curPlan;
	Cost minCost = -1.0;
	int i;

	/* check if query contains provenance queries */
	if (checkIfRewriteNeeded(query))
		*cheapestPlan = standard_planner(query, cursorOptions, boundParams);

	/* try all applicable rewrite methods */
	*cheapestPlan = NULL;

	LOGNOTICE("-------------- optimize statement");
	for(i = 0; i < NUM_OPTIONS; i++)
	{
		logNotice("--next plan");
		/* rewrite Query */
		setOptions(i);
		rewrittenQuery = copyObject(query);
		rewrittenQuery = provenanceRewriteQuery(rewrittenQuery);

		/* plan current rewrite. If the resulting plan has lower estimated cost than the
		 * cheaptest Plan we have seen until now, the current plan is the new cheapest plan.
		 */
		curPlan = standard_planner(copyObject(rewrittenQuery), cursorOptions, boundParams);
		if (!(*cheapestPlan) || curPlan->planTree->total_cost < minCost)
		{
			LOGNOTICE("------- is cheapest");
			if (*cheapestPlan)
				pfree(*cheapestPlan);

			*cheapestPlan = curPlan;
			minCost = (*cheapestPlan)->planTree->total_cost;
			((ProvInfo *) query->provInfo)->rewriteInfo = copyObject(rewriteMethodStack);
			*cheapestQuery = rewrittenQuery;
			((ProvInfo *) (*cheapestQuery)->provInfo)->rewriteInfo = copyObject(rewriteMethodStack);
		}
		else
		{
			pfree(curPlan);
			pfree(rewrittenQuery);
		}

		if (minCost < STOP_PLANING_THRESHHOLD)
			break;
	}
}

/*
 * Checks if the query contains parts that are marked for provenance rewrite.
 */

static bool
checkIfRewriteNeeded (Query *query)
{
	/* ignore Utility statements */
	if (query->commandType != CMD_UTILITY)
		return false;

	if (!queryHasRewriteChildren(query))
		return false;

	/* check if query contains provenance queries */
	return true;
}

/*
 *	Sets the provenance optimization options according to the value of flags.
 *	Each bit in flags determines if one of the options is set to true or to false.
 */
static void
setOptions(int flags)
{
	prov_use_set_optimization = flags & 0x01;
	prov_use_sublink_optimization_left_join = flags & 0x02;
	prov_use_sublink_move_to_target = flags & 0x04;
	prov_use_sublink_transfrom_top_level_any_to_join = flags & 0x08;
	prov_use_unnest_JA = flags & 0x10;
}
