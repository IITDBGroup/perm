/*-------------------------------------------------------------------------
 *
 * prov_restr_pushdown.h
 *		 :
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_restr_pushdown.h,v 1.29 08.12.2008 18:03:01 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_RESTR_PUSHDOWN_H_
#define PROV_RESTR_PUSHDOWN_H_

#include "nodes/parsenodes.h"

extern Query *pushdownSelections(Query *query);

#endif /* PROV_RESTR_PUSHDOWN_H_ */
