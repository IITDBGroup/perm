/*-------------------------------------------------------------------------
 *
 * prov_restr_pushdown.c
 *	  POSTGRES C
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/prov_restr_pushdown.c,v 1.542 08.12.2008 18:02:40 bglav Exp $
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
#include "utils/guc.h"
#include "utils/memutils.h"

#include "provrewrite/prov_sublink_util_search.h"
#include "provrewrite/prov_sublink_util_analyze.h"
#include "provrewrite/prov_sublink_util_mutate.h"
#include "provrewrite/prov_restr_pushdown.h"
#include "provrewrite/prov_restr_rewrite.h"
#include "provrewrite/prov_restr_util.h"
#include "provrewrite/prov_restr_final.h"
#include "provrewrite/prov_restr_scope.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/provlog.h"



/* prototypes */
static void pushdownQueryNode (Query *query, ExprHandlers *handlers, PushdownInfo *pushdown);
static PushdownInfo *pushdownJoin (PushdownInfo *pushdown, Query *query, ExprHandlers *handlers);
static void pushdownOneJoinOp (PushdownInfo *info, Query *query, Node *joinNode, ExprHandlers *handlers);
static void pushdownRestrictToRangeTblAttrs (PushdownInfo *info, Query *query, Node *joinNode, ExprHandlers *handlers);
static PushdownInfo *pushDownSetOp (PushdownInfo *pushdown, Query *query, ExprHandlers *handlers);
static void pushDownOneSetOp (PushdownInfo *pushdown, Query *query, Node *setOp, ExprHandlers *handlers);
static void pushdownRTE (PushdownInfo *pushdown, Query *query, Index rtindex, ExprHandlers *handlers);
static PushdownInfo *pushDownHaving (PushdownInfo *pushdown, Query *query, ExprHandlers *handlers);

static ExprHandlers *getExprHandlers (void);
static bool replaceVarVarno (Node *node, Index *context);


/*
 * pushdown restriction for provenance computation. If a user wants to compute the provenance for a subset of a query's result we try
 * to pushdown these restriction as far as possible into the provenance query.
 */

Query *
pushdownSelections (Query *query)
{
	MemoryContext pushdownMem;

	if(!prov_use_selection_pushdown || IsProvRewrite(query))
		return query;

	/* create memory context and set as default for memory allocation */
	pushdownMem = AllocSetContextCreate(CurrentMemoryContext,
			  "pushdown memory context",
			  ALLOCSET_DEFAULT_MINSIZE,
			  ALLOCSET_DEFAULT_INITSIZE,
			  ALLOCSET_DEFAULT_MAXSIZE);
	MemoryContextSwitchTo(pushdownMem);

	/* generate data structures used for selection pushdown */
	generateQueryPushdownInfos(query);

	/* rewrite and move around selections */
	//pushdownQueryNode(query, getExprHandlers(), NULL);
	LOGNODE(query,"selections pushed down");

	/* replace query selections with generated selections */
	finalizeSelections(query);

	/* free context and switch back to memory context used before*/
	MemoryContextSwitchTo(pushdownMem->parent);
	MemoryContextDelete(pushdownMem);

	return query;
}

/*
 * try to pushdown the selections given as parameter pushdown into the Query node query.
 */

static void
pushdownQueryNode (Query *query, ExprHandlers *handlers, PushdownInfo *pushdown)
{
	/* if we have proven that parent pushdown expression is an contradiction stop pushing down */
	if(pushdown->contradiction)
		return;

	/* query node is a set operation */
	if (query->setOperations)
	{
		pushdown = handlers->setPusher (pushdown, query, handlers);
	}
	/* query node is a ASPJ query */
	else
	{
		if (query->havingQual)
			pushdown = handlers->havingPusher (pushdown, query, handlers);

		handlers->joinPusher (pushdown, query, handlers);
	}
}


/*
 * Try to pushdown a selection through a having clause.
 */

static PushdownInfo *
pushDownHaving (PushdownInfo *pushdown, Query *query, ExprHandlers *handlers)
{
	ListCell *lc;
	SelectionInfo *sel;
//	PushdownInfo *result;
//
//	/* if we have proven that parent pushdown expression is an contradiction stop pushing down */
//	if(pushdown->contradiction)
//		return pushdown;
//
//	/* add having qual to pushdown and rewrite expr */
//	addExprToPushdownWithRewrite(pushdown, query->havingQual, query, handlers);
//
//	/* createNewHavingQual */
//	query->havingQual = createQual(pushdown, false);

	/* restrict pushdown conjuncts */
	//	result = makePushdownInfo();

	/* search for conjuncts that can be pushed down */
	foreach(lc, pushdown->conjuncts)
	{
		sel = (SelectionInfo *) lfirst(lc);

		/*
		 * check if conjunct contains only group by attributes and no sublinks,
		 * or volatile functions and is not a constant expr
		 */
//		if (SelNoSubOrVolatile(sel) && sel->vars)
//		{
//			/* an expr solely over group by attributes can be pushed down */
//			if(!sel->containsAggs)
//			{
//				result->conjuncts = lappend(result->conjuncts, sel);
//			}
			/* check if is a mix or just aggregates */
			//TODO check for special aggregate expr that can be pushed down (should be handled by restricters anyway?)
//		}
	}

	//createEquivalenceClasses(result);

	//TODO restricter for aggregations

	return NULL;
}

