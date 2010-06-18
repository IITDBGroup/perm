/*-------------------------------------------------------------------------
 *
 * prov_restr_rewrite.c
 *	  POSTGRES C
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/prov_restr_rewrite.c,v 1.542 06.01.2009 11:29:37 bglav Exp $
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
#include "provrewrite/provstack.h"

#include "provrewrite/prov_restr_rewrite.h"
#include "provrewrite/prov_restr_util.h"
#include "provrewrite/prov_restr_ineq.h"

/* prototypes */
static RestricterInfo *createRestricterInfo (Query *query, Index rtindex, PushdownInfo *input);

/* rewriters that use logical equivalences or implications to rewrite an expression */
static void rewriteExpandEquivalenceLists (PushdownInfo *pushdown, ExprHandlers *handlers);
static void rewriteRemoveDups (PushdownInfo *pushdown, ExprHandlers *handlers);
static void rewriteDeduceInequalities (PushdownInfo *pushdown, ExprHandlers *handlers);

/* restriction rewriters */
static void restrictSimpleSelection (PushdownInfo *input, PushdownInfo *output, RestricterInfo *restrInfo, ExprHandlers *handlers);
static void restrictUsingEquivalenceLists (PushdownInfo *input, PushdownInfo *output, RestricterInfo *restrInfo, ExprHandlers *handlers);
static void restrictFilterForDisjunctions (PushdownInfo *input, PushdownInfo *output, RestricterInfo *restrInfo, ExprHandlers *handlers);
static void restrictPushThroughAgg (PushdownInfo *input, PushdownInfo *output, RestricterInfo *restrInfo, ExprHandlers *handlers);

/* rewrite helpers */
static void adaptEquivalenceList (EquivalenceList **equi, RestricterInfo *restrInfo);
static bool replaceJoinRTEVarsWalker (Node *node, RestricterInfo *context);
static Node *pushdownVarToTargetExprMutator (Node *node, List *context);
static bool isPushable (Node *expr, RestricterInfo *restrInfo);
static bool isPushableWalker (Node *node, RestrictableWalkerContext *context);
static bool selectionIsPushable (SelectionInfo *sel, RestricterInfo *restrInfo, List *equis);
static bool selectionIsPushableWalker (Node *node, SelectionPushableWalkerContext *context);
static int getEquivalenceListForExpr (Node *expr, RestricterInfo *restrInfo, List *equis);
static List *adaptSelectionInfoForPushdown (SelectionInfo *sel, RestricterInfo *restrInfo, List *inputEquis, List *outputEquis, PushdownInfo *info);
static Node *adaptSelectionInfoMutator (Node *node, AdaptSelectionMutatorContext *context);
//static void replaceRtindex (SelectionInfo *sel, Index newIndex);
static List *getSubtreeRTindexes (Query *query, Index rtindex);
static void getSubtreeRtesWalker (Node *node, List **context);
static void adaptIndices (SelectionInfo *sel, Index rtindex, Query *query);
static PushdownInfo *replaceVarsWithEquivalenceLists (PushdownInfo *info);

/*
 * Apply all expression rewrites stored in the ExprHandlers data structure to PushdownInfo info.
 */

void
rewriteExpr (PushdownInfo *info, ExprHandlers *handlers)
{
	ListCell *lc;
	ExprRewriter rewriter;

	foreach(lc, handlers->rewriters)
	{
		rewriter = (ExprRewriter) lfirst(lc);

		if (!info->contradiction)
			(*rewriter) (info, handlers);
	}
}

/*
 * Apply all restriction expression rewriters stored in the ExprHandlers data structure to PushdownInfo info for the range table
 * entry at rtindex.
 */

PushdownInfo *
restrictExpr (PushdownInfo *info, Query *query, Index rtindex, ExprHandlers *handlers)
{
	ListCell *lc;
	ExprRestricter restricter;
	PushdownInfo *newInfo;
	RestricterInfo *restrInfo;

	newInfo = makePushdownInfo ();
	restrInfo = createRestricterInfo (query, rtindex, info);

	foreach(lc, handlers->restricters)
	{
		restricter = (ExprRestricter) lfirst(lc);

		if (!info->contradiction)
			restricter (info, newInfo, restrInfo, handlers);
	}

	return newInfo;
}

/*
 *
 */

