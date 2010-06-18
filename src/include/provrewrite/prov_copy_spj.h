/*-------------------------------------------------------------------------
 *
 * prov_copy_sqj.h
 *		External interface to provenance spj query rewriting with copy contribution semantics
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql/src/include/provrewrite/prov_copy_spj.h,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_COPY_SPJ_H_
#define PROV_COPY_SPJ_H_

#include "nodes/parsenodes.h"

extern Query *rewriteSPJQueryCopy (Query *query);
extern void rewriteRTEsCopy (Query *query, List **subList, Index maxRtindex);


#endif /*PROV_COPY_SPJ_H_*/
