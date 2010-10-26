/*-------------------------------------------------------------------------
 *
 * prov_trans_map.h
 *		 :
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_trans_map.h,v 1.29 26.10.2009 18:37:55 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_TRANS_MAP_H_
#define PROV_TRANS_MAP_H_

#include "nodes/parsenodes.h"
#include "provrewrite/prov_trans_bitset.h"

extern void generateMapString (Query *query, TransRepQueryInfo *repInfo,
		MemoryContext funcPrivateCntx);

#endif /* PROV_TRANS_MAP_H_ */
