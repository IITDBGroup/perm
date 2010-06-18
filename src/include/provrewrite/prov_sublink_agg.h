/*-------------------------------------------------------------------------
 *
 * prov_sublink_agg.h
 *		 :Interface for module that normalizes aggregation queries with sublinks in HAVING, GROUP BY or target list
 *		 into a query without these kinds of sublinks in the aggregation.
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_sublink_agg.h,v 1.29 14.08.2008 16:57:05 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_SUBLINK_AGG_H_
#define PROV_SUBLINK_AGG_H_

#include "provrewrite/prov_sublink_util_search.h"
#include "provrewrite/prov_sublink_util_mutate.h"
#include "provrewrite/prov_sublink_util_analyze.h"

/*
 * Context structure for the replace params mutator
 */
typedef struct GetLocationTypeContext
{
	SublinkInfo *info;
	bool inAggrOrGroupBy;
	List *groupByExprs;
} GetLocationTypeContext;

/*
 * Context structure for the mutator that replaces sublevels up nodes in an aggregation sublink.
 */
typedef struct SublevelsUpMutatorContext
{
	SublinkInfo *info;
} SublevelsUpMutatorContext;

/*
 * Context structure for the mutator that replaces sublevels up nodes in an aggregation sublink.
 */
typedef struct AggOutputAndGroupByWalkerContext
{
	List **result;
	int nestDepth;
	bool inAgg;
	bool justSeenSublink;
	bool inSublink;
} AggOutputAndGroupByWalkerContext;

/*
 */
typedef struct ReplaceSubExprTopMutatorContext
{
	List *replaceExprs;
	List *mapExprTarget;
	int nestDepth;
	bool justSeenSublink;
	bool inSublink;
} ReplaceSubExprTopMutatorContext;

/*
 */
typedef struct CheckVarLevelsWalkerContext
{
	bool result;
	int nestDepth;
} CheckVarLevelsWalkerContext;




/* prototypes */
extern bool hasHavingOrTargetListOrGroupBySublinks (Query *query);
extern bool hasNonHavingSublink (Query *query);
extern Query *transformAggWithSublinks (Query *query);

#endif	/*PROV_SUBLINK_AGG_H_*/
