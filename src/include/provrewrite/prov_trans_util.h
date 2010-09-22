/*-------------------------------------------------------------------------
 *
 * prov_trans_util.h
 *		 : Support functions and macros used by transformation provenance rewrite code.
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_trans_util.h,v 1.29 03.09.2009 10:24:41 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_TRANS_UTIL_H_
#define PROV_TRANS_UTIL_H_

#include "provrewrite/prov_nodes.h"
#include "utils/fmgroids.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "utils/datum.h"

/* types */
typedef enum TransProjType
{
	belongToTop,
	SingleOp,
	ProjUnderAgg,
	ProjOverAgg,
	ProjBothAgg,
	None
} TransProjType;

/* macros */
#define MAKE_TRANS_PROV_ATTR(query,bitComp) \
		(makeTargetEntry( \
			((Expr *) bitComp), \
			list_length(query->targetList) + 1, \
			appendIdToString("trans_prov", &curUniqueAttrNum), \
			false))

#define MAKE_VARBIT_CONST(varbitset) \
	(makeRelabelType((Expr *) makeConst(BITOID, -1, -1, datumCopy(varbitset, false, -1), false, false), VARBITOID, -1, COERCE_EXPLICIT_CAST))

#define MAKE_EMPTY_BITSET_CONST(length) \
	(MAKE_VARBIT_CONST(VarBitPGetDatum(generateEmptyBitset(length))))

#define COPY_VARBIT(input) \
		(DatumGetVarBitP(datumCopy(((Datum) input), false, -1)))

#define MAKE_SETOR_FUNC(args) \
	(makeFuncExpr(F_BITOR_WITH_NULL, VARBITOID, args, COERCE_EXPLICIT_CALL))

#define MAKE_SETOR_FUNC_NO_NULL(args) \
	(makeFuncExpr(F_BITOR, VARBITOID, args, COERCE_EXPLICIT_CALL))

#define MAKE_SETCONT_FUNC(args) \
	(makeFuncExpr(F_BITSET_CONTAINS, BOOLOID, args, COERCE_EXPLICIT_CALL))

#define MAKE_SETNEQ_FUNC(argl,argr) \
	(makeBoolExpr(NOT_EXPR, list_make1(makeFuncExpr(F_BITEQ, BOOLOID, list_make2(argl, argr), COERCE_EXPLICIT_CALL))))

#define MAKE_SETREPEAT_FUNC(argl,argr) \
	(makeFuncExpr(F_BITSET_NONZERO_REPEAT, BOOLOID, list_make2(argl, argr), COERCE_EXPLICIT_CALL))

#define TSET_LARG(sub) \
	((TransSubInfo *) (((TransSubInfo *) sub)->children->head->data.ptr_value))

#define TSET_RARG(sub) \
	((TransSubInfo *) (((TransSubInfo *) sub)->children->head->next->data.ptr_value))

/* functions */
extern TransSubInfo *getRootSubForNode (TransProvInfo *info);
extern void addStaticTransProvAttr (Query *query);
extern Node **getParentPointer (TransProvInfo *parent, TransProvInfo *sub, Node **current);
extern Node *getSubqueryTransProvAttr (Query *query, Index rtIndex);
extern Node *getRealSubqueryTransProvAttr (Query *query, Index rtIndex);
extern Node *getSubqueryTransProvUnionSet (Query *query, Index rtIndex, Datum bitset);
extern Node *createBitorExpr (List *args);
extern TransSubInfo *getSpecificInfo (TransSubInfo *cur, SubOperationType type);
extern TransSubInfo *getTopJoinInfo (Query *query, bool hasCross);
extern TransProjType getProjectionType (Query *query, bool *underHaving);


#endif /* PROV_TRANS_UTIL_H_ */
