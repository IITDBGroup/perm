/*-------------------------------------------------------------------------
 *
 * prov_aggr.h
 *		External interface to provenance aggregation query rewriting
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql/src/include/provrewrite/prov_aggr.h,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_AGGR_H_
#define PROV_AGGR_H_

#include "nodes/parsenodes.h"

extern Query *rewriteAggregateQuery (Query *query);
extern bool isAggrExpr (Node *node);
extern void addSubqueryTargetListToTargetList (Query *query, Index rtindex);
extern void addJoinOnAttributes (Query *query, List *joinAttrsLeft, Query *rightSub);
extern List *rewriteAggrSubqueryForRewrite (Query *query, bool returnMapping);
extern void checkOrderClause (Query *newTop, Query *query);

#endif /*PROV_AGGR_H_*/