static RestricterInfo *
createRestricterInfo (Query *query, Index rtindex, PushdownInfo *input)
{
	RestricterInfo *restrInfo;
	EquivalenceList **equiMap;
	int i;

	restrInfo = (RestricterInfo *) palloc(sizeof(RestricterInfo));
	restrInfo->query = query;
	restrInfo->rtindex = rtindex;
	restrInfo->rtIndexes = getSubtreeRTindexes(restrInfo->query, restrInfo->rtindex);
	restrInfo->rte = rt_fetch(rtindex, query->rtable);

	switch(restrInfo->rte->rtekind)
	{
	case RTE_JOIN:
		restrInfo->type = PUSHDOWN_JOIN;
	break;
	case RTE_SUBQUERY:
		restrInfo->type = PUSHDOWN_SUBQUERY;
	break;
	case RTE_RELATION:
		restrInfo->type = PUSHDOWN_RELATION;
	break;
	default:
		restrInfo->type = PUSHDOWN_OTHER;
	break;
	}
	 //TODO PUSHDOWN AGG

	/* allocate map from original equivalence list position to new equivalence lists */
	equiMap = (EquivalenceList **) palloc(sizeof(EquivalenceList *) * list_length(input->equiLists));
	restrInfo->equiMap = equiMap;

	for (i = 0; i < list_length(input->equiLists); i++)
		equiMap[i] = NULL;

	return restrInfo;
}

/*
 * Return the list of activated rewriter functions. Which functions are actived is determined by GUC options.
 */

List *
getRewriters ()
{
	List *result;

	result = NIL;

	//result = lappend(result, rewriteExpandEquivalenceClasses);
	result = lappend(result, rewriteRemoveDups);
	//result = lappend(result, rewriteDeduceInequalities);

	return result;
}

/*
 * Return the list of activated restricter functions. Which functions are actived is determined by GUC options.
 */

List *
getRestricters ()
{
	List *result;
	result = NIL;

//	result = lappend(result, restrictSimpleSelection);
	result = lappend(result, restrictUsingEquivalenceLists);

	return result;
}

/*
 * ----------------------------------------------------------------------------------------
 * 		Rewriters
 * ----------------------------------------------------------------------------------------
 */

/*
 * add implications derived from equivalence classes to the list of conjuncts of a PushdownInfo
 */

static void
rewriteExpandEquivalenceLists (PushdownInfo *pushdown, ExprHandlers *handlers)
{
	//EquivalenceList *equi;
	ListCell *lc;

	foreach(lc, pushdown->equiLists)
	{

	}

	//CHECK we don't need this do we?
}

/*
 * remove duplicate expressions taking equality classes into account
 */

static void rewriteRemoveDups (PushdownInfo *pushdown, ExprHandlers *handlers)
{
	PushdownInfo *infoWithEqui;
	ListCell *left;
	ListCell *right;
	List *remove;
	int i, j;

	infoWithEqui = replaceVarsWithEquivalenceLists(pushdown);
	remove = NIL;

	/* for each conjunct not already in the remove list, search for duplicate conjuncts
	 * and add them to the remove list.
	 */
	foreachi(left, i, infoWithEqui->conjuncts)
	{
		if (!list_member_int(remove, i))
		{
			foreachisince(right, j, left->next, i + 1)
			{
				if (equal(lfirst(left), lfirst(right)))
					remove = list_append_unique_int(remove, j);
			}
		}
	}

	/* free Pushdown without Vars */
	pfree(infoWithEqui);

	/* remove duplicates from original PushdownInfo */
	removeElems(&(pushdown->conjuncts), remove);
}

/*
 *	Deduce new inequalities and remove redundent ones. The work is done by the prov_restr_ineq.h methods.
 *		E.g. a < c1 AND b < c2 with c1 < c2. If E(a) = E(b) we can safely remove b < c2.
 */
/*
 * Derive new inequalities from existing ones. The following rules are used:
 * 		E1 < E2 && E2 < E1 ->	false
 * 		E1 < E2 && E2 < c  -> E1 < c
 * 		E1 < c1 && E1 > c2 with c1 < c2  -> false
 * 		E1 < E1 -> false
 */

