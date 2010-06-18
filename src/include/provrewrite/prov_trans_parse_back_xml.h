/*-------------------------------------------------------------------------
 *
 * prov_trans_parse_back_xml.h
 *		 :
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_trans_parse_back_xml.h,v 1.29 21.09.2009 10:42:31 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_TRANS_PARSE_BACK_XML_H_
#define PROV_TRANS_PARSE_BACK_XML_H_

#include "provrewrite/prov_trans_bitset.h"
#include "nodes/parsenodes.h"

extern StringInfo parseBackTransToXML (Query *query, TransRepQueryInfo *newInfo, bool simple, MemoryContext funcPrivateCtx);

#endif /* PROV_TRANS_PARSE_BACK_XML_H_ */
