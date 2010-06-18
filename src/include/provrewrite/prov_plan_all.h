/*-------------------------------------------------------------------------
 *
 * prov_plan_all.h
 *		External interface to provenance exhaustive optimization for rewrites.
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql/src/include/provrewrite/prov_plan_all.h,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_PLAN_ALL_H_
#define PROV_PLAN_ALL_H_

#include "nodes/parsenodes.h"
#include "nodes/params.h"
#include "nodes/plannodes.h"

extern PlannedStmt *generateCheapestProvenancePlan (Query *query, int cursorOptions, ParamListInfo boundParams);
extern Query *generateCheapestPlanQuery (Query *query, int cursorOptions, ParamListInfo boundParams);

#endif /* PROV_PLAN_ALL_H_ */
