/*-------------------------------------------------------------------------
 *
 * parse_back_oracle.h
 *		External interface to 
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql//SF_Perm/src/include/provrewrite/parse_back_oracle.h ,v 1.29 Mar 27, 2014 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PARSE_BACK_ORACLE_H_
#define PARSE_BACK_ORACLE_H_

#include "nodes/parsenodes.h"
#include "lib/stringinfo.h"

StringInfo parseBackOracle(Query *query, bool pretty);

#endif /* PARSE_BACK_ORACLE_H_ */
