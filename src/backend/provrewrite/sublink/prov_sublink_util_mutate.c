/*-------------------------------------------------------------------------
 *
 * prov_sublink_util_mutate.c
 *	  PERM C 
 *
 * Portions Copyright (c) 2010 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $Perm: pgsql//perm/src/backend/provrewrite/sublink/prov_sublink_util_mutate.c,v 1.542 Mar 31, 2010 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "nodes/makefuncs.h"
#include "nodes/nodes.h"
#include "nodes/primnodes.h"
#include "nodes/print.h"                                // pretty print node (trees)
#include "optimizer/clauses.h"
#include "parser/parse_relation.h"
#include "parser/parse_expr.h"
#include "parser/parse_oper.h"
#include "parser/parsetree.h"
#include "utils/guc.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/provstack.h"
#include "provrewrite/provlog.h"
#include "provrewrite/prov_sublink_unnest.h"
#include "provrewrite/prov_sublink_unn.h"
#include "provrewrite/prov_sublink_util_mutate.h"

/* Static methods */
static int compareSublinkInfos (void *left, void *right);

/*
 * Replaces the Param nodes in a sublink test-expression with Var nodes referencing the
 * range table entry with index rtIndex for the sublink query.s
 */

Node *
replaceParamsMutator (Node *node, ReplaceParamsContext* context)
{
        if (node == NULL)
                return NULL;

        // replace Param nodes with Vars
        if (IsA(node, Param) && context->touchParams)
        {
                Param *param;
                Node *newExpr;
                TargetEntry *te;

                param = (Param *) node;

                /* find target list entry for param and retrieve expr value */
                te = (TargetEntry *) list_nth(context->sublink->targetList,param->paramid - 1);

                /* if the caller provides an varno value create a new var referencing this RTE */
                if (context->useVarnoValue)
                        newExpr = (Node *) makeVar(context->useVarnoValue, param->paramid, param->paramtype, param->paramtypmod, 0);
                /* else use the expr from the original sublink target entry */
                else
                        newExpr = (Node *) copyObject(te->expr);

                return (Node *) newExpr;
        }
        // adapt varlevelsup for Var nodes
        else if (IsA(node, Var))
        {
                Var *var;

                var = (Var *) node;

                if (context->addVarSublevelsUp)
                        var->varlevelsup = var->varlevelsup + context->addVarSublevelsUp;

                if (context->varSublevelsUp != -1)
                        var->varlevelsup = context->varSublevelsUp;

                return (Node *) var;
        }
        // adapt aggregation varlevels up
        else if (IsA(node, Aggref) && context->touchAggs)
        {
                Aggref *aggref;

                aggref = (Aggref *) node;
                aggref->agglevelsup = context->aggSublevelsUp;

                return expression_tree_mutator(node, replaceParamsMutator, (void *) context);
        }

        // recurse
        return expression_tree_mutator(node, replaceParamsMutator, (void *) context);
}



/*
 * A mutator that walks trough a sublink query and increases sublevelsup values of Var nodes that
 * reference an attribute from outside the sublink query. Note that the sublink query may contain
 * sublinks itself so we have to adapt their Var nodes too.
 */

Node *
increaseSublevelsUpMutator (Node *node, int *context)
{
        if (node == NULL)
                return node;

        if (IsA(node, Var))
        {
                Var *var;

                var = (Var *) node;
                if (var->varlevelsup > *context)
                {
                        var->varlevelsup++;
                }

                return (Node *) var;
        }
        else if (IsA(node, Query))
        {
                int *newContext;

                newContext = (int *) palloc(sizeof(int));
                *newContext = *context + 1;
                return (Node *) query_tree_mutator((Query *) node, increaseSublevelsUpMutator, (void *) newContext, QTW_IGNORE_JOINALIASES);
        }

        return expression_tree_mutator(node, increaseSublevelsUpMutator, (void *) context);
}


/*
 * Adds an constant valued dummy attribute to a queries target list. This is used to
 * distinguish between cases where the sublink is left joined to the original query
 * and it's not clear wether the left join failed or the result attributes of the
 * sublink query contain NULL values.
 */

void
addDummyAttr (Query *query)
{
        TargetEntry *te;
        Const *dummyConst;

        dummyConst = (Const *) makeBoolConst(false,false);
        te = makeTargetEntry((Expr *) dummyConst, list_length(query->targetList) + 1, "dummy1", false);
        query->targetList = lappend(query->targetList, te);
}


/*
 * If query has more than one from item (implicit cross prod), join them explicitly.
 */

void
joinQueryRTEs(Query *query)
{
        ListCell *lc;
        JoinExpr *newJoin;
        List *fromItems;
        List *subJoins;

        /* if query has only one or no range table entry return */
        if (list_length(query->jointree->fromlist) <= 1)
                return;

        subJoins = NIL;
    fromItems = query->jointree->fromlist;
    query->jointree->fromlist = NIL;

    /* create a join tree that joins all from items */
    lc = fromItems->head;

    newJoin = createJoinExpr (query, JOIN_INNER);
    newJoin->quals = (Node *) makeBoolConst(true, false);
    newJoin->larg = (Node *) lfirst(lc);
    lc = lc ->next;
    newJoin->rarg = (Node *) lfirst(lc);

    subJoins = lappend(subJoins, newJoin);

    for(lc = lc->next; lc != NULL; lc = lc->next)
        {
        newJoin = createJoinExpr (query, JOIN_INNER);

                /* set join condition to true */
                newJoin->quals = (Node *) makeBoolConst(true, false);

                /* set children */
                newJoin->rarg = (Node *) llast(subJoins);
                newJoin->larg = (Node *) lfirst(lc);

                /* append to join list */
                subJoins = lappend(subJoins, newJoin);
        }

    /* create from list as new top join */
    query->jointree->fromlist = list_make1(newJoin);

    adaptRTEsForJoins(subJoins, query, "query_rte_joined");
}

/*
 *
 */

void
sortSublinkInfos (List **sublinks)
{
        sortList(sublinks, compareSublinkInfos, true);
}

/*
 *
 */

static int
compareSublinkInfos (void *left, void *right)
{
        SublinkInfo *l;
        SublinkInfo *r;

        l = (SublinkInfo *) left;
        r = (SublinkInfo *) right;

        if (l->sublinkPos < r->sublinkPos)
                return -1;

        if (l->sublinkPos > r->sublinkPos)
                return 1;

        return 0;
}
