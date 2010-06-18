/*-------------------------------------------------------------------------
 *
 * prov_copy_equi.h
 *		 :
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_copy_equi.h,v 1.29 14.11.2008 09:00:54 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_COPY_EQUI_H_
#define PROV_COPY_EQUI_H_

#define GET_NUM_COMPONENTS (graph) \
	(list_length(((EqGraph *) (graph))->components))

#define HAS_EDGE (graph, nodeA, nodeB) \
	(((EqGraph *) (graph)) edgeConditions[nodeA][nodeB] != NULL)

typedef struct EqGraph {
	//int nodes[];
	//Node[][] *edgeConditions;
	List *components;
} EqGraph;

extern List *getEquivFromExpr (Node *expr);
extern EqGraph *computeEqGraph (Query *query);
extern List *getComponentAttrs (EqGraph *graph, Var *attr);



#endif /* PROV_COPY_EQUI_H_ */
