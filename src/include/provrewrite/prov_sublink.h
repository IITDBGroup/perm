/*-------------------------------------------------------------------------
 *
 * prov_sublink.h
 *		External interface to provenance sublink handling
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql/src/include/provrewrite/prov_sublink.h,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_SUBLINK_H_
#define PROV_SUBLINK_H_

#include "utils/rel.h"
#include "nodes/parsenodes.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_sublink_util_search.h"
#include "provrewrite/prov_sublink_util_mutate.h"
#include "provrewrite/prov_sublink_util_analyze.h"

extern Query *rewriteSublinks (Query *query, List **subList);
extern void rewriteSublink (Query *query, SublinkInfo *info, Index subList[], List **rewritePos);
extern void createSubList (Index subPos[], List **subList, List *rewritePos);

#endif /*PROV_SUBLINK_H_*/
