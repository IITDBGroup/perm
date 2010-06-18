/*-------------------------------------------------------------------------
 *
 * prov_copy_set.h
 *		 :	interface to the copy contribution semantics rewrites for set operations.
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_copy_set.h,v 1.29 23.06.2009 11:36:43 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_COPY_SET_H_
#define PROV_COPY_SET_H_

#include "nodes/parsenodes.h"

Query *rewriteCopySetQuery (Query *query);

#endif /* PROV_COPY_SET_H_ */
