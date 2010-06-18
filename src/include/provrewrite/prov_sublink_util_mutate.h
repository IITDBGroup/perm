/*-------------------------------------------------------------------------
 *
 * prov_sublink_util_mutate.h
 *		External interface to 
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql//perm/src/include/provrewrite/prov_sublink_util_mutate.h ,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_SUBLINK_UTIL_MUTATE_H_
#define PROV_SUBLINK_UTIL_MUTATE_H_

#include "utils/rel.h"
#include "nodes/nodes.h"
#include "nodes/parsenodes.h"
#include "provrewrite/prov_nodes.h"
//#include "provrewrite/prov_sublink_unnest.h"



/*
 * Context structure for the replace params mutator
 */
typedef struct ReplaceParamsContext
{
        int paramSublevelsUp;
        int addVarSublevelsUp;  /* use if a fixed value should be added to varlevelsup */
        int varSublevelsUp;             /* use if varlevelsup should be set to a fixed value */
        int aggSublevelsUp;
        bool touchAggs;
        bool touchParams;
        int useVarnoValue;
        Query *sublink;
} ReplaceParamsContext;



/* macros */

#define IsUnnestable(sublink) \
        (checkJAstrategyPreconditions((SublinkInfo *) sublink) \
        || checkEXISTSstrategyPreconditions((SublinkInfo *) sublink) \
        )

/* Mutator methods */
extern Node *replaceParamsMutator (Node *node, ReplaceParamsContext* context);
extern Node *increaseSublevelsUpMutator (Node *node, int *context);
extern void addDummyAttr (Query *query);
extern void joinQueryRTEs(Query *query);

/* functions */
extern void sortSublinkInfos (List **sublinks);

#endif /* PROV_SUBLINK_UTIL_MUTATE_H_ */
