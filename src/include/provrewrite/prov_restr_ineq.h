/*-------------------------------------------------------------------------
 *
 * prov_restr_ineq.h
 *		 :	External interface to provenance inequality graph of expression equivalence classes
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_restr_ineq.h,v 1.29 08.01.2009 17:44:38 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_RESTR_INEQ_H_
#define PROV_RESTR_INEQ_H_

#include "provrewrite/prov_nodes.h"

/* prototypes */
extern List *getSimpleInequalities (PushdownInfo *info);
extern InequalityGraph *computeInequalityGraph (PushdownInfo *info);
extern InequalityGraph *computeTransitiveClosure (InequalityGraph *graph);
extern InequalityGraph *minimizeInEqualityGraph (PushdownInfo *info, InequalityGraph *graph);
extern void generateInequalitiesFromGraph (PushdownInfo *info, InequalityGraph *graph);

#endif /* PROV_RESTR_INEQ_H_ */
