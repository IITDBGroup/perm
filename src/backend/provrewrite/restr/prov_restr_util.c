/*-------------------------------------------------------------------------
 *
 * prov_restr_util.c
 *	  POSTGRES C
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/prov_restr_util.c,v 1.542 06.01.2009 11:30:02 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "optimizer/prep.h"
#include "optimizer/clauses.h"
#include "nodes/parsenodes.h"
#include "nodes/makefuncs.h"
#include "parser/parsetree.h"
#include "utils/lsyscache.h"

#include "provrewrite/prov_restr_pushdown.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/prov_nodes.h"

#include "provrewrite/prov_restr_rewrite.h"
#include "provrewrite/prov_restr_util.h"

/* prototypes */
static EquivalenceList *getEquivalenceList (List *equis, Node *expr);
static bool mergeEquivalenceLists (List *equis, EquivalenceList *left, EquivalenceList *right);
static bool createSelInfoWalker (Node *node, SelectionInfo *context);
static bool createEquivalenceList (SelScope *scope, SelectionInfo *sel);
static void addSelectionInfos (SelScope *scope, List *sels);
static bool computeEquivalenceListConsts (EquivalenceList *equi);
static Node *replaceJoinVarsWithBaseRelVars (Node *expr, Query *query);
static Var *getBaseRelVar (Var *var, Query *query);


/*
 * Adds a new expression to a PushdownInfo, rewrites the result and adapts EquivalenceLists.
 */

void
addExprToScopeWithRewrite (SelScope *scope, Node *expr, Query *query, ExprHandlers *handlers, Index rtIndex)
{
	addExprToScope(scope, expr, query, rtIndex);
	//TODO rewriteExpr(scope, handlers);
}

/*
 * Adds a new expression to a PushdownInfo
 */

void
addExprToScope(SelScope *scope, Node *expr, Query *query, Index rtIndex)
{
	List *sel;

	/* if expr is null return */
	if (expr == NULL)
		return;

	/* create a list of selection infos for the expr */
	sel = createSelectionInfoList(expr, scope, query, rtIndex);

	/* if at least one new selection info is created add it to SelScope */
	if (list_length(sel) > 0)
	{
		scope->selInfos = list_union (scope->selInfos, sel);

		addSelectionInfos(scope, sel);
	}
}

/*
 * For a given SelScope create Equivalence Lists for the SelectionInfos stored in the PushdownInfo.
 */

void
createEquivalenceLists (SelScope *scope)
{
	SelectionInfo *sel;
	ListCell *lc;
	ListCell *before;

	/* for each SelectionInfo check if it represents an
	 * new or existing equality list.
	 */
	foreachwithbefore(lc, before, scope->selInfos)
	{
		sel = (SelectionInfo *) lfirst(lc);

		/* if sel is an equality comparison remove it from
		 * the conjuncts list because it is redundend information.
		 */
		if (createEquivalenceList (scope, sel))
		{
//			if (before == NULL || before == result->conjuncts->head)
//				scope->conjuncts->head = lc->next;
//			else
//				before->next = lc->next;//CHECK ok to not remove expr, should be
		}
	}

	computeEquivalenceListsConsts (scope);
}

/*
 * Adds a list SelectionInfos to an SelScope. The equivalence lists are adapted accordingly.
 */
static void
addSelectionInfos (SelScope *scope, List *sels)
{
	ListCell *lc;
	SelectionInfo *sel;

	foreach(lc, sels)
	{
		sel = (SelectionInfo *) lfirst(lc);

		createEquivalenceList(scope, sel);
	}
}


/*
 * For a given SelScope and a new SelectionInfo, adapt the equivalence lists of the pushdown info, if
 * SelectionInfo is an equality comparison.
 */

