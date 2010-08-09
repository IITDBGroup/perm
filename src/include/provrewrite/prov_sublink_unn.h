/*-------------------------------------------------------------------------
 *
 * prov_sublink_unn.h
 *		 :
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_sublink_unn.h,v 1.29 27.10.2008 16:13:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_SUBLINK_UNN_H_
#define PROV_SUBLINK_UNN_H_

#include "nodes/parsenodes.h"
#include "provrewrite/prov_sublink_util_search.h"
#include "provrewrite/prov_sublink_util_mutate.h"
#include "provrewrite/prov_sublink_util_analyze.h"

extern bool checkUnnStrategyPreconditions (SublinkInfo *info, Query *query);
extern Query *rewriteUnnStrategy (Query *query, SublinkInfo *info, Index subList[], List *infos);
extern bool checkUnnNotStrategyPreconditions (SublinkInfo *info, Query *query);
extern Query *rewriteUnnNotStrategy (Query *query, SublinkInfo *info, Index subList[], List *infos);

#endif /* PROV_SUBLINK_UNN_H_ */
