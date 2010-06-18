/*-------------------------------------------------------------------------
 *
 * prov_restr_scope.c
 *	  POSTGRES C
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/prov_restr_scope.c,v 1.542 03.02.2009 13:50:29 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "nodes/bitmapset.h"
#include "nodes/parsenodes.h"
#include "parser/parsetree.h"

#include "provrewrite/prov_restr_scope.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_restr_util.h"

/* prototypes */
static void generateQueryNodePushdownInfo (Query *query, QueryPushdownInfo *parent);
static void generateScopes (Query *query);
static void generateScopeList (QueryPushdownInfo *info, SelScope *scope);
static void createAggScope (Query *query);
static void createJoinScope (Query *query);
static void createOneJoinScope (Query *query, Node *joinNode, SelScope *currentScope);
static void createSetOpScopes (Query *query);
static SelScope *createOneSetOpScope (Query *query, Node *setOp);

/* helper functions */
static void addQualToScope (SelScope *scope, Node *qual, Index rtIndex);
static void addRtindexToScope (SelScope *scope, Index rtIndex, bool join);
static void computeBaseRelMaps (QueryPushdownInfo *info);
static void computeSelScopeBaseRelMaps (SelScope *scope);
static void addBaseRelChildren (Node *joinNode, SelScope *scope);
static Index getLowestCommonJoinTreeNode(Index startIndex, Bitmapset *nodeSet, Index member, Query *query);
static Index getLowestMemberOnPath (Node *join, Index member, Bitmapset *nodeSet);
static void generateValidityScopes (QueryPushdownInfo *info);
static void generateIndexSetForNode (Node *node, Bitmapset **set);

/*
 *
 */

void
generateQueryPushdownInfos (Query *query)
{
	generateQueryNodePushdownInfo (query, NULL);

	computeBaseRelMaps (getQueryInfo(query));

	generateValidityScopes (getQueryInfo(query));
}

/*
 * Create the QueryPushdownInfo for a query node.
 */

static void
generateQueryNodePushdownInfo (Query *query, QueryPushdownInfo *parent)
{
	QueryPushdownInfo *newInfo;

	/* create PushdownInfo */
	newInfo = makeQueryPushdownInfo ();
	newInfo->query = query;

	if (parent != NULL)
	{
		newInfo->parent = parent;
		parent->children = lappend(parent->children, newInfo);
	}

	query->provInfo = (Node *) newInfo;

	/* generate the scopes */
	generateScopes (query);

	/* generate list of scopes */
	generateScopeList (newInfo, newInfo->topScope);
}

/*
 * Creates the list of scopes of an QueryPushdownInfo by traversing the Scope tree starting at the top scope.
 */

static void
generateScopes (Query *query)
{
	/* if query contains aggregations add scope for the result of the aggregation */
	if (query->hasAggs)
	{
		createAggScope(query);
	}
	else if (query->setOperations)
	{
		createSetOpScopes (query);
	}
	else
	{
		createJoinScope (query);
	}
}

/*
 * Creates the list of scopes of an QueryPushdownInfo by traversing the Scope tree starting at the top scope.
 */

static void
generateScopeList (QueryPushdownInfo *info, SelScope *scope)
{
	ListCell *lc;
	SelScope *child;

	info->scopes = lappend(info->scopes, scope);

	foreach(lc, scope->children)
	{
		child = (SelScope *) lfirst(lc);

		generateScopeList(info, child);
	}
}

/*
 *
 */

static void
createAggScope (Query *query)
{
	SelScope *newScope;

	newScope = makeSelScope ();
	newScope->scopeType = SCOPE_AGG;
	newScope->pushdown = getQueryInfo(query);
	addRtindexToScope(newScope, RTINDEX_TOPQUAL, true);

	addQualToScope (newScope, query->havingQual, RTINDEX_TOPQUAL);

	getQueryInfo(query)->topScope = newScope;

	createJoinScope(query);
}

/*
 *
 */

