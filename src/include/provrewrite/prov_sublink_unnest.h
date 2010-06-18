/*-------------------------------------------------------------------------
 *
 * prov_sublink_unnest.h
 *		 : Unnest correlated sublinks for provenance rewritting
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_sublink_unnest.h,v 1.29 21.10.2008 11:23:42 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_SUBLINK_UNNEST_H_
#define PROV_SUBLINK_UNNEST_H_

#include "nodes/parsenodes.h"
#include "provrewrite/prov_sublink_util_search.h"
#include "provrewrite/prov_sublink_util_mutate.h"
#include "provrewrite/prov_sublink_util_analyze.h"

/*
 *
 */
typedef struct FindSubQueryWalker
{
	Query *result;
	Query *searchQuery;
} FindSubQueryWalker;

extern bool checkJAstrategyPreconditions (SublinkInfo *info);
extern bool checkEXISTSstrategyPreconditions (SublinkInfo *info);
extern Query *rewriteJAstrategy (Query *query, SublinkInfo *info, Index subList[]);
extern Query *rewriteEXISTSstrategy (Query *query, SublinkInfo *info, Index subList[]);

#endif /*PROV_SUBLINK_TOTARGET_H_*/
