/*-------------------------------------------------------------------------
 *
 * prov_corsublink.h
 *		External interface to provenance correlated sublink rewriting
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql/src/include/provrewrite/prov_corsublink.h,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_CORSUBLINK_H_
#define PROV_CORSUBLINK_H_

#include "nodes/parsenodes.h"

extern Query *rewriteCorSublinkQuery (Query *query);

#endif /*PROV_CORSUBLINK_H_*/