static void
createJoinScope (Query *query)
{
	SelScope *topScope;
	ListCell *lc;
	Node *currentJoin;

	topScope = makeSelScope ();
	topScope->pushdown = getQueryInfo(query);
	addRtindexToScope(topScope, RTINDEX_TOPQUAL, true);

	addQualToScope (topScope, query->jointree->quals, RTINDEX_TOPQUAL);

	foreach(lc, query->jointree->fromlist)
	{
		currentJoin = (Node *) lfirst(lc);

		createOneJoinScope (query, currentJoin, topScope);
	}

	if (getQueryInfo(query)->topScope)
	{
		topScope->parent = getQueryInfo(query)->topScope;
		topScope->parent->children = list_make1(topScope);
	}
	else
		getQueryInfo(query)->topScope = topScope;
}

/*
 * Process a single node from a queries join tree. Depending on the type of node, we either add the node and its qual to the
 * current scope or create a new scope below the current scope:
 * 		INNER JOIN: add node to current scope
 * 		OUTER JOIN: create a new scope for join node and two scopes for its left and right child
 * 		RANGE TABLE REF: add node to current scope
 *
 * Depending on the type of outer join (LEFT, RIGHT, FULL), the type of the join scopes child scopes differ. The nullable side of an
 * outer join is typed SCOPE_NULLABLE_OUTER. A not nullable side is typed SCOPE_NONNULL_OUTER.
 */

static void
createOneJoinScope (Query *query, Node *joinNode, SelScope *currentScope)
{
	JoinExpr *join;
	SelScope *newScope;
	SelScope *childScope;

/* convenience macro to create a child scope */
#define createChildScope(childScopeType, childNode) \
	do { \
		childScope = makeSelScope (); \
		childScope->scopeType = (childScopeType); \
		childScope->parent = newScope; \
		childScope->pushdown = newScope->pushdown; \
		newScope->children = lappend(newScope->children, childScope); \
		createOneJoinScope (query, (childNode), childScope); \
	} while (0)

/* convenience macro to create scope for outer join */
#define createOuterScope(outerType) \
	do { \
		if (currentScope->scopeType == SCOPE_NULLABLE_OUTER || currentScope->scopeType == SCOPE_NONNULL_OUTER) \
		{ \
			currentScope->scopeType = (outerType); \
			newScope = currentScope; \
		} \
		else { \
			newScope = makeSelScope (); \
			newScope->parent = currentScope; \
			newScope->scopeType = (outerType); \
			newScope->pushdown = currentScope->pushdown; \
			currentScope->children = lappend(currentScope->children, newScope); \
		} \
		addRtindexToScope(newScope, join->rtindex, true); \
	} while (0)

	/* node is a range tbl reference, add it to current scope */
	if (IsA(joinNode, RangeTblRef))
	{
		RangeTblRef *rtRef;
		RangeTblEntry *rte;

		rtRef = (RangeTblRef *) joinNode;
		addRtindexToScope(currentScope, rtRef->rtindex, false);

		/* fetch range table entry and check if it is a subquery */
		rte = (RangeTblEntry *) rt_fetch(rtRef->rtindex, query->rtable);

		if (rte->rtekind == RTE_SUBQUERY)
			generateQueryNodePushdownInfo (rte->subquery, currentScope->pushdown);
		//TODO set pushdown info on new children
		return;
	}

	/* its a join node. Process with code for different join types */
	join = (JoinExpr *) joinNode;

	/* an inner join, add join to current scope */
	if (join->jointype == JOIN_INNER)
	{
		addRtindexToScope(newScope, join->rtindex, true);
		addQualToScope(currentScope, join->quals, join->rtindex);

		/* create scopes for children of join node */
		createOneJoinScope (query, join->larg, currentScope);
		createOneJoinScope (query, join->rarg, currentScope);

		return;
	}

	/* its an outer join, first check that current is not a below outer join
	 * type
	 */
	switch(join->jointype)
	{
	/* its a left outer join */
	case JOIN_LEFT:
		createOuterScope(SCOPE_OUTERLEFT);

		createChildScope (SCOPE_NULLABLE_OUTER,join->larg);
		createChildScope (SCOPE_NONNULL_OUTER,join->rarg);

		break;
	/* a right outer join */
	case JOIN_RIGHT:
		createOuterScope(SCOPE_OUTERRIGHT);

		createChildScope (SCOPE_NONNULL_OUTER,join->larg);
		createChildScope (SCOPE_NULLABLE_OUTER,join->rarg);

		break;
	/* a full outer join */
	case JOIN_FULL:
		createOuterScope(SCOPE_OUTERFULL);

		createChildScope (SCOPE_NULLABLE_OUTER,join->larg);
		createChildScope (SCOPE_NULLABLE_OUTER,join->rarg);

		break;
	default:
		//TODO Error
		break;
	}

	addQualToScope (newScope, join->quals, join->rtindex);
}

