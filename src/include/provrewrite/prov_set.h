/*-------------------------------------------------------------------------
 *
 * prov_set.h
 *		External interface to provenance set query rewriting
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql/src/include/provrewrite/prov_set.h,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_SET_H_
#define PROV_SET_H_

#include "nodes/parsenodes.h"

extern Query *rewriteSetQuery (Query *query);

#endif /*PROV_SET_H_*/
