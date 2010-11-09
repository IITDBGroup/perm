/*-------------------------------------------------------------------------
 *
 * prov_where_set.h
 *		External interface to 
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql//SF_Perm/src/include/provrewrite/prov_where_set.h ,v 1.29 Oct 4, 2010 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_WHERE_SET_H_
#define PROV_WHERE_SET_H_

#include "nodes/parsenodes.h"

extern List *rewriteSetWhere (Query *query);
extern List *rewriteWhereInSetQuery (Query *query);

#endif /* PROV_WHERE_SET_H_ */