/*
 * Try to pushdown a selection trough a join tree.
 */

static PushdownInfo *
pushdownJoin (PushdownInfo *pushdown, Query *query, ExprHandlers *handlers)
{
//	ListCell *lc;
//	Node *fromItem;
//	PushdownInfo *newPushdown;
//
//	/* if we have an contradiction we can stop pushing down */
//	if (pushdown->contradiction)
//		return pushdown;
//
//	/* copy PushdownInfo */
//	newPushdown = copyObject(pushdown);
//
//	/* query has a top level qual */
//	if (query->jointree->quals)
//	{
//		addExprToPushdownWithRewrite(newPushdown, query->jointree->quals, query, handlers);
//		query->jointree->quals = createQual(newPushdown, false);
//		setVolatileAndSubsRedundend(newPushdown);
//	}
//
//	/* process each item in the from list */
//	foreach(lc, query->jointree->fromlist)
//	{
//		fromItem = (Node *) lfirst(lc);
//
//		pushdownOneJoinOp (newPushdown, query, fromItem, handlers);
//	}

	return NULL;
}

/*
 * Try to pushdown a selection trough one node in a join tree (a join expression or a range table reference).
 */

static void
pushdownOneJoinOp (PushdownInfo *info, Query *query, Node *joinNode, ExprHandlers *handlers)
{
	JoinExpr *join;
	PushdownInfo *newInfo;

	/* if we have an contradiction we can stop pushing down */
//	if (info->contradiction)
//		return;
//
//	/* if join tree node is a range table entry push selection into range table entry */
//	if (IsA(joinNode, RangeTblRef))
//	{
//		pushdownRestrictToRangeTblAttrs(info, query, joinNode, handlers);
//		return;
//	}
//	/* join tree node is a join expression */
//	join = (JoinExpr *) joinNode;
//
//	/* restrict expression to rangetbl attributes */
//	newInfo = restrictExpr(info, query, join->rtindex, handlers);
//
//	/* if newInfo including join qual is an contradiction stop pushing */
//	if (newInfo->contradiction)
//	{
//		join->quals = createQual(newInfo, false);
//		return;
//	}
//
//	/* add join->qual to PushdownInfo, rewrite and generate new joinQual.
//	 * If possible pushdown expression into child nodes
//	 */
//	switch(join->jointype)
//	{
//		case JOIN_INNER:
//		case JOIN_LEFT:
//		case JOIN_RIGHT:
//		case JOIN_FULL:
//			/* add join qualification to expressions */
//			addExprToPushdownWithRewrite(newInfo, join->quals, query, handlers);
//			join->quals = createQual(newInfo, false);
//			setVolatileAndSubsRedundend(newInfo);
//
//			if (!newInfo->contradiction)
//			{
//			/* push into children */
//			pushdownRestrictToRangeTblAttrs(newInfo, query, join->larg, handlers);
//			pushdownRestrictToRangeTblAttrs(newInfo, query, join->rarg, handlers);
//			}
//			break;
//			/*
//			 * TODO check for cases were nullable side is compared to null, because from such an condition together with an
//			 * Equivalence Class (in this case from the join qual)
//			 * we normally derive false. In this case we just have to remove this condition, add a method to do this.
//			 * As an alternative we could derive stuff from different conjuncts at once and add the condition (a = b OR b IS NULL) which
//			 * is fullfilled for every tuple of the LEFT JOIN result
//			 */
////			break;
////			break;
////			break;
//		default:
//			break;
//	}
}

/*
 * Restrict a selection to the parts of the selection expression that are applicable for one range tbl entry
 */

static void
pushdownRestrictToRangeTblAttrs (PushdownInfo *info, Query *query, Node *joinNode, ExprHandlers *handlers)
{
	Index rtindex;

	/* get the range table index for the join tree node */
	rtindex = IsA(joinNode, RangeTblRef) ? ((RangeTblRef *) joinNode)->rtindex: ((JoinExpr *) joinNode)->rtindex;

	/* if join tree node is a join expression call pushdownOneJoinOp */
	if (IsA(joinNode, JoinExpr))
	{
		pushdownOneJoinOp(info, query, joinNode, handlers);
		return;
	}
	/* if join tree node is a range table reference call pushdownRTE */
	pushdownRTE (info, query, rtindex, handlers);
}


