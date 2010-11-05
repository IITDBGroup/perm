/*-------------------------------------------------------------------------
 *
 * prov_where_util.h
 *		External interface to 
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql//SF_Perm/src/include/provrewrite/prov_where_util.h ,v 1.29 Oct 28, 2010 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_WHERE_UTIL_H_
#define PROV_WHERE_UTIL_H_

#include "nodes/parsenodes.h"

/* data types */
typedef struct FetchVarsContext
{
	int rtindex;
	List **result;
} FetchVarsContext;

/* function declarations */
extern Query *pullUpSubqueries (Query *query);

#endif /* PROV_WHERE_UTIL_H_ */
