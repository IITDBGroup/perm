/*-------------------------------------------------------------------------
 *
 * prov_sublink_util_analyze.h
 *		External interface to sublink analyze utility functions.
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql//perm/src/include/provrewrite/prov_sublink_util_analyze.h ,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_SUBLINK_UTIL_ANALYZE_H_
#define PROV_SUBLINK_UTIL_ANALYZE_H_

#include "utils/rel.h"
#include "nodes/nodes.h"
#include "nodes/parsenodes.h"
#include "provrewrite/prov_nodes.h"

/*
 * Structs used as context data structures for walker methods.
 */

/* */
typedef struct InsideTestWalkerContext
{
        bool *result;
        bool inSub;
        SubLink *search;
} InsideTestWalkerContext;

/* */
typedef struct CorrelatedVarsWalkerContext
{
        bool result;
        int varlevelsUp;
} CorrelatedVarsWalkerContext;

/* */
typedef struct PathToExprWalkerContext
{
        List *path;
        List **resultPath;
        Node *node;
} PathToExprWalkerContext;

/* Analyze functions */
extern bool sublinkHasSublinksInTestOrInTest (SublinkInfo *info);
extern bool isTopLevelSublink(SublinkInfo *info);
extern SublinkCat getSublinkTypeAfterChildRewrite (SublinkInfo *info);
extern bool corrIsEquality (CorrVarInfo *corrVar);
extern bool corrIsOp (CorrVarInfo *corrVar);
extern bool SublinkInNegAndOrTop (SublinkInfo *info);
extern bool SublinkInAndOrTop (SublinkInfo *info);
extern bool exprInAndOrTop (Node *expr, Node *root);
extern bool queryIsAggWithoutGroupBy (Query *query);
extern bool hasCorrelatedVars (Node *node, CorrelatedVarsWalkerContext *context);
extern bool isEqualityOper (OpExpr *op);

#endif /* PROV_SUBLINK_UTIL_ANALYZE_H_ */
