/*-------------------------------------------------------------------------
 *
 * prov_sqj.h
 *		External interface to provenance spj query rewriting
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql/src/include/provrewrite/prov_spj.h,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_SPJ_H_
#define PROV_SPJ_H_

#include "nodes/parsenodes.h"

extern Query *rewriteSPJQuery (Query *query);
extern Query *rewriteSPJrestrict (Query *query, List **subList, Index maxRtindex);
extern List *rewriteRTEs (Query *query, List **subList, Index maxRtindex);

extern void rewriteRTEwithProvenance (int rtindex, RangeTblEntry *rte);
extern void rewriteDistinctClause (Query *query);
extern void correctJoinRTEs (Query *query, List **subList);
extern bool checkLimit (Query *query);
extern Query *handleLimit (Query *query);

#endif /*PROV_SPJ_H_*/