static void
rewriteDeduceInequalities (PushdownInfo *pushdown, ExprHandlers *handlers)
{
	InequalityGraph *graph;

	/* build inequality-EC graph */
	graph = computeInequalityGraph(pushdown);
	/* compute transitive closure */
	graph = computeTransitiveClosure(graph);

	if (graph == NULL)
	{
		pushdown->contradiction = true;
		return;
	}

	/* remove redundend edges and merge ECs */
	graph = minimizeInEqualityGraph(pushdown, graph);

	if (graph == NULL)
	{
		pushdown->contradiction = true;
		return;
	}

	/* delete old inequalities and add new inequalities for edges */
	generateInequalitiesFromGraph(pushdown, graph);
}




/*
 * ----------------------------------------------------------------------------------------
 *		Restrict rewriters:
 * 		Rewriters that restrict expressions to the attributes of a range table entry.
 * 		Used to push selections down through join nodes.
 * ----------------------------------------------------------------------------------------
 */

/*
 * Searches for conjuncts that contain solely vars from the range table given by parameter rtindex and pushes
 */

static void
restrictSimpleSelection (PushdownInfo *input, PushdownInfo *output, RestricterInfo *restrInfo, ExprHandlers *handlers)
{
	ListCell *lc;
	ListCell *innerLc;
	SelectionInfo *sel;
	SelectionInfo *newSel;
	bool contained;
	Var *var;

	/* this restricter cannot push trough aggregations */
	if(restrInfo->type == PUSHDOWN_AGG)
		return;

	foreach(lc, input->conjuncts)
	{
		sel = (SelectionInfo *) lfirst(lc);
		contained = true;

		foreach(innerLc, sel->vars)
		{
			var = (Var *) lfirst(innerLc);

			contained = contained && list_member_int(restrInfo->rtIndexes, var->varno);
		}

		if (contained)
		{
			newSel = copyObject(sel);
			adaptIndices(sel, restrInfo->rtindex, restrInfo->query);
			output->conjuncts = lappend(output->conjuncts, newSel);
		}
	}
}

/*
 * Searches for conjuncts that contain solely vars from the range table given by parameter rtindex and pushes
 */

static void
restrictUsingEquivalenceLists (PushdownInfo *input, PushdownInfo *output, RestricterInfo *restrInfo, ExprHandlers *handlers)
{
	EquivalenceList *equi;
	EquivalenceList *newEqui;
	List *newExprs;
	ListCell *lc;
	ListCell *innerLc;
	Node *expr;
	int i;
	SelectionInfo *sel;
	List *newSels;

	/* this restricter cannot push trough aggregations */
	if(restrInfo->type == PUSHDOWN_AGG)
		return;

	/* check for each equivalence list which expression can be kept and
	 * create new equivalence lists for them with adapted expressions
	 */
	foreachi(lc, i,  input->equiLists)
	{
		equi = (EquivalenceList *) lfirst(lc);
		newExprs = NIL;

		/* check for each expr of EC if it is valid after pushdown */
		foreach(innerLc, equi->exprs)
		{
			expr = (Node *) lfirst(innerLc);

			if(isPushable(expr, restrInfo))
				newExprs = lappend(newExprs, expr);
		}

		/* if at least one expr is valid after pushdown create a pushed down version
		 * of this equivalence list. Remove expressions that are not pushable and
		 * for the other expressions replace var references with target list expressions (or
		 * range table vars).
		 */
		if (list_length(newExprs) != 0)
		{
			newEqui = makeEquivalenceList ();
			newEqui->exprs = newExprs;
			newEqui->constant = equi->constant;

			adaptEquivalenceList (&newEqui, restrInfo);

			/* add new equivalence list to pushdown info and map */
			restrInfo->equiMap[i] = newEqui;
			output->equiLists = lappend(output->equiLists, newEqui);
		}
		else
			restrInfo->equiMap[i] = NULL;
	}

	/*
	 * Examine SelectionInfo expressions and check if they can be kept.
	 */
	foreach(lc, input->conjuncts)
	{
		sel = (SelectionInfo *) lfirst(lc);

		if (selectionIsPushable (sel, restrInfo, input->equiLists))
		{
			newSels = adaptSelectionInfoForPushdown (sel, restrInfo, input->equiLists, output->equiLists, output);

			output->conjuncts = list_union(output->conjuncts, newSels);
		}
	}


}