static bool
createEquivalenceList (SelScope *scope, SelectionInfo *sel)
{
	EquivalenceList *equiLeft;
	EquivalenceList *equiRight;
	EquivalenceList *new;
	Expr *op;
	bool contra;

	contra = false;

	/* check if sel is a equality comparison between two simple arguments (no sublinks, ...) */
	if (isEquivOverSimpleArgs(sel))
	{
		op = (Expr *) sel->expr;
		equiLeft = getEquivalenceList(scope->equiLists, get_leftop(op));
		equiRight = getEquivalenceList(scope->equiLists, get_rightop(op));

		if (equiLeft == NULL)
		{
			/* both lists do not exist. Add new class with left and right expr as members */
			if (equiRight == NULL)
			{
				new = makeEquivalenceList();
				new->exprs = list_make2(get_leftop(op), get_rightop(op));
				new->genSels = list_make1(sel);
				new->scope = scope;

				contra = contra || computeEquivalenceListConsts(new);

				scope->equiLists = lappend(scope->equiLists, new);
			}
			/* left class does not exist. Add left expr to the class for right expr */
			else
			{
				equiRight->exprs = lappend(equiRight->exprs, get_leftop(op));
				equiRight->genSels = lappend(equiRight->genSels, sel);
				contra = contra || computeEquivalenceListConsts(equiRight);
			}
		}
		else
		{
			/* right class does not exist. Add right expr to the class for left expr */
			if (equiRight == NULL)
			{
				equiLeft->exprs = lappend(equiLeft->exprs, get_rightop(op));
				equiLeft->genSels = lappend(equiLeft->genSels, sel);
				contra = contra || computeEquivalenceListConsts(equiLeft);
			}
			/* both equi lists exist. If they are not equal, merge them */
			else
			{
				if (!equal(equiLeft, equiRight))
				{
					contra = contra || mergeEquivalenceLists (scope->equiLists, equiLeft, equiRight);
				}
			}
		}

		/* if an contradiction was found set the contradiction flag of the pushdown info */
		if (contra)
			scope->contradiction = true;

		/* return true to indicate that SelectionInfo should be removed */
		return true;
	}

	/* return false to indicate that SelectionInfo should not be removed */
	return false;
}

/*
 * merges two EquivalenceLists. (append their exprs-lists and remove duplicates)
 */

static bool
mergeEquivalenceLists (List *equis, EquivalenceList *left, EquivalenceList *right)
{
	EquivalenceList *merged;

	/* create merged list */
	merged = makeEquivalenceList ();
	merged->exprs = list_union(left->exprs, right->exprs);
	merged->genSels = list_union(left->genSels, right->genSels);
	merged->derivedSels = list_union(left->derivedSels, right->derivedSels);
	merged->scope = left->scope;

	/* compute consts */
	if(!computeEquivalenceListConsts (merged))
		return true;

	/* adapt list of equivalence lists */
	equis = list_delete(equis, left);
	equis = list_delete(equis, right);
	equis = lappend(equis, merged);
	//TODO additional stuff to do
	return false;
}

/*
 * Searches for the equivalence list an expression belongs to. If no such list is found NULL is returned.
 */

static EquivalenceList *
getEquivalenceList (List *equis, Node *expr)
{
	ListCell *classLc;
	ListCell *innerLc;
	EquivalenceList *equi;
	Node *member;

	foreach(classLc, equis)
	{
		equi = (EquivalenceList *) lfirst(classLc);

		foreach(innerLc, equi->exprs)
		{
			member = (Node *) lfirst(innerLc);

			if (equal(expr, member))
				return equi;
		}
	}

	return NULL;
}

/*
 * Check if a conjunct (SelectionInfo) is an equality comparison between arguments that do not contain sublinks or volatile
 * functions.
 */

bool
isEquivOverSimpleArgs (SelectionInfo *sel)
{
	OpExpr *op;

	if (sel->notMovable)
		return false;

	if (!IsA(sel->expr, OpExpr))
		return false;

	op = (OpExpr *) sel->expr;

	if (!isEqualityOper(op))
		return false;

	return true;
}

/*
 * Check if a conjunc (SelectionInfo) is an inequality between arguments that are vars or constants.
 */

bool
isInequalOverSimpleArgs (SelectionInfo *sel)
{
	OpExpr *op;

	if (sel->notMovable)
		return false;

	if (!IsA(sel->expr, OpExpr))
		return false;

	op = (OpExpr *) sel->expr;

	if (!isInequality((Node *) op))
		return false;

	if (!isVarOrConstWithCast(get_leftop((Expr *) op)) || !isVarOrConstWithCast(get_rightop((Expr *) op)))
		return false;

	return true;
}

/*
 * Checks if an conjunct (SelectionInfo) is an inequality. (and has no sublinks or volatile functions)
 */