/*
 *
 */

static void
createSetOpScopes (Query *query)
{

}

/*
 *
 */

static SelScope *
createOneSetOpScope (Query *query, Node *setOp)
{
	SelScope *newScope;

	newScope = makeSelScope ();

	return NULL;
}

/*
 * add a qualification to a scope.
 */

static void
addQualToScope (SelScope *scope, Node *qual, Index rtIndex)
{
	addExprToScope (scope, qual, scope->pushdown->query, rtIndex);
}

/*
 *
 */

static void
addRtindexToScope (SelScope *scope, Index rtIndex, bool join)
{
	/* if rtIndex is the first index we add to the scope, then set it as top index */
	if(bms_num_members(scope->joinRTEs) == 0 && bms_num_members(scope->baseRTEs) == 0)
		scope->topIndex = rtIndex;

	/* add it to join or base rel index sets */
	if (join)
		scope->joinRTEs = bms_add_member(scope->joinRTEs, rtIndex);
	else
		scope->baseRTEs = bms_add_member(scope->baseRTEs, rtIndex);

	scope->childRTEs = bms_add_member(scope->childRTEs, rtIndex);
}


/*
 * Merges two scope data structures. The left scope is modified to include the
 * information from the right scope.
 */

void
mergeScopes (SelScope *left, SelScope *right)
{
	/* to merge two selScopes from different queries doesn't make any sense */
	Assert(left->pushdown == right->pushdown
			&& left->parent == right->parent
			&& left->scopeType == right->scopeType);

	/* merge data structures */
	left->children = list_union (left->children, right->children);
	left->baseRTEs = bms_join(left->baseRTEs, right->baseRTEs);
	left->childRTEs = bms_join(left->childRTEs, right->childRTEs);
	left->contradiction = left->contradiction || right->contradiction;
	left->equiLists = list_union (left->equiLists, right->equiLists);
	left->joinRTEs = bms_join(left->joinRTEs, right->joinRTEs);
	left->selInfos = list_union(left->selInfos, right->selInfos);

	//CHECK need to adapt top index!
}

static void
computeBaseRelMaps (QueryPushdownInfo *info)
{
	QueryPushdownInfo *child;
	ListCell *lc;

	computeSelScopeBaseRelMaps(info->topScope);

	foreach(lc, info->children)
	{
		child = (QueryPushdownInfo *) lfirst(lc);
		computeBaseRelMaps(child);
	}

}

static void
computeSelScopeBaseRelMaps (SelScope *scope)
{
	ListCell *lc;
	SelScope *child;
	Bitmapset *tmp;
	int numBaseRels;
	int member, pos, i;

	/* add children of join nodes to base rel set. If top qual belongs to scope, add
	 * all base rels, otherwise add children of top join nodes that belong to scope
	 */
	if (bms_is_member(RTINDEX_TOPQUAL, scope->childRTEs))
	{
		for (i = 1; i <= list_length(scope->pushdown->query->rtable); i++)
		{
			/* if range table entry is a base relation */
			if (IsA(getJoinTreeNode(scope->pushdown->query, i), RangeTblRef))
				scope->baseRTEs = bms_add_member(scope->baseRTEs, i);
		}
	}
	else
	{
		addBaseRelChildren(getJoinTreeNode(scope->pushdown->query, scope->topIndex), scope);
	}

	/* create map that stores for each base relation the rtindex of the lowest node in the
	 * join tree that belongs to the scope and which is an ancestor of the base relation
	 */
	numBaseRels = bms_num_members(scope->baseRTEs);
	scope->baseRelMap = (int *) palloc(numBaseRels * sizeof(int));

	tmp = bms_copy(scope->baseRTEs);
	pos = 0;
	while ((member = bms_first_member(tmp)) >= 0)
	{
		(scope->baseRelMap)[pos] = getLowestCommonJoinTreeNode(scope->topIndex, scope->childRTEs, member, scope->pushdown->query);
		pos ++;
	}

	/* process all children */
	foreach(lc, scope->children)
	{
		child = (SelScope *) lfirst(lc);
		computeSelScopeBaseRelMaps (child);
	}
}

