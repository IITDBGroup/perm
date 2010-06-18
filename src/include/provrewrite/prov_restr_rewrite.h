/*-------------------------------------------------------------------------
 *
 * prov_restr_rewrite.h
 *		 :
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_restr_rewrite.h,v 1.29 06.01.2009 11:33:39 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_RESTR_REWRITE_H_
#define PROV_RESTR_REWRITE_H_

typedef enum PushdownType
{
	PUSHDOWN_JOIN,
	PUSHDOWN_SUBQUERY,
	PUSHDOWN_RELATION,
	PUSHDOWN_AGG,	//CHECK if needed
	PUSHDOWN_OTHER
} PushdownType;

typedef struct RestricterInfo //TODO what is useful and should be included here
{
	List *rtIndexes;
	Index rtindex;
	PushdownType type;
	Query *query;
	RangeTblEntry *rte;
	EquivalenceList **equiMap;
} RestricterInfo;

typedef struct PushableWalkerContext
{
	bool *result;
	List *rtes;
} RestrictableWalkerContext;

typedef struct SelectionPushableWalkerContext
{
	bool *result;
	List *equis;
	RestricterInfo *restrInfo;
} SelectionPushableWalkerContext;

typedef struct AdaptSelectionMutatorContext
{
	List *inputEquis;
	List *outputEquis;
	RestricterInfo *restrInfo;
} AdaptSelectionMutatorContext;

struct ExprHandlers;

/* define rewrite function pointers */
typedef void (*ExprRewriter) (PushdownInfo *pushdown, struct ExprHandlers *handlers);
typedef PushdownInfo *(*ExprPusher) (PushdownInfo *pushdown, Query *query, struct ExprHandlers *handlers);
typedef void (*ExprRestricter) (PushdownInfo *input, PushdownInfo *output, RestricterInfo *restrInfo, struct ExprHandlers *handlers);

typedef struct ExprHandlers {
	List *rewriters;
	List *restricters;
	ExprPusher setPusher;
	ExprPusher joinPusher;
	ExprPusher havingPusher;
} ExprHandlers;

extern void rewriteExpr (PushdownInfo *info, ExprHandlers *handlers);
extern PushdownInfo *restrictExpr (PushdownInfo *info, Query *query, Index rtindex, ExprHandlers *handlers);
extern List *getRewriters (void);
extern List *getRestricters (void);

#endif /* PROV_RESTR_REWRITE_H_ */
