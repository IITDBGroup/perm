/*-------------------------------------------------------------------------
 *
 * prov_restr_scope.h
 *		 :
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_restr_scope.h,v 1.29 03.02.2009 13:50:50 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_RESTR_SCOPE_H_
#define PROV_RESTR_SCOPE_H_

#include "provrewrite/prov_nodes.h"

/* consts */
#define RTINDEX_TOPQUAL 0

/* prototypes */
void generateQueryPushdownInfos (Query *query);
void mergeScopes (SelScope *left, SelScope *right);

#endif /* PROV_RESTR_SCOPE_H_ */