/*
 *
 */

static void
addBaseRelChildren (Node *joinNode, SelScope *scope)
{
	if (IsA(joinNode, RangeTblRef))
	{
		RangeTblRef *rtRef;

		rtRef = (RangeTblRef *) joinNode;
		scope->baseRTEs = bms_add_member(scope->baseRTEs, rtRef->rtindex);
	}
	else
	{
		JoinExpr *join;

		join = (JoinExpr *) joinNode;
		addBaseRelChildren(join->larg, scope);
		addBaseRelChildren(join->rarg, scope);
	}
}

/*
 *
 */

static Index
getLowestCommonJoinTreeNode(Index startIndex, Bitmapset *nodeSet, Index member, Query *query)
{
	Node *startNode;
	List *path;
	ListCell *lc;
	int curIndex;

	path = NIL;

	/* if top node of scope is top qual we have to check for the base rel in each from list item */
	if (startIndex == 0)
	{
		/* search each from item for base rel and joins that belong to scope */
		foreach(lc, query->jointree->fromlist)
		{
			startNode = (Node *) lfirst(lc);

			curIndex = getLowestMemberOnPath(startNode, member, nodeSet);
			if (curIndex != -1)
				return curIndex;
		}

		/* haven't found join parent that belongs to scope return qual dummy rt index */
		return RTINDEX_TOPQUAL;
	}
	/* top node of scope is a normal JoinExpr or RangeTblRef */
	else
	{
		/* get the top node for the current Scope */
		startNode = getJoinTreeNode(query, startIndex);

		/* if top node is RangeTblRef we are done */
		if (IsA(startNode, RangeTblRef))
			return member;

		/* get the path in the join tree from top node to the base relation */
		curIndex = getLowestMemberOnPath(startNode, member, nodeSet);

		if (curIndex != -1)
			return curIndex;

		//TODO error haven't found base rel!
	}

	return -1;
}

/*
 * For a given join tree node, search for a path to base relation with range table index "member". If
 * such a path is found, return the lowest node in the path that belongs to "nodeSet".
 */

static Index
getLowestMemberOnPath (Node *join, Index member, Bitmapset *nodeSet)
{
	ListCell *lc;
	List *path;
	Index curIndex;
	Index result;

	result = -1;
	path = NIL;

	/* base relation rt index is included in nodeSet? */
	if (bms_is_member(member, nodeSet))
		return member;

	/* if node is a RangeTblEntry but not included in nodeSet return -1 */
	if (IsA(join, RangeTblRef))
		return result;

	/* get path to base relation and check each node's rtindex for
	 * membership in nodeSet.
	 */
	if(findRTindexInJoin(member, (JoinExpr *) join, &path, NULL))
	{
		foreach(lc, path)
		{
			curIndex = lfirst_int(lc);

			if (bms_is_member(curIndex, nodeSet))
				result = curIndex;
		}

		return curIndex;
	}

	return result;
}

/*
 *
 */

static void
generateValidityScopes (QueryPushdownInfo *info)
{
	Bitmapset *curSet;
	Node *node;
	int i;
	ListCell *lc;
	QueryPushdownInfo *child;

	for(i = 1; i <= list_length(info->query->rtable); i++)
	{
		node = getJoinTreeNode(info->query, i);
		curSet = NULL;

		generateIndexSetForNode(node, &curSet);

		info->validityScopes = lappend(info->validityScopes, curSet);
	}

	foreach(lc, info->children)
	{
		child = (QueryPushdownInfo *) lfirst(lc);

		generateValidityScopes(child);
	}
}

/*
 *
 */

static void
generateIndexSetForNode (Node *node, Bitmapset **set)
{
	if (IsA(node, RangeTblRef))
	{
		RangeTblRef *rtRef;

		rtRef = (RangeTblRef *) node;
		*set = bms_add_member(*set, rtRef->rtindex);
	}
	else if (IsA(node, JoinExpr))
	{
		JoinExpr *join;

		join = (JoinExpr *) node;
		*set = bms_add_member(*set, join->rtindex);

		generateIndexSetForNode(join->larg, set);
		generateIndexSetForNode(join->rarg, set);
	}
}
