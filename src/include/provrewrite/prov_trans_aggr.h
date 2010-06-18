/*-------------------------------------------------------------------------
 *
 * prov_trans_aggr.h
 *		 : Interface to transformation provenance rewrites for aggregation queries.
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_trans_aggr.h,v 1.29 03.09.2009 09:51:20 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_TRANS_AGGR_H_
#define PROV_TRANS_AGGR_H_

#include "nodes/parsenodes.h"

extern Query *rewriteTransAgg (Query *query, Node **parentPointer);


#endif /* PROV_TRANS_AGGR_H_ */
