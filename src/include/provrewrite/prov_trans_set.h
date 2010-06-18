/*-------------------------------------------------------------------------
 *
 * prov_trans_set.h
 *		 : Interface to transformation provenance rewrites for set operations
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_trans_set.h,v 1.29 02.09.2009 17:25:16 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_TRANS_SET_H_
#define PROV_TRANS_SET_H_

#include "nodes/parsenodes.h"

extern Query *rewriteTransSet (Query *query, Node **parent, RangeTblEntry *queryRte);
extern Query *rewriteStaticSetOp (Query *query, Node **parentInfo);

#endif /* PROV_TRANS_SET_H_ */