/*
 * Try to pushdown a selection through a set operation tree.
 */

static PushdownInfo *
pushDownSetOp (PushdownInfo *pushdown, Query *query, ExprHandlers *handlers)
{
	PushdownInfo *newInfo;

	newInfo = copyObject(pushdown);
	rewriteExpr(newInfo, handlers);

	pushDownOneSetOp (newInfo, query, query->setOperations, handlers);

	return NULL;
}

/*
 * Try to pushdown a selection trough one node of a set operation tree
 */

static void
pushDownOneSetOp (PushdownInfo *pushdown, Query *query, Node *setOp, ExprHandlers *handlers)
{
	SetOperationStmt *op;

	/*
	 * set operation tree node is a range table reference. push selection into the range
	 * table entries query.
	 */
	if (IsA(setOp, RangeTblRef))
	{
		RangeTblRef *rtRef;

		rtRef = (RangeTblRef *) setOp;
		replaceVarVarno((Node *) pushdown, (Index *) &(rtRef->rtindex));

		pushdownRTE(pushdown, query, rtRef->rtindex, handlers);
		return;
	}

	/*
	 * set operation tree node is a set operation. Push selection into the child nodes (depending on type of set operation)
	 */
	op = (SetOperationStmt *) setOp;

	/* union or intersection. Push selection into both children of the set op node */
	if (op->op == SETOP_UNION || op->op == SETOP_INTERSECT)
	{
		//TODO add new equivalence lists for intersection or union
		pushDownOneSetOp (pushdown, query, op->larg, handlers);
		pushDownOneSetOp (pushdown, query, op->rarg, handlers);
	}
	/* set difference. Push only into the left child */
	else
	{
		pushDownOneSetOp (pushdown, query, op->larg, handlers);
	}
}

/*
 * Change the Varno of all Vars contained in an expression into the provided index.
 */

static bool
replaceVarVarno (Node *node, Index *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, Var))
	{
		Var *var;

		var = (Var *) node;
		var->varno = *context;
	}

	return expression_tree_walker(node, replaceVarVarno, (void *) context);
}



/*
 * Try to push a selection into a range table entry.
 */

static void
pushdownRTE (PushdownInfo *pushdown, Query *query, Index rtindex, ExprHandlers *handlers)
{
	RangeTblEntry *rte;
	PushdownInfo *newPushdown;

	rte = rt_fetch(rtindex, query->rtable);

	/* range table is a subquery. Pushdown selection conditions into subquery */
	if (rte->rtekind == RTE_SUBQUERY)
	{
		/* replace var entries with target list expr */
		newPushdown = restrictExpr(pushdown, query, rtindex, handlers);
		//recreateSelFromExprs(newPushdown);
		//setVolatileAndSubsRedundend(newPushdown);

		/* pushdown selection into query node */
		pushdownQueryNode(rte->subquery, handlers, newPushdown);
	}
	/* range table entry is a relation */
	else if (rte->rtekind == RTE_RELATION)
	{
		RangeTblEntry *newRte;
		Query *newSub;

		/* generate subquery that encapsulates range table entry rte */
		newSub = generateQueryFromBaseRelation (rte);

		/* generate new range table entry for subquery */
		newRte = makeRte (RTE_SUBQUERY);
		SetRteNames(newRte,"NewSelectionPushdownQueryNode");
		newRte->subquery = newSub;
		correctRTEAlias(newRte);

		/* replace rte with new range table entry */
		list_nth_cell(query->rtable, rtindex - 1)->data.ptr_value = (void *) newRte;

		/* adapt restriction and apply it to the new subquery */
		newPushdown = restrictExpr(pushdown, query, rtindex, handlers);
		//newSub->jointree->quals = createQual(newPushdown, false);
	}

}

/*
 * Returns the ExprHandler data structure. Which rewriters and restricters are included is determined by options set in the
 * GUC.
 */

static ExprHandlers *
getExprHandlers ()
{
	ExprHandlers *handler;

	handler = (ExprHandlers *) palloc(sizeof(ExprHandlers));

	/* set rewriters */
	handler->rewriters = getRewriters();

	/* set restrictors */
	handler->restricters = getRestricters();

	/* set algebra operator handlers */
	handler->setPusher = pushDownSetOp;
	handler->joinPusher = pushdownJoin;
	handler->havingPusher = pushDownHaving;

	return handler;
}

