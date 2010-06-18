/*-------------------------------------------------------------------------
 *
 * prov_restr_util.h
 *		 :
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_restr_util.h,v 1.29 06.01.2009 11:34:03 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_RESTR_UTIL_H_
#define PROV_RESTR_UTIL_H_

#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_restr_rewrite.h"

/*macros */
#define getLeftSelOp(selinfo) \
	(getOpSelInfoOperand (((SelectionInfo *) ( selinfo )), 0))

#define getRightSelOp(selinfo) \
	(getOpSelInfoOperand (((SelectionInfo *) ( selinfo )), 1))

/* prototypes */
extern void recreateSelFromExprs (SelScope *scope);
extern List *createSelectionInfoList (Node *expr, SelScope *scope, Query *query, Index rtIndex);
extern SelectionInfo * createSelectionInfo (Node *expr, Index rtIndex, bool derived);
extern void createEquivalenceLists (SelScope *scope);
extern bool isEquivOverSimpleArgs (SelectionInfo *sel);
extern bool isInequalOverSimpleArgs (SelectionInfo *sel);
extern bool selIsInequality (SelectionInfo *sel);
extern ComparisonType getTypeForIneq (SelectionInfo *sel);
extern Node *createQual (SelScope *scope, bool includeRedundend);
//extern void setVolatileAndSubsRedundend (PushdownInfo *info);
extern void addExprToScopeWithRewrite (SelScope *scope, Node *expr, Query *query, ExprHandlers *handlers, Index rtIndex);
extern void addExprToScope (SelScope *scope, Node *expr, Query *query, Index rtIndex);
extern EquivalenceList *getEqualityListForExpr (SelScope *info, Node *expr);
extern Node *getOpSelInfoOperand (SelectionInfo *sel, Index opIndex);
extern List *getEquivalenceListConsts (EquivalenceList *equi);
extern void computeEquivalenceListsConsts (SelScope *pushdown);
#endif /* PROV_RESTR_UTIL_H_ */
