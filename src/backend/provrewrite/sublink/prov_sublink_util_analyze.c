/*
 * prov_sublink_util_analyze.c
 *
 *  Created on: Mar 31, 2010
 *      Author: lord_pretzel
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
#include "provrewrite/prov_sublink_util_analyze.h"


/* static methods */
static bool insideTestWalker (Node *node, InsideTestWalkerContext *context);
static bool OpHasOnlyVars (OpExpr *op, Node *corrAncestor, List **vars);
static bool getPathToExprWalker (Node *node, PathToExprWalkerContext *context);
static OpExpr *getCorrVarOp (CorrVarInfo *corrVar);
static bool isVarOrCasts (Node *node);


/*
 * Checks if a sublink has sublinks in its test expression or is itself used inside the
 * testexpression of another sublink.
 */

bool
sublinkHasSublinksInTestOrInTest (SublinkInfo *info)
{
        List *subInTest;
        bool insideTest;
        InsideTestWalkerContext *context;

        subInTest = NIL;

        findExprSublinkWalker (info->sublink->testexpr, &subInTest);

        if (list_length(subInTest) > 0)
                return true;

        insideTest = false;
        context = (InsideTestWalkerContext *) palloc(sizeof(InsideTestWalkerContext));
        context->result = &insideTest;
        context->search = info->sublink;
        context->inSub = false;

        insideTestWalker(info->exprRoot, context);

        pfree(context);

        return insideTest;
}

/*
 *
 */

static bool
insideTestWalker (Node *node, InsideTestWalkerContext *context)
{
        SubLink *sub;
        InsideTestWalkerContext *newContext;

        if(node == NULL)
                return false;

        if(IsA(node, SubLink))
        {
                sub = (SubLink *) node;

                if(equal(sub, context->search))
                {
                        if(context->inSub)
                                return (*context->result = true);

                        return true;
                }

                newContext = (InsideTestWalkerContext *) palloc(sizeof(InsideTestWalkerContext));

                newContext->result = context->result;
                newContext->search = context->search;
                newContext->inSub = true;

                return expression_tree_walker(sub->testexpr, insideTestWalker, (void *) newContext);
        }

        return expression_tree_walker(node, insideTestWalker, (void *) context);
}

/*
 * checks if an sublink is the root of an expression tree
 */

bool
isTopLevelSublink(SublinkInfo *info)
{
        if ((Node *) info->sublink == info->exprRoot)
        {
                return true;
        }
        if (equal(info->sublink, info->exprRoot))
        {
                return true;
        }
        return false;
}

/*
 * Returns the category the provided sublink would have if its child sublinks would be rewritten. We
 * need this information to determine if we can apply a rewrite method that requires an uncorrelated
 * sublink to a sublink that has children that were correlated before rewrite but are uncorrelated
 * afterwards.
 */

SublinkCat
getSublinkTypeAfterChildRewrite (SublinkInfo *info)
{
        List *childInfos;
        ListCell *lc;
        SublinkInfo *child;
        List *corrVars;
        bool uncorr;

        /* category UNCORRELATED does not change if child sublinks are rewritten */
        if (info->category == SUBCAT_UNCORRELATED)
                return SUBCAT_UNCORRELATED;

        /* if deccorelation is deactivated rewritting the children does not change the category */
        if (!prov_use_unnest_JA && !prov_use_sublink_transfrom_top_level_any_to_join)
                return info->category;

        /* check which correlated Vars disappear if child sublinks are decorrelated */
        corrVars = copyObject(info->corrVarInfos);

        foreach(lc, corrVars)
        {
                corrIsEquality((CorrVarInfo *) lfirst(lc));
        }

        childInfos = getChildSublinks (info);
        foreach(lc, childInfos)
        {
                child = (SublinkInfo *) lfirst(lc);
                uncorr = false;

                if (checkJAstrategyPreconditions(child))
                        uncorr = true;
                if (checkEXISTSstrategyPreconditions(child))
                        uncorr = true;
                if (uncorr)
                        corrVars = list_difference(corrVars, child->corrVarInfos);
        }

        /* if no corelated vars remain that means all child sublinks will be decorrelated and
         * the sublink can be handled as a uncorrelated sublink.
         */
        if (list_length(corrVars) == 0)
                return SUBCAT_UNCORRELATED;
        else
                return info->category;
}





