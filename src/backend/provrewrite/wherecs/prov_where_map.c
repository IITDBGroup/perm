/*-------------------------------------------------------------------------
 *
 * prov_where_map.c
 *	  PERM C 
 *
 * Portions Copyright (c) 2010 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $Perm: pgsql//SF_Perm/src/backend/provrewrite/wherecs/prov_where_map.c,v 1.542 Oct 4, 2010 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "nodes/makefuncs.h"
#include "optimizer/clauses.h"
#include "parser/parse_expr.h"

#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_sublink_util_analyze.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_where_map.h"


/* data type declarations */
typedef struct EqualWalkerContext
{
	Query *query;
	List **eqClasses;
} EqualWalkerContext;

/* function declarations */
static List *getInVars (Var *outVar, List *eqClasses);
static List *findEqClasses (Query *query);
static void findEqInJoinTreeItem (Node *item, EqualWalkerContext *context);
static bool getEqualitiesWalker (Node *node, EqualWalkerContext *context);
static void addSingleToClasses (Var *var, EqualWalkerContext *context);
static void addEqualitiesToClasses (Var *left, Var *right,
		EqualWalkerContext *context);

/*
 * Add the information about transitive equalities to the Where-CS ProvInfo
 * data structure. For each output attribute A of the query store all input
 * attributes that are (transitively) equal to the input attribute from
 * which A is derived. For instance,
 *
 * 		SELECT a AS out FROM r JOIN s ON (r.a = s.b);
 *
 * Here "out" is derived from r.a in the input and s.b is equal to r.a.
 */

void
makeRepresentativeQuery (Query *query)
{
	WhereProvInfo *info;
	WhereAttrInfo *attr;
	TargetEntry *te;
	Var *var;
	List *eqClasses;
	ListCell *lc;

	eqClasses = findEqClasses (query);

	/* create a info for each output attr */
	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);
		var = getVarFromTeIfSimple((Node *) te->expr);
		var = resolveToRteVar(var, query);

		attr = (WhereAttrInfo *) makeNode(WhereAttrInfo);
		attr->outVar = makeVar(0, te->resno, exprType((Node *) te->expr),
				exprTypmod((Node *) te->expr), 0);
		attr->inVars = getInVars(var, eqClasses);
		attr->annotVars = NIL;

		info->attrInfos = lappend(info->attrInfos, attr);
	}
}


/*
 *
 */

static List *
getInVars (Var *outVar, List *eqClasses)
{
	List *eqClass;
	ListCell *lc;

	if (!outVar)
		return NIL;

	foreach(lc, eqClasses)
	{
		eqClass = (List *) lfirst(lc);

		if (list_member(eqClass, outVar))
			return eqClass;
	}

	return NIL;
}

/*
 *
 */

static List *
findEqClasses (Query *query)
{
	List *classes = NIL;
	ListCell *lc;
	Node *item;
	EqualWalkerContext *context;
	TargetEntry *te;
	Var *var;

	context = (EqualWalkerContext *) palloc(sizeof(EqualWalkerContext));
	context->query = query;
	context->eqClasses = &classes;

	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);
		var = getVarFromTeIfSimple((Node *) te->expr);
		if (var)
		{
			var = resolveToRteVar(var, query);
			addSingleToClasses(var, context);
		}
	}

	foreach(lc, query->jointree->fromlist)
	{
		item = (Node *) lfirst(lc);
		findEqInJoinTreeItem(item, context);
	}

	getEqualitiesWalker(query->jointree->quals, context);

	context->eqClasses = NULL;
	pfree(context);

	return classes;
}

/*
 *
 */

static void
findEqInJoinTreeItem (Node *item, EqualWalkerContext *context)
{
	if (IsA(item, JoinExpr))
	{
		JoinExpr *join = (JoinExpr *) item;

		getEqualitiesWalker (join->quals, context);

		findEqInJoinTreeItem(join->larg, context);
		findEqInJoinTreeItem(join->rarg, context);
	}
}

/*
 *
 */

static bool
getEqualitiesWalker (Node *node, EqualWalkerContext *context)
{
	OpExpr *op;
	Var *left, *right;

	if (node == NULL)
		return false;

	// we found an equality
	if (IsA(node, OpExpr) && isEqualityOper((OpExpr *) node))
	{
		op = (OpExpr *) node;
		left = getVarFromTeIfSimple(linitial(op->args));
		left = resolveToRteVar(left, context->query);
		right = getVarFromTeIfSimple(lsecond(op->args));
		right = resolveToRteVar(right, context->query);

		// check that equality compares Vars (possibly casted)
		if (left == NULL || right == NULL)
			return false;

		addEqualitiesToClasses (left, right, context);

		return false;
	}

	return expression_tree_walker(node, getEqualitiesWalker, (void *) context);
}

/*
 *
 */

static void
addSingleToClasses (Var *var, EqualWalkerContext *context)
{
	ListCell *lc;
	List *cur;

	foreach(lc, *(context->eqClasses))
	{
		cur = (List *) lfirst(lc);

		if (list_member(cur, var))
			return;
	}

	cur = list_make1(var);
	*(context->eqClasses) = lappend(*(context->eqClasses), cur);
}

/*
 *
 */

static void
addEqualitiesToClasses (Var *left, Var *right, EqualWalkerContext *context)
{
	ListCell *lc;
	List **hit = NULL;
	List **secondhit = NULL;
	List *cur;

	foreach(lc, *(context->eqClasses))
	{
		cur = (List *) lfirst(lc);

		if (list_member(cur, left) || list_member(cur,right))
		{
			if (!hit)
				hit = (List **) &(lc->data.ptr_value);
			else
				secondhit = (List **) &(lc->data.ptr_value);
		}
	}

	/* no equivalence class contains left or right -> create a new one */
	if (!hit)
	{
		cur = list_make2(left, right);
		*(context->eqClasses) = lappend(*(context->eqClasses), cur);
	}
	/* one class contains left or right -> add both to this class */
	else if (!secondhit)
	{
		*hit = list_append_unique(*hit, left);
		*hit = list_append_unique(*hit, right);
	}
	/* one class contains left and another one contains right, merge these
	 * classes. */
	else
	{
		List *firstL = *hit;
		List *secondL = *secondhit;

		cur = list_union(firstL, secondL);

		*(context->eqClasses) = list_delete_ptr(*(context->eqClasses), *hit);
		*(context->eqClasses) = list_delete_ptr(*(context->eqClasses), *secondhit);

		*(context->eqClasses) = lappend(*(context->eqClasses), cur);
	}
}

