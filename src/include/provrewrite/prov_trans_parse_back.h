/*-------------------------------------------------------------------------
 *
 * prov_trans_parse_back.h
 *		 :
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_trans_parse_back.h,v 1.29 09.09.2009 10:52:12 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_TRANS_PARSE_BACK_H_
#define PROV_TRANS_PARSE_BACK_H_

#include "utils/palloc.h"
#include "nodes/parsenodes.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_trans_bitset.h"

/* types */
typedef struct TransParseRange
{
	VarBit *set;
	int begin;
	int end;
} TransParseRange;

/* macros */
#define MAKE_RANGE(range,node) \
	do { \
		range = (TransParseRange *) palloc(sizeof(TransParseRange)); \
		range->begin = str->len; \
		range->set = COPY_VARBIT(((TransSubInfo *) node)->setForNode); \
		*(context->ranges) = lappend(*context->ranges, range); \
	} while (0)

/* functions */
extern void parseBackTransToSQL (Query *query, TransRepQueryInfo *repInfo, MemoryContext funcPrivateCtx);
extern void postprocessRanges (List *ranges, StringInfo str, TransRepQueryInfo *repInfo, MemoryContext funcPrivateCtx);


#endif /* PROV_TRANS_PARSE_BACK_H_ */