/*
 * For the special type of disjunctive conditions that arise from specifying more that one
 * result tuple for which the provenance should be computed  use distributive laws to generate
 * a filter condition over the attributes of one relation that can be pushed down.
 */

static void
restrictFilterForDisjunctions (PushdownInfo *input, PushdownInfo *output,  RestricterInfo *restrInfo, ExprHandlers *handlers)
{
	/* this restricter cannot push trough aggregations */
	if(restrInfo->type == PUSHDOWN_AGG)
		return;

	/* search for disjunctions */



}

/*
 *
 */

static void
restrictPushThroughAgg (PushdownInfo *input, PushdownInfo *output,  RestricterInfo *restrInfo, ExprHandlers *handlers)
{
	/* this restricter can only push trough aggregations */
	if(restrInfo->type != PUSHDOWN_AGG)
		return;
}


/*
 * ----------------------------------------------------------------------------------------
 *		Restricter and Rewriter helper functions
 * ----------------------------------------------------------------------------------------
 */

/*
 * Adapt a pushed down equivalence list.
 */

static void
adaptEquivalenceList (EquivalenceList **equi, RestricterInfo *restrInfo)
{
	/* if we are pushing into a subquery replace vars with target list expressions */
	if (restrInfo->type == PUSHDOWN_SUBQUERY)
		*equi = (EquivalenceList *) pushdownVarToTargetExprMutator((Node *) *equi, restrInfo->rte->subquery->targetList);

	/* if we are pushing into a join replace vars with the vars from joinaliasvars */
	if (restrInfo->type == PUSHDOWN_JOIN)
		replaceJoinRTEVarsWalker ((Node *) *equi, restrInfo);
}

/*
 * Replaces Vars that references the join range table entry with the Vars from the joinaliasvars.
 */

static bool
replaceJoinRTEVarsWalker (Node *node, RestricterInfo *context)
{
	if (node == NULL)
		return false;

	if (IsA(node,Var))
	{
		Var *var;
		Var *newVar;

		var = (Var *) node;

		/* if we find a var that references the join range table entry
		 * then replace it with the referenced var from joinaliasvars
		 */
		if (var->varno == context->rtindex)
		{
			newVar = list_nth(context->rte->joinaliasvars, var->varattno - 1);

			var->varno = newVar->varno;
			var->varattno = newVar->varattno;
			var->varlevelsup = newVar->varlevelsup;
			var->varnoold = newVar->varnoold;
			var->varoattno = newVar->varoattno;
		}

		return false;
	}

	return expression_tree_walker(node, replaceJoinRTEVarsWalker, (void *) context);
}

/*
 * Replaces Var nodes in an expression by the expressions from the target list  given as parameter context.
 */

static Node *
pushdownVarToTargetExprMutator (Node *node, List *context)
{
	if (node == NULL)
		return NULL;

	if (IsA(node, Var))
	{
		Var *var;
		TargetEntry *te;

		var = (Var *) node;
		te = (TargetEntry *) list_nth(context, var->varattno - 1);
		return copyObject(te->expr);
	}

	return expression_tree_mutator(node, pushdownVarToTargetExprMutator, (void *) context);
}

/*
 * For a given range table index R return a list with all range table indices of RTE in the subtree under R.
 */

static List *
getSubtreeRTindexes (Query *query, Index rtindex)
{
	List *subtreeRtes;
	Node *joinTreeNode;

	subtreeRtes = NIL;
	joinTreeNode = getJoinTreeNode(query, rtindex);
	getSubtreeRtesWalker(joinTreeNode, &subtreeRtes);

	return subtreeRtes;
}

/*
 *	Checks if an expression can be pushed down.
 */

static bool
isPushable (Node *expr, RestricterInfo *restrInfo)
{
	RestrictableWalkerContext *context;
	bool result = true;

	if (IsA(expr,Const))
		return true;

	context = (RestrictableWalkerContext *) palloc(sizeof(RestrictableWalkerContext));
	context->result = &result;
	context->rtes = restrInfo->rtIndexes;

	isPushableWalker (expr, context);

	pfree(context);

	return result;
}

/*
 * Search for vars in an expression that cannot be pushed down.
 */

