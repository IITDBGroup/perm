/*-------------------------------------------------------------------------
 *
 * prov_sublink_totarget.h
 *		External interface to sublink rewrite
 * 		that moves sublinks to the target list of a query
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql/src/include/provrewrite/prov_sublink_totarget.h,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_SUBLINK_TOTARGET_H_
#define PROV_SUBLINK_TOTARGET_H_

#include "nodes/parsenodes.h"
#include "provrewrite/prov_sublink_util_search.h"
#include "provrewrite/prov_sublink_util_mutate.h"
#include "provrewrite/prov_sublink_util_analyze.h"

/*
 *
 */

typedef struct BelowLeftJoinContext
{
	SublinkInfo *sublink;
	bool result;
} BelowLeftJoinContext;

/*
 * Find sublinks from infos list and vars context.
 */

typedef struct VarAndSublinkWalkerContext
{
	List *infos;
	List **result;
	int maxVarlevelsUp;
} VarAndSublinkWalkerContext;

/*
 * Find sublinks from infos list and vars context.
 */

typedef struct AdaptReferencesMutatorContext
{
	List *sublinksAndVars;
	List *map;
} AdaptReferencesMutatorContext;


/*
 *
 */

typedef struct AdaptTestExpressionMutatorContext
{
	List *sublinksAndVars;
	List *map;
} AdaptTestExpressionMutatorContext;



/* functions */
extern Query *rewriteSublinkQueryWithMoveToTarget (Query *query, List *sublinks, List *uncorrelated,  Index subList[], List **rewritePos);

#endif /*PROV_SUBLINK_TOTARGET_H_*/
