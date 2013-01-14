/*-------------------------------------------------------------------------
 *
 * prov_where_main.h
 *		External interface to 
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql//SF_Perm/src/include/provrewrite/prov_where_main.h ,v 1.29 Oct 4, 2010 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_WHERE_MAIN_H_
#define PROV_WHERE_MAIN_H_

#include "nodes/parsenodes.h"

extern Query *rewriteQueryWhere (Query *query);
extern Query *rewriteQueryWhereInSen (Query *query);
extern Query *rewriteQueryWhereInSenNoUnion (Query *query);


#endif /* PROV_WHERE_MAIN_H_ */