static bool
isPushableWalker (Node *node, RestrictableWalkerContext *context)
{
	if (node == NULL)
		return false;

	if (IsA(node,Var))
	{
		Var *var;

		var = (Var *) node;

		/* check if referenced range table entry belongs to the list of rtes of the subtree we are pushing into */
		if (!list_member_int(context->rtes, var->varno))
			*(context->result) = false;

		return false;
	}
	//CHECK need to check for sublinks? etc

	return expression_tree_walker(node, isPushableWalker, (void *) context);
}

/*
 * Checks if an SelectionInfo can be pushedDown.
 */

static bool
selectionIsPushable (SelectionInfo *sel, RestricterInfo *restrInfo, List *equis)
{
	bool result;
	SelectionPushableWalkerContext *context;

	/* exprs containing sublinks or volatile functions should never be pushed */
	if (sel->notMovable)
		return false;

	/* Use Walker to determine if the expr is pushable */
	result = true;

	context = (SelectionPushableWalkerContext *) palloc(sizeof(SelectionPushableWalkerContext));
	context->result = &result;
	context->equis = equis;
	context->restrInfo = restrInfo;

	selectionIsPushableWalker(sel->expr, context);

	pfree(context);

	return result;
}

/*
 * Searches for vars that are valid after pushdown or are contained in
 * an expression that belongs to a equivalence list that is valid after
 * pushdown.
 */

static bool
selectionIsPushableWalker (Node *node, SelectionPushableWalkerContext *context)
{
	if (node == NULL)
		return false;

	if (IsA(node,Var))
	{
		Var *var;

		var = (Var *) node;

		/* if var is valid after pushdown its ok */
		if (list_member_int(context->restrInfo->rtIndexes, var->varno))
			return false;

		/* if not check if it belongs to one equivalence list that is
		 * valid after pushdown.
		 */
		if (getEquivalenceListForExpr(node, context->restrInfo, context->equis) != -1)
			return false;

		/* not pushable stop walker and set result to false */
		*(context->result) = false;

		return true;
	}

	/* check if the subexpression node is an expression of one
	 * of the Equivalence lists that are valid after pushdown.
	 * If yes do not recurse into subtree under node.
	 */
	if(getEquivalenceListForExpr(node, context->restrInfo, context->equis) != -1)
		return false;

	return expression_tree_walker(node, selectionIsPushableWalker, (void *) context);
}

/*
 * Checks if an expression belongs to one ot the equivalence lists that are valid after pushdown.
 */

static int
getEquivalenceListForExpr (Node *expr, RestricterInfo *restrInfo, List *equis)
{
	ListCell *lc;
	int i;
	EquivalenceList *equi;

	foreachi(lc, i, equis)
	{
		equi = (EquivalenceList *) lfirst(lc);

		/* expression belongs to EL */
		if (list_member(equi->exprs, expr))
		{
			/* if EL valid after pushdown return true */
			if (restrInfo->equiMap[i] != NULL)
				return i;

			/* belongs to an EL that is not valid after pushdown -> return false */
			return -1;
		}
	}

	/* belongs to no EL return false */
	return -1;
}

/*
 * Adapts a pushed down SelectionInfo.
 */

static List *
adaptSelectionInfoForPushdown (SelectionInfo *sel, RestricterInfo *restrInfo, List *inputEquis, List *outputEquis, PushdownInfo *info)
{
	List *newSels;
	Node *newExpr;
	AdaptSelectionMutatorContext *context;

	context = (AdaptSelectionMutatorContext *) palloc(sizeof(AdaptSelectionMutatorContext));
	context->restrInfo = restrInfo;
	context->inputEquis = inputEquis;
	context->outputEquis = outputEquis;

	newExpr = adaptSelectionInfoMutator (sel->expr, context);

	//newSels = createSelectionInfoList(newExpr, info, restrInfo->query);

	pfree(context);

	return newSels;
}

/*
 * Inspect a expression tree and replace expressions from equivalence lists EL with one of the expressions
 * from the EL and replace vars with their pushed down equivalent.
 */

