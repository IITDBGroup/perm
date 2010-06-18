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

extern List *copyAddProvAttrsForSet (Query *query, List *subList, List *pList);
extern List *copyAddProvAttrForNonRewritten (Query *query);
extern List *copyAddProvAttrs (Query *query, List *subList, List *pList);

#endif /*PROV_COPY_UTIL_H_*/
