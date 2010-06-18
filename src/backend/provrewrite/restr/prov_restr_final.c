/*-------------------------------------------------------------------------
 *
 * prov_restr_final.c
 *	  Use the QueryPushdownNodes to create final selection conditions and add them to query quals
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/prov_restr_final.c,v 1.542 05.02.2009 13:58:26 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "nodes/makefuncs.h"
#include "utils/palloc.h"
#include "nodes/memnodes.h"
#include "parser/parsetree.h"

#include "provrewrite/prov_restr_final.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/provstack.h"

/* prototypes */
static void generateQualPointers (QueryPushdownInfo *info);
static void generateQualPointerForNode (Node **node, QueryPushdownInfo *info);
static void cleanUp (Query *query);
static void *copyObjectNorm(void *obj);
//static Node *createQual (PushdownInfo *info, bool includeRedundend);
static void finalizeQueryPushdown (QueryPushdownInfo *info);
static void finalizeAggScope (Query *query, SelScope *scope);
static void finalizeSelScope (Query *query, SelScope *scope);
static void addExprToQual (Query *query, Node *newQual, Node **qualPointer);
static void implementSelection (SelectionInfo *sel, SelScope *scope);
static Index getLCP (Bitmapset *indices, SelScope *scope);
static Node **getQualPointer (Index rtindex, SelScope *scope, SelectionInfo *sel);
static Node **generateSubqueryForSelection (Query *, RangeTblEntry *rte, Index rtindex, SelectionInfo *sel);

/*
 * Uses the derived selection conditions stored in QueryPushdownInfo objects to
 * create selection conditions for query. Afterwards disconnect the QueryPushdownInfo structure
 * from the query nodes (so it can be safely destroyed).
 */

void
finalizeSelections (Query *query)
{
	QueryPushdownInfo *info;

	info = getQueryInfo(query);

	/* generate pointers to query qual nodes */
	generateQualPointers (info);

	/* generate selection conditions */
	finalizeQueryPushdown (info);

	/* remove pointer from queries to QueryPushdownInfos */
	MemoryContextSwitchTo(CurrentMemoryContext->parent);
	cleanUp(query);
}

/*
 * Generate list with pointers to quals for each range table entry
 */

static void
generateQualPointers (QueryPushdownInfo *info)
{
	ListCell *lc;
	QueryPushdownInfo *child;
	Node *fromItem;

	info->qualPointers = generateDuplicatesList(NULL, list_length(info->query->rtable) + 1);

	/* create pointer for top qual */
	replaceNth (info->qualPointers, &(info->query->jointree->quals), 0);
	info->query->jointree->quals = NULL;

	/* create pointers for each from item */
	foreach(lc, info->query->jointree->fromlist)
	{
		fromItem = (Node *) lfirst(lc);

		generateQualPointerForNode (&fromItem, info);
	}

	/* create Quals pointers for children */
	foreach(lc, info->children)
	{
		child = (QueryPushdownInfo *) lfirst(lc);

		generateQualPointers(child);
	}
}

/*
 *
 */

static void
generateQualPointerForNode (Node **node, QueryPushdownInfo *info)
{
	if (IsA(*node, RangeTblRef))
	{
		RangeTblRef *rtRef;

		rtRef = (RangeTblRef *) *node;
		replaceNth(info->qualPointers, node, rtRef->rtindex);
	}
	else
	{
		JoinExpr *join;

		join = (JoinExpr *) *node;

		replaceNth(info->qualPointers, &(join->quals), join->rtindex);
		join->quals = NULL;

		generateQualPointerForNode(&(join->larg), info);
		generateQualPointerForNode(&(join->rarg), info);
	}
}

/*
 *
 */

