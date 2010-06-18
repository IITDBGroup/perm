/*-------------------------------------------------------------------------
 *
 * parse_back_db2.h
 *		 : interface to functions for parsing back a query tree into a format readable by DB2.
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/parse_back_db2.h,v 1.29 21.07.2009 21:46:56 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PARSE_BACK_DB2_H_
#define PARSE_BACK_DB2_H_

#include "nodes/parsenodes.h"
#include "lib/stringinfo.h"

StringInfo parseBackDB2(Query *query, bool pretty);

#endif /* PARSE_BACK_DB2_H_ */
