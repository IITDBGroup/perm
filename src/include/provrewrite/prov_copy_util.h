/*-------------------------------------------------------------------------
 *
 * prov_copy_util.h
 *		External interface to utility functions for copy contribution semantics
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql/src/include/provrewrite/prov_copy_spj.h,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_COPY_UTIL_H_
#define PROV_COPY_UTIL_H_

#include "nodes/parsenodes.h"

extern Node *conditionOnRteVarsMutator (Node *node, Query *context);
extern List *getRteVarsForJoin (Query *query, JoinExpr *join);
extern List *getCopyRelsForRtindex (Query *query, Index rtindex);
extern List *copyAddProvAttrForNonRewritten (Query *query);
extern void copyAddProvAttrs (Query *query, List *subList);

#endif /*PROV_COPY_UTIL_H_*/