static void
finalizeQueryPushdown (QueryPushdownInfo *info)
{
	QueryPushdownInfo *child;
	ListCell *lc;

	/* finalize scopes */
	if (info->topScope->scopeType == SCOPE_AGG)
		finalizeAggScope(info->query, info->topScope);
	else
		finalizeSelScope(info->query, info->topScope);

	/* finalize children */
	foreach(lc, info->children)
	{
		child = (QueryPushdownInfo *) lfirst(lc);

		finalizeQueryPushdown(child);
	}
}

/*
 *
 */

static void
finalizeAggScope (Query *query, SelScope *scope)
{
	ListCell *lc;
	SelectionInfo *sel;
	Node **qualPointer;

	qualPointer = &(query->havingQual);

	/* add all selection conditions to having qual */
	foreach(lc, scope->selInfos)
	{
		sel = (SelectionInfo *) lfirst(lc);

		addExprToQual (query, sel->expr, qualPointer);
	}

	/* proceed with join scopes */
	finalizeSelScope(query, (SelScope *) linitial(scope->children));
}

/*
 * finalize the selections for one SelScope.
 */

static void
finalizeSelScope (Query *query, SelScope *scope)
{
	ListCell *lc;
	SelectionInfo *sel;
	//EquivalenceList *eq;
	Node **qualPointer;
	SelScope *child;

	/* process selection infos */
	foreach(lc, scope->selInfos)
	{
		sel = (SelectionInfo *) lfirst(lc);

		/* if sel is not moveable create qual at origin of sel */
		if (sel->notMovable)
		{
			qualPointer = getQualPointer(sel->rtOrigin, scope, sel);
			addExprToQual (query, sel->expr, qualPointer);
		}
		/* sel is moveable place it as far down as possible */
		else
		{
			implementSelection (sel, scope);
		}
	}

//	/* process equivalence lists */
//	foreach(lc, scope->equiLists)
//	{
//		eq = (EquivalenceList *) lfirst(lc);
//	}

	/* finalize children */
	foreach(lc, scope->children)
	{
		child = (SelScope *) lfirst(lc);

		finalizeSelScope(query, child);
	}

}


/*
 * Search for the lowest place in the join tree where we can add a selection and
 * add the selection there.
 */

static void
implementSelection (SelectionInfo *sel, SelScope *scope)
{
	Bitmapset *selRTindices = NULL;
	ListCell *lc;
	Var *var;
	Index lcpIndex;
	Node **qualPointer;

	/* generate set of range table entries used in the selection */
	foreach(lc, sel->vars)
	{
		var = (Var *) lfirst(lc);

		selRTindices = bms_add_member(selRTindices, var->varno);
	}

	/* get LCP of selRTindices and implement selection */
	lcpIndex = getLCP(selRTindices, scope);

	qualPointer = getQualPointer(lcpIndex, scope, sel);

	addExprToQual(scope->pushdown->query, sel->expr, qualPointer);
}

/*
 * Get the LCP (lowest common parent) were all range table entries from "indices" are valid in this scope.
 */

static Index
getLCP (Bitmapset *indices, SelScope *scope)
{
	ListCell *lc;
	Bitmapset *curSet;
	Index i, result;
	int resultNumMembers;
	QueryPushdownInfo *info;

	info = scope->pushdown;
	resultNumMembers = list_length(info->query->rtable) + 2;
	result = -1;

	/* Search through the list of sets of valid rtes for each rtindex. Each rte for which
	 * its set is a super-set of the indicies is a candidate to implement the selection condition.
	 * The super-set with the lowest number of members is the LCP.
	 */
	foreachi(lc, i, info->validityScopes)
	{
		if (!bms_is_member((i + 1), scope->childRTEs))
			continue;

		curSet = (Bitmapset *) lfirst(lc);

		if(bms_is_subset(indices, curSet))
		{
			/* if the current candidate set has less member the last candidate or the last candidate is
			 * a base relation, then this is the current candidate is the best one so far
			 */
			if (bms_num_members(curSet) < resultNumMembers || resultNumMembers == 1)
			{
				/* if we have a single member set (a base rel) this only a candidate if this is the only candidate */
				if (!(result != -1 && bms_num_members(curSet) == 1))
				{
					result = i + 1;
					resultNumMembers = bms_num_members(curSet);
				}
			}
		}
	}

	if (result == -1 && bms_is_member(0, scope->joinRTEs))
		result = 0;

	//TODO error in case not found

	return result;
}