/*
 * Checks if a correlated var is used in an equality comparison
 */

bool
corrIsEquality (CorrVarInfo *corrVar)
{
        OpExpr *op;

        op = getCorrVarOp (corrVar);

        if (op == NULL)
                return false;

        return isEqualityOper(op);
}

/*
 * Checks if an correlated var is used in an normal operator (an OpExpr node).
 */

bool
corrIsOp (CorrVarInfo *corrVar)
{
        OpExpr *op;

        op = getCorrVarOp (corrVar);

        return (op != NULL);
}

/*
 *
 */
static OpExpr *
getCorrVarOp (CorrVarInfo *corrVar)
{
        OpExpr *op;
        Node *node;
        Node *last;
        List *path;
        List *vars;
        PathToExprWalkerContext *context;
        ListCell *lc;
        Oid oid;

        vars = NIL;

        context = (PathToExprWalkerContext *) palloc(sizeof(PathToExprWalkerContext));
        context->node = (Node *) corrVar->corrVar;
        context->path = NIL;
        context->resultPath = &path;

        getPathToExprWalker (corrVar->exprRoot, context);

        reverseCellsInPlace(path);

        last = (Node *) corrVar->corrVar;
        foreach(lc, path)
        {
                node = (Node *) lfirst(lc);

                if (IsA(node, OpExpr))
                {
                        op = (OpExpr *) node;

                        oid = exprType(linitial(op->args));

                        if (OpHasOnlyVars(op, (Node *) corrVar->corrVar, &vars))
                        {
                                corrVar->parent = node;
                                corrVar->vars = vars;
                                return op;
                        }
                        return NULL;
                }
                else if (!IsA(node,FuncExpr))
                {
                        //TODO check if it is a type cast
                        return NULL;
                }
                else
                {
                        return NULL;//CHECK ????
                }
                last = node;
        }

        return NULL;
}


/*
 *
 */

static bool
OpHasOnlyVars (OpExpr *op, Node *corrAncestor, List **vars)
{
        ListCell *lc;
        Node *node;
        Var *var;

        foreach(lc, op->args)
        {
                node = lfirst(lc);
                if (!equal(node, corrAncestor))
                {
                        if(!IsA(node,Var))
                                return false;

                        var = (Var *) node;

                        if (var->varlevelsup > 0)
                                return false;

                        *vars = lappend(*vars, node);
                }
        }

        return true;
}


/*
 *      Searches for an subexpression in an expression. If the expression is found, returns a path (List of nodes) from the top of the
 *      expression to the subexpression that is searched.
 *              For example if we search for expression a = b IN expression (true OR (d = e AND a = b)) the resulting path is:
 *              {OR,AND}
 */

static bool
getPathToExprWalker (Node *node, PathToExprWalkerContext *context)
{
        PathToExprWalkerContext *newContext;

        if (node == NULL)
                return false;

        if (equal(node,context->node)) {
                if (context->path != NIL)
                        *(context->resultPath) = copyObject(context->path);

                return false;
        }

        newContext = (PathToExprWalkerContext *) palloc(sizeof(PathToExprWalkerContext));
        newContext->node = context->node;
        newContext->path = copyObject(context->path);
        newContext->path = lappend(newContext->path, node);
        newContext->resultPath = context->resultPath;

        return expression_tree_walker(node, getPathToExprWalker, (void *) newContext);
}



/*
 * Checks if a sublink is directly below a NOT node and this NOT node is either the top level node or in an AND expression.
 */
bool
SublinkInNegAndOrTop (SublinkInfo *info)
{
        BoolExpr *not;

        if (info->parent == NULL)
                return false;
        if (!IsA(info->parent, BoolExpr))
                return false;

        not = (BoolExpr *) info->parent;
        if (!(not->boolop == NOT_EXPR))
                return false;

        //TODO add support for EXPR_SUBLINK in op like in SublinkInAndOrTop
        return exprInAndOrTop (info->parent, info->exprRoot);
}


