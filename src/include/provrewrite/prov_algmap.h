/*-------------------------------------------------------------------------
 *
 * prov_algmap.h
 *		 : Interface for helper methods to map a query tree part to its equivalent algebra expression.
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_algmap.h,v 1.29 26.08.2009 11:14:45 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_ALGMAP_H_
#define PROV_ALGMAP_H_

extern bool isProjection (Query *query);
extern bool isProjectionUnderAgg (Query *query);
extern bool isProjectionOverAgg (Query *query);
extern bool isVarRenamed (Query *query, TargetEntry *te, Var *var);

#endif /* PROV_ALGMAP_H_ */