static Node *
adaptSelectionInfoMutator (Node *node, AdaptSelectionMutatorContext *context)
{
	int equiPos;
	EquivalenceList *equi;

	if (node == NULL)
		return NULL;

	/* check expression belongs to an equivalence list that is valid after pushdown */
	equiPos = getEquivalenceListForExpr(node, context->restrInfo, context->inputEquis);

	if (equiPos != -1)
	{
		equi = (EquivalenceList *) list_nth(context->outputEquis, equiPos);

		return (Node *) copyObject(linitial(equi->exprs));
	}

	/* replace vars with their pushed down equivalent */
	if (IsA(node, Var))
	{
		Node *var;

		if (context->restrInfo->type == PUSHDOWN_JOIN)
		{
			var = (Node *) copyObject(node);
			replaceJoinRTEVarsWalker (var, context->restrInfo);

			return var;
		}
		else if (context->restrInfo->type == PUSHDOWN_SUBQUERY);
			return pushdownVarToTargetExprMutator (node, context->restrInfo->rte->subquery->targetList);

			//TODO other type what to do?
//		Var *var;
//CHECK unness because if condition not fulfilled we would not have come until here
//		var = (Var *) node;
//
//		if (list_member_int(context->restrInfo->rtIndexes, var->varno))
//
	}

	return expression_tree_mutator(node, adaptSelectionInfoMutator, (void *) context);
}

/*
 *TODO remove
 */

static void
adaptIndices (SelectionInfo *sel, Index rtindex, Query *query)
{
	ListCell *lc;
	List *newVars;
	List *joinVars;
	Var *var;
	RangeTblEntry *rte;

	/* if rte is not a join rte nothing has to be adapted */
	rte = rt_fetch(rtindex, query->rtable);
	if (rte->rtekind != RTE_JOIN)
		return;

	/* get join vars */
	joinVars = (rt_fetch(rtindex, query->rtable))->joinaliasvars;
	newVars = copyObject(sel->vars);

	/* switch vars that reference the join RTE with the join alias vars */
	foreach(lc, newVars)
	{
		var = (Var *) lfirst(lc);

		if (var->varno == rtindex)
		{
			lfirst(lc) = copyObject(list_nth(joinVars,var->varattno));
		}
	}

	/* replace vars in SelectionInfo with the joinVars */
	sel->expr = replaceSubExpression(sel->expr, sel->vars, newVars, REPLACE_SUB_EXPR_ALL);
	sel->vars = newVars;
}

/*
 *	get a list with range table indices of all nodes under node. node should be a node of a join tree.
 */

static void
getSubtreeRtesWalker (Node *node, List **context)
{
	if (node == NULL)
		return;

	if (IsA(node, JoinExpr))
	{
		JoinExpr *join;

		join = (JoinExpr *) node;
		*context = lappend_int(*context, join->rtindex);

		getSubtreeRtesWalker (join->larg, context);
		getSubtreeRtesWalker (join->rarg, context);
	}
	else
	{
		RangeTblRef *rtRef;

		rtRef = (RangeTblRef *) node;
		*context = lappend_int(*context, rtRef->rtindex);
	}
}

///*
// * Compare two expressions. The expressions are considered equal, if they represent exactly the same structure with vars allowed to be
// * different if they belong to the same EqualityList. For the comparison the var nodes are replaced with the EqualityLists they belong to.
// * This is done to be able to use the normal equal for comparison.
// */
//
//static bool
//compareWithEqualityLists (Node *node, CompareWithEqualityListContext *context)
//{
//	if (node == NULL);
//		return false;
//}

/*
 *
 */

static PushdownInfo *
replaceVarsWithEquivalenceLists (PushdownInfo *info)
{
	List *searchVars;
	List *replaceEquis;
	ListCell *lc;
	SelectionInfo *sel;
	Var *var;
	EquivalenceList *equi;
	PushdownInfo *result;

	searchVars = NIL;
	replaceEquis = NIL;

	/* build list with vars in pushdown info */
	foreach(lc, info->conjuncts)
	{
		sel = (SelectionInfo *) lfirst(lc);

		searchVars = list_union(searchVars, sel->vars);
	}

	/* build list with equivalence lists for vars */
	foreach(lc, searchVars)
	{
		var = (Var *) lfirst(lc);
		equi = getEqualityListForExpr (info, (Node *) var);
		replaceEquis = lappend(replaceEquis, equi);
	}

	/* replace equivalence lists with vars */
	result = (PushdownInfo *) replaceSubExpression((Node *) info, searchVars, replaceEquis, REPLACE_SUB_EXPR_ALL || REPLACE_CHECK_REPLACERS);

	return result;
}
