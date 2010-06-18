/*-------------------------------------------------------------------------
 *
 * prov_restr_final.h
 *		 :
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_restr_final.h,v 1.29 05.02.2009 17:53:05 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_RESTR_FINAL_H_
#define PROV_RESTR_FINAL_H_

#include "nodes/parsenodes.h"

void finalizeSelections (Query *query);

#endif /* PROV_RESTR_FINAL_H_ */