/*
 *
 */

static Node **
getQualPointer (Index rtindex, SelScope *scope, SelectionInfo *sel)
{
	Node **result;
	List *indices;
	QueryPushdownInfo *info;
	RangeTblEntry *rte;

	info = scope->pushdown;

	/* get Qual pointer */
	result = (Node **) list_nth(info->qualPointers, rtindex);

	/*
	 * if it is a range table ref check if we can use a join or
	 * where qual that is a direct parent of ther rt-Ref. If
	 * the direct parent is an outer join transform range table
	 * into query. (If RTE is a simple select subquery,
	 * just return the where-qual of this query.
	 */
	if (*result != NULL && IsA(*result, RangeTblRef))
	{
		indices = NIL;

		/* get path to rtRef in join tree */
		if(findRTindexInFrom(rtindex, info->query, &indices, NULL))
		{
			/* if rtRef is direct child of the top qual return qual pointer to top qual
			 * and top qual belongs to the scope, then return top qual pointer.
			 */
			if (list_length(indices) == 1 && bms_is_member(0,scope->joinRTEs))
				return (Node **) linitial(info->qualPointers);

			/* If the direct parent of rtRef belongs to the scope and is a INNER join,
			 * then return a pointer to this node
			 */
			if (list_length(indices) > 1)
			{
				Index joinRTindex;
				JoinExpr *join;

				joinRTindex = list_nth_int(indices, list_length(indices) - 2);

				if(bms_is_member(joinRTindex, scope->joinRTEs))
				{
					result = (Node **) list_nth(info->qualPointers, joinRTindex);
					join = (JoinExpr *) *result;

					if(join->jointype == JOIN_INNER)
						return result;
				}
			}

			/* check if rte is a simple SPJ query, if it is then return the top qual */
			rte = rt_fetch(rtindex, info->query->rtable);

			if (rte->rtekind == RTE_SUBQUERY)
			{
				Query *subquery;
				QueryPushdownInfo *subInfo;

				subquery = rte->subquery;

				if (subquery->hasAggs == false && !(subquery->setOperations))
				{
					subInfo = (QueryPushdownInfo *) subquery->provInfo;
					return (Node **) linitial(subInfo->qualPointers);
				}
			}

			/* no chance, we have to create a subquery for the selection condition */
			result = generateSubqueryForSelection(info->query, rte, rtindex, sel);
		}
	}

	return result;
}

/*
 *
 */

static Node **
generateSubqueryForSelection (Query *query, RangeTblEntry *rte, Index rtindex, SelectionInfo *sel)
{
	RangeTblEntry *newRte;
	Query *newSub;

	/* generate subquery that encapsulates range table entry rte. */
	newSub = generateQueryFromBaseRelation (rte);	//CHECK that it works for subqueries too.

	/* generate new range table entry for subquery */
	newRte = makeRte (RTE_SUBQUERY);
	SetRteNames(newRte,"NewSelectionPushdownQueryNode");
	newRte->subquery = newSub;
	correctRTEAlias(newRte);

	/* replace rte with new range table entry */
	list_nth_cell(query->rtable, rtindex - 1)->data.ptr_value = (void *) newRte;

	/* adapt restriction and apply it to the new subquery by adapting the vars of selection */
	//TODO

	/* return pointer to the new queries qual */
	return &(newSub->jointree->quals);
}

/*
 *
 */

