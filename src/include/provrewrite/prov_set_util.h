/*-------------------------------------------------------------------------
 *
 * prov_set_util.h
 *		External interface to 
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql//SF_Perm/src/include/provrewrite/prov_set_util.h ,v 1.29 2012-03-20 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_SET_UTIL_H_
#define PROV_SET_UTIL_H_

#include "nodes/parsenodes.h"

extern void removeDummyRewriterRTEs (Query *query);
extern void findSetOpRTEs (List *rtable, Node *setTreeNode, List **rtes, List **rtindex);
extern List *getSetOpRTRefs (Node *setTreeNode);
extern void replaceSetOperationSubTrees (Query *query, Node *node,
		Node **parentPointer, SetOperation rootType);
extern void createSetJoinCondition (Query *query, JoinExpr *join, Index leftIndex, Index rightIndex, bool neq);
extern void adaptSetStmtCols (SetOperationStmt *stmt, List *colTypes, List *colTypmods);

#endif /* PROV_SET_UTIL_H_ */
