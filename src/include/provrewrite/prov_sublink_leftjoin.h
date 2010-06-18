/*-------------------------------------------------------------------------
 *
 * prov_sublink_leftjoin.h
 *		External interface to provenance sublink rewrite variant that propagates the provenance
 * 		of sublinks using left join operators.
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql/src/include/provrewrite/prov_sublink_util.h,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_SUBLINK_LEFTJOIN_H_
#define PROV_SUBLINK_LEFTJOIN_H_

#include "nodes/parsenodes.h"
#include "provrewrite/prov_sublink_util_search.h"
#include "provrewrite/prov_sublink_util_mutate.h"
#include "provrewrite/prov_sublink_util_analyze.h"

extern Query *rewriteSublinkUsingLeftJoin (Query *query, SublinkInfo *info, Index subList[]);
extern Query *rewriteTargetSublinkUsingLeftJoin (Query *newTop, Query *query, SublinkInfo *info, Index subList[]);

#endif /* PROV_SUBLINK_LEFTJOIN_H_ */