bool
selIsInequality (SelectionInfo *sel)
{
	OpExpr *op;

	if (sel->notMovable)
		return false;

	if (!IsA(sel->expr, OpExpr))
		return false;

	op = (OpExpr *) sel->expr;

	return isInequality((Node *) op);
}

/*
 * Returns the type (<,<=,>,>=) of an inequality.
 */

ComparisonType
getTypeForIneq (SelectionInfo *sel)
{
	Assert (IsA(sel->expr, OpExpr));

	return getComparisonType(sel->expr);
}

/*
 * Creates a list of SelectionInfo nodes for an expression:
 * 		1) AND/OR expressions are flattened
 * 		2) NOT is pushed down
 * 		3) constant expressions are evaluated
 * 		4) a SelectionInfo node is created for every top level conjunct
 */

List *
createSelectionInfoList (Node *expr, SelScope *scope, Query *query, Index rtIndex)
{
	List *result;
	SelectionInfo *sel;
	Node *newExpr;
	ListCell *lc;
	result = NIL;
	bool derived;

	derived = (rtIndex == -1);

	/* replace join vars with base relation vars */
	newExpr = replaceJoinVarsWithBaseRelVars (expr, query);

	/* flatten AND/OR and find constant subexpressions */
	newExpr = eval_const_expressions(newExpr);

	/* pushdown NOT */
	newExpr = (Node *) canonicalize_qual((Expr *) newExpr);

	/* if newExpr is true const return NIL */
	if (equal(expr, makeBoolConst(true,false)))
		return NIL;

	/* if newExpr is false const set contradiction of pushdown info */
	if (equal(expr, makeBoolConst(false,false)))
	{
		scope->contradiction = true;
		return NIL;
	}

	/*
	 * if top node of transformed expression is not a AND create a
	 * single SelectionInfo, otherwise create a SelectionInfo for each
	 * conjunct.
	 */
	if (IsA(newExpr, BoolExpr))
	{
		BoolExpr *bool;

		bool = (BoolExpr *) newExpr;

		if(bool->boolop == AND_EXPR)
		{
			foreach(lc, bool->args)
			{
				sel = createSelectionInfo((Node *) lfirst(lc), rtIndex, derived);
				result = lappend(result, sel);
			}

			return result;
		}
	}

	return list_make1(createSelectionInfo(newExpr, rtIndex, derived));
}

/*
 *
 */

static Node *
replaceJoinVarsWithBaseRelVars (Node *expr, Query *query)
{
	ListCell *lc;
	List *vars;
	List *searchVars;
	List *replaceVars;
	Var *var;
	RangeTblEntry *rte;

	searchVars = NIL;
	replaceVars = NIL;

	/* get allVars in Expr */
	vars = findExprVars (expr);

	/* find join vars */
	foreach(lc, vars)
	{
		var = (Var *) lfirst(lc);

		rte = rt_fetch(var->varno, query->rtable);

		/* is a join var ? */
		if (rte->rtekind == RTE_JOIN)
		{
			searchVars = lappend(searchVars, var);
			replaceVars = lappend(replaceVars, getBaseRelVar (var, query));
		}
	}

	/* replace join vars with base rel vars */
	return replaceSubExpression(expr, searchVars, replaceVars, REPLACE_SUB_EXPR_ALL);
}

/*
 *
 */

Var *
getBaseRelVar (Var *var, Query *query)
{
	RangeTblEntry *rte;

	rte = rt_fetch(var->varno, query->rtable);

	if (rte->rtekind == RTE_JOIN)
	{
		return getBaseRelVar((Var *) list_nth(rte->joinaliasvars, var->varattno - 1), query);
	}

	return var;
}

/*
 * returns the EqualityList a Var belongs to.
 */

EquivalenceList *
getEqualityListForExpr (SelScope *scope, Node *expr)
{
	ListCell *lc;
	ListCell *innerLc;
	EquivalenceList *equi;
	Node *node;

	foreach(lc, scope->equiLists)
	{
		equi = (EquivalenceList *) lfirst(lc);

		foreach(innerLc,equi->exprs)
		{
			node = (Node *) lfirst(innerLc);

			if(equal(expr, node))
				return equi;
		}
	}

	return NULL;
}