/*
 * Checks if a sublink is in either a top level sublink or is used in an expression using only boolean AND.
 */

bool
SublinkInAndOrTop (SublinkInfo *info)
{
        OpExpr *op;
        Node *arg;
        ListCell *lc;

        /* If sublink is an expression sublink check if it is used in an simple comparison.
         * If that is the case check this comparison instead of the sublink.
         */
        if (info->sublink->subLinkType == EXPR_SUBLINK && info->parent)
        {
                if (IsA(info->parent, OpExpr))//TODO this is maybe to restrictive
                {

                        op = (OpExpr *) info->parent;

                        /* check that oper is an equality comparison */
//                      if(!isEqualityOper(op))//CHECK ok to not check? We need info is func is strict
//                              return false;

                        /* check that other args of op are simple vars */
                        foreach(lc, op->args)
                        {
                                arg = (Node *) lfirst(lc);

                                if (!equal(arg, info->sublink) && !isVarOrCasts(arg))
                                        return false;
                        }
                        return exprInAndOrTop ((Node *) op, info->exprRoot);
                }
                if (!IsA(info->parent, BoolExpr))
                {
                        return false;
                }
        }

        return exprInAndOrTop ((Node *) info->sublink, info->exprRoot);
}


/*
 * Checks if an expression is Var or some cast functions applied to an Var.
 */

static bool
isVarOrCasts (Node *node)
{
        FuncExpr *funcExpr;
        ListCell *lc;
        Node *arg;
        Var *var;

        if (IsA(node, Var)) {
                var = (Var *) node;

                return var->varlevelsup == 0;
        }
        else if (IsA(node, FuncExpr))
        {
                funcExpr = (FuncExpr *) node;

                if (funcExpr->funcformat == COERCE_EXPLICIT_CALL)//CHECK does this really define if a function is cast or not
                        return false;

                foreach(lc,funcExpr->args)
                {
                        arg = (Node *) lfirst(lc);

                        if (!isVarOrCasts(arg))
                                return false;
                }
        }
        return true;
}

/*
 * Checks if an expression is either a top level expression or is used in an AND-tree.
 */

bool
exprInAndOrTop (Node *expr, Node *root)
{
        ListCell *lc;
        List *path;
        PathToExprWalkerContext *context;
        Node *node;
        BoolExpr *boolExpr;

        if (equal(expr, root))
                return true;

        path = NIL;

        context = (PathToExprWalkerContext *) palloc(sizeof(PathToExprWalkerContext));
        context->node = expr;
        context->path = NIL;
        context->resultPath = &path;

        getPathToExprWalker (root, context);

        if (path == NIL)
                return false;

        foreach(lc, path)
        {
                node = (Node *) lfirst(lc);
                if (!IsA(node, BoolExpr))
                        return false;
                else
                {
                        boolExpr = (BoolExpr *) node;

                        if (boolExpr->boolop != AND_EXPR)
                                return false;
                }
        }

        return true;
}

/*
 *      Checks if query is an aggregation without GROUP BY and HAVING
 */

bool
queryIsAggWithoutGroupBy (Query *query)
{
        if (!query->hasAggs)
                return false;

        if (list_length(query->groupClause) > 0)
                return false;

        return query->havingQual == NULL;
}

/*
 * Checks if an expression used in a sublink contains vars that reference a relation outside the sublink.
 */

bool
hasCorrelatedVars (Node *node, CorrelatedVarsWalkerContext *context)
{
        if (node == NULL)
                return false;

        // check for var nodes with sublevelsup
        if (IsA(node, Var))
        {
                Var *var;

                var = (Var *) node;
                if (var->varlevelsup >= context->varlevelsUp)
                {
                        context->result = true;
                }
                return false;
        }

        // recurse
        return expression_tree_walker(node, hasCorrelatedVars, (void *) context);
}

/*
 *      Checks if OpExpr is an equality comparison.
 */

bool
isEqualityOper (OpExpr *op)
{
        Oid oid;

        Assert(list_length(op->args) > 0);

        oid = exprType(linitial(op->args));
        return op->opfuncid == equality_oper_funcid(oid);
}
