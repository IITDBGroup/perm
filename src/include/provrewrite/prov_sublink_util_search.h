/*-------------------------------------------------------------------------
 *
 * prov_sublink_util_search.h
 *		External interface to 
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql//perm/src/include/provrewrite/prov_sublink_util_search.h ,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_SUBLINK_UTIL_SEARCH_H_
#define PROV_SUBLINK_UTIL_SEARCH_H_

#include "utils/rel.h"
#include "nodes/nodes.h"
#include "nodes/parsenodes.h"
#include "provrewrite/prov_nodes.h"

/* flags for findSublinkLocations */
#define PROV_SUBLINK_SEARCH_SELECT      0x01
#define PROV_SUBLINK_SEARCH_WHERE       0x02
#define PROV_SUBLINK_SEARCH_HAVING      0x04
#define PROV_SUBLINK_SEARCH_GROUPBY 0x08
#define PROV_SUBLINK_SEARCH_ORDER       0x10
#define PROV_SUBLINK_SEARCH_ALL         0x1F            /* search in all locations */
#define PROV_SUBLINK_SEARCH_AGG         0x0D            /* used for aggregation. Search in SELECT, GROUP BY, HAVING */
#define PROV_SUBLINK_SEARCH_SPJ         0x13            /* for SPJ queries skip HAVING and GROUP BY */

/* flags for findSublinkByCats */
#define PROV_SUBLINK_SEARCH_UNCORR      0x01
#define PROV_SUBLINK_SEARCH_CORR        0x02
#define PROV_SUBLINK_SEARCH_CORR_IN     0x04

/* flags for findSublinksUnnested */
#define PROV_SUBLINK_SEARCH_UNNEST      0x01
#define PROV_SUBLINK_SEARCH_NOUNNEST    0x02

/* flags for findCorrVarTypes */
#define PROV_CORRVAR_INSIDE             0x01
#define PROV_CORRVAR_OUTSIDE            0x02

/*
 * Find sublink vars correlated to outside?
 */

typedef struct GetCorrelatedVarsWalkerContext
{
        List **correlated;
        Node *parent;
        Node *exprRoot;
        SublinkLocation location;
        int varlevelsUp;
		List *realLevelsUp;
        bool belowAgg;
        bool belowSet;
} GetCorrelatedVarsWalkerContext;

/*
 *
 */

typedef struct FindLinkToExprWalkerContext
{
        Node **parent;
        Node *expr;
        Node **result;
} FindLinkToExprWalkerContext;

/* Search functions */
extern List *getSublinkBaseRelations(Query *query);
extern bool findExprSublinkWalker (Node *node, List **context);
extern List *findSublinkLocations (Query *query, int flags);
extern List *findSublinkByCats (List *sublinks, int flags);
extern List *findSublinksUnnested (List *infos, int flags);
extern void findVarsSublevelsUp (Query *query, SublinkInfo *info);
extern List *findExprVars (Node *node);
extern List *findSublinksForExpr (Node *expr);
extern SublinkInfo *extractSublinks (Node *node, Node *exprRoot, Node **parentRef, Node **grandParentRef, SubLink* sublink, Node *parent);
extern void findRefRTEinCondition (SublinkInfo *info);
extern bool findRTRefInFromExpr (Node *fromItem, Index rtIndex);
extern List *findAccessedBaseRelations (Query *query);
extern List *getSublinkTypes (List *sublinks, int flags);
extern Node **findLinkToExpr (Node *expr, Node *root);
extern List *findCorrVarTypes (List *corrVars, int flags);
extern bool containsGenCorrSublink (SublinkInfo *info);
extern List *getChildSublinks (SublinkInfo *info);

#endif /* PROV_SUBLINK_UTIL_SEARCH_H_ */