/*
 * get an operand from a conjunct (SelectionInfo) that is an OpExpr. This will break if the operand is not
 * a simple var or const surrounded by 0 or more casts.
 */

Node *
getOpSelInfoOperand (SelectionInfo *sel, Index opIndex)//CHECK code dups with prov util
{
	OpExpr *op;
	Node *result;
	FuncExpr *funcExpr;

	Assert(IsA(sel->expr, OpExpr));

	op = (OpExpr *) sel->expr;

	result = (Node *) list_nth(op->args,opIndex);

	while(IsA(result, FuncExpr))
	{
		funcExpr = (FuncExpr *) result;

		result = (Node *) linitial(funcExpr->args);
	}

	return result;
}

/*
 * Create a SelectionInfo node from an expression.
 */

SelectionInfo *
createSelectionInfo (Node *expr, Index rtIndex, bool derived)
{
	SelectionInfo *result;

	result = makeSelectionInfo();
	result->expr = expr;
	result->derived = derived;
	result->rtOrigin = rtIndex;

	result->notMovable = contain_volatile_functions(expr);

	/* search for sublinks, aggregates, group by and normal vars */
	createSelInfoWalker (expr, result);

	return result;
}

/*
 * Search inside an expression for certain node types that are used to set the fields of a SelectionInfo node. E.g.
 * if we find an aggreagtion node "containsAggs" is set to true.
 */

static bool
createSelInfoWalker (Node *node, SelectionInfo *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, Var))
	{
		Var *var;

		var = (Var *) node;
		//TODO remove group by or actually check it
		context->vars = lappend(context->vars,var);
	}
	if (IsA(node, SubLink))
	{
		context->notMovable = true;
	}
//	if (IsA(node, FuncExpr))
//	{
//		FuncExpr *func;
//
//		func = (FuncExpr *) node;
//
//		//TODO search for non-strict and mutable funcs
//		if (func_volatile(func->funcid) == 'v')
//			//context->notMovable = true;
//	}
	if (IsA(node, Aggref))
	{
		context->vars = lappend(context->vars,node);
	}

	return expression_tree_walker(node, createSelInfoWalker, (void *) context);
}

/*
 * Computes the constants for each equivalence list in a PushdownInfo.
 */

void
computeEquivalenceListsConsts (SelScope *scope)
{
	ListCell *lc;
	EquivalenceList *equi;

	/* fill the constant field for each equivalence list */
	foreach(lc, scope->equiLists)
	{
		equi = (EquivalenceList *) lfirst(lc);

		if (!computeEquivalenceListConsts(equi))
			scope->contradiction = true;
	}
}

/*
 *	Generates a list with constants that are contained in an equivalence list. If there is one constant set this constant as
 *	the "constant" field of the equivalence list. If there is more than one constant check that all constants are the same.
 *	If this is the case keep just one of them. If return false to indicate a contradiction.
 */

static bool
computeEquivalenceListConsts (EquivalenceList *equi)
{
	List *consts;
	Const *constLeft;
	Const *constRight;
	ListCell *lc;

	consts = getEquivalenceListConsts (equi);

	if (list_length(consts) == 1)
	{
		equi->constant = (Const *) linitial(consts);

		return false;
	}

	/* more than one const, check that all consts are of the same value */
	if (list_length(consts) > 1)
	{
		constLeft = (Const *) linitial(consts);

		foreachsince(lc, consts->head->next)
		{
			constRight = (Const *) lfirst(lc);

			/* not same value, return true to indicate contradiction */
			if (!equal(constLeft, constRight))
				return false;
		}

		/* remove additional copies of constant */
		consts->head = consts->head->next;

		equi->exprs = list_difference(equi->exprs, consts);
		equi->constant = constLeft;
	}

	return false;
}

/*
 * return all constant expressions of an equivalence list.
 */

List *
getEquivalenceListConsts (EquivalenceList *equi)
{
	List *result;
	ListCell *lc;
	Node *expr;

	result = NIL;

	foreach(lc, equi->exprs)
	{
		expr = (Node *) lfirst(lc);

		if (IsA(expr, Const)) // CHECK THAT THIS Is derived by eval_const_expr
			result = lappend(result, expr);
	}

	return result;
}
