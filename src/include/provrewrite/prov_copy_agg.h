/*-------------------------------------------------------------------------
 *
 * prov_copy_agg.h
 *		 :
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_copy_agg.h,v 1.29 23.06.2009 11:36:55 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_COPY_AGG_H_
#define PROV_COPY_AGG_H_

#include "nodes/parsenodes.h"

extern Query *rewriteCopyAggregateQuery (Query *query);

#endif /* PROV_COPY_AGG_H_ */
