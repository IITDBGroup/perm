/*-------------------------------------------------------------------------
 *
 * prov_where_aggfunc.h
 *		External interface to 
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql//SF_Perm/src/include/provrewrite/prov_where_aggfunc.h ,v 1.29 Oct 6, 2010 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_WHERE_AGGFUNC_H_
#define PROV_WHERE_AGGFUNC_H_

#include "fmgr.h"

extern Datum textarray_unique_concat(PG_FUNCTION_ARGS);

#endif /* PROV_WHERE_AGGFUNC_H_ */