static void
addExprToQual (Query *query, Node *newQual, Node **qualPointer)
{
	BoolExpr *and;

	/* qual was NULL before */
	if (*qualPointer == NULL)
	{
		*qualPointer = copyObjectNorm(newQual);
	}
	/* top level node of qual is boolean AND */
	else if (IsA(*qualPointer, BoolExpr) && ((BoolExpr *) *qualPointer)->boolop == AND_EXPR)
	{
		and = (BoolExpr *) *qualPointer;
		and->args = lappend(and->args, copyObjectNorm(newQual));
	}
	/* top level node is something else */
	else
	{
		and = (BoolExpr *) makeBoolExpr (AND_EXPR, list_make2(newQual, *qualPointer));
		*qualPointer = (Node *) copyObjectNorm(and);
	}
}

///*
// * Create a single expression from the conjuncts list of a pushdown info node.
// */
//
//static Node *
//createQual (PushdownInfo *info, bool includeRedundend)
//{
//	Node *result;
//	SelectionInfo *sel;
//	ListCell *lc;
//	ListCell *innerLc;
//	List *conjExprs;
//	Node *leftExpr;
//	Node *rightExpr;
//	EquivalenceList *equi;
//
//	/* if info is null or has no conjuncts return null */
//	if (info == NULL || (info->conjuncts == NIL && info->equiLists == NIL))
//		return NULL;
//
//	/* if we have already proven that the pushdown expression is an conradiction return false */
//	if (info->contradiction)
//		return makeBoolConst(false,false);
//
//	/* walk through conjuncts and create list with conjunct exprs that are not redundend */
//	conjExprs = NIL;
//
//	foreach(lc, info->conjuncts)
//	{
//		sel = (SelectionInfo *) lfirst(lc);
//
//		//if (includeRedundend || !sel->redundend)
//			conjExprs = lappend(conjExprs, copyObject(sel->expr));
//	}
//
//	/* create equalities for equivalence lists */
//	foreach(lc,info->equiLists)
//	{
//		equi = (EquivalenceList *) lfirst(lc);
//
//		if (list_length(equi->exprs) > 1)
//		{
//			leftExpr = (Node *) linitial(equi->exprs);
//
//			foreachsince(innerLc, equi->exprs->head->next)
//			{
//				rightExpr = (Node *) lfirst(innerLc);
//				conjExprs = lappend(conjExprs, createEqualityConditionForNodes(leftExpr, rightExpr));
//			}
//		}
//	}
//
//	/* all conjuncts are redundent and no equivalence lists -> return NULL */
//	if (conjExprs == NIL)
//		return NULL;
//
//	/* return ANDed list of conjunct expressions */
//	if (list_length(conjExprs) == 1)
//		return linitial(conjExprs);
//
//	result = (Node *) makeBoolExpr(AND_EXPR, conjExprs);
//
//	return result;
//}


/*
 * Switch to normal memory context and create an copy of obj in this
 * context. Switch back afterwards. This method is used to create the
 * selection conditions used in the query structure. We need to switch context, because
 * all auxiliary structure used in the selection pushdown code is allocated in a separate
 * memory context.
 */

static void *
copyObjectNorm(void *obj)
{
	MemoryContext old;
	void *copyObj;

	/* switch to parent context */
	old = MemoryContextSwitchTo(CurrentMemoryContext->parent);

	/* copyObject allocating memory in parent context */
	copyObj = copyObject(obj);

	/* switch back to child context */
	MemoryContextSwitchTo(old);

	return copyObj;
}

/*
 * Removes the links from queries to pushdown infos. Pushdown info are not free'd because
 * we destroy the whole memory context they are allocated in after selection pushdown.
 */

static void
cleanUp (Query *query)
{
	ListCell *lc;
	QueryPushdownInfo *child;
	QueryPushdownInfo *info;

	/* get query pushdown info and remove link to it */
	info = getQueryInfo(query);
	query->provInfo = (Node *) makeProvInfo();

	/* clean up children */
	foreach(lc, info->children)
	{
		child = (QueryPushdownInfo *) lfirst(lc);

		cleanUp(child->query);
	}
}
