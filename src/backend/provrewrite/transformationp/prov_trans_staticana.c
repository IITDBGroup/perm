/*-------------------------------------------------------------------------
 *
 * prov_trans_staticana.c
 *	  PERM C - Static analysis of a query tree for transformation provenance
 *	  			rewrite. This used to omit unnecessary runtime computation for
 *	  			parts of a query that have a static transformation provenance.
 *	  			Static information is gathered by traversing the query tree and
 *	  			annotating it (provInfo field) with the necessary info.
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/transformationp/prov_trans_staticana.c,v 1.542 25.08.2009 17:45:57 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "utils/varbit.h"
#include "parser/parsetree.h"

#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_algmap.h"
#include "provrewrite/prov_trans_staticana.h"
#include "provrewrite/prov_trans_util.h"
#include "provrewrite/prov_trans_bitset.h"

/* macros */
#define ADD_NODE_TO_TRANS(info, curnode, newnode) \
	do { \
		if (!((TransProvInfo *) info)->root) \
		{ \
			curnode = ((TransSubInfo *) newnode); \
			((TransProvInfo *) info)->root = ((Node *) curnode); \
		} \
		else \
		{ \
			((TransSubInfo *) curnode)->children = \
					list_make1(((TransSubInfo *) newnode)); \
			curnode = ((TransSubInfo *) newnode); \
		} \
	} while(0)

/* methods */
static int analyseOneQueryNode (Query *query, int curId, TransSubInfo *parent,
		Index rtIndex);
static void handleDummyQueryNode (Query *query);
static int analyseAgg (Query *query, int curId);
static int analyseJoinTree (Query *query, int curId, TransSubInfo *cur);
static int analyseJoinTreeNode (Node *join, int curId, TransSubInfo *cur,
		Query *query);
static int analyseRte (RangeTblEntry *rte, TransSubInfo *cur, int curId,
		TransProvInfo *info, Index rtIndex);
static int analyseProjection (Query *query, int curId, TransSubInfo *cur);
static int analyseSelection (Query *query, int curId, TransSubInfo *cur);
static int analyseSetOp (Query *query, int curId);
static int analyseOneSetOp (Node *node, int curId, TransSubInfo *cur,
		Query *query);

//static void computeAnnots (Query *query);

static bool checkStatic (Query *query);
static bool checkStaticTreeNode (Node *node);

static void computeSets(TransSubInfo *node, int numNodes);

/*
 * Generate ids for parts of the query and analyze which parts are static. This
 * information is stored in as a TransProvInfo in the provInfo->rewriteInfo
 * field of each query node.
 */

void
analyseStaticTransProv (Query *query)
{
	int numNodes;

	numNodes = analyseOneQueryNode (query, 1, NULL, TRANS_NO_RTINDEX);

	checkStatic(query);

	computeSets(getRootSubForNode(GET_TRANS_INFO(query)), numNodes - 1);
}

///*
// *
// */
//
//static void
//computeAnnots (Query *query)
//{
//	TransSubInfo *sub;
//	ListCell *lc;
//	RangeTblEntry *rte;
//
//	sub = getRootSubForNode(GET_TRANS_INFO(query));
//	sub->annot = list_concat(sub->annot, (List *) Provinfo(query)->annotations);
//
//	foreach(lc, query->rtable)
//	{
//		rte = (RangeTblEntry *) lfirst(lc);
//
//		if (rte->rtekind == RTE_SUBQUERY)
//			computeAnnots(rte->subquery);
//	}
//}


/*
 *
 */

static int
analyseOneQueryNode (Query *query, int curId, TransSubInfo *parent,
		Index rtIndex)
{
	TransProvInfo *info;

	DO_SET_TRANS_INFO(query);
	info = GET_TRANS_INFO(query);
	info->rtIndex = rtIndex;

	if(query->setOperations)
		curId = analyseSetOp(query, curId);
	else
		curId = analyseAgg (query, curId);

	if (parent)
		parent->children = lappend(parent->children, info);

	/* In case the query node is a SELECT * FROM subquery dummy query node
	 * we have to add the subquery TransProvInfo as dummy root.
	 */
	if (!info->root)
		handleDummyQueryNode (query);

	return curId;
}

/*
 *
 */
static void
handleDummyQueryNode (Query *query)
{
	RangeTblEntry *rte;
	TransProvInfo *sub;
	TransProvInfo *info;

	info = GET_TRANS_INFO(query);
	rte = rt_fetch(1, query->rtable);
	sub = GET_TRANS_INFO(rte->subquery); //TODO handle other cases: SELECT without FROM, only VALUES etc.

	info->root = (Node *) sub;
}


/*
 *
 */

static int
analyseAgg (Query *query, int curId)
{
	TransSubInfo *next;
	TransSubInfo *cur;
	TransProvInfo *info;

	info = GET_TRANS_INFO(query);
	cur = NULL;

	if (query->hasAggs)
	{
		/* check for projection above aggregation */
		if (isProjectionOverAgg(query))
		{
			cur = makeTransSubInfo (curId++, SUBOP_Projection);
			info->root = (Node *) cur;
		}

		/* check for having clause */
		if (query->havingQual)
		{
			next = makeTransSubInfo(curId++, SUBOP_Having);
			ADD_NODE_TO_TRANS(info,cur,next);
		}

		/* add agg node */
		next = makeTransSubInfo(curId++, SUBOP_Aggregation);
		ADD_NODE_TO_TRANS(info,cur,next);
	}

	curId = analyseProjection(query, curId, cur);

	return curId;
}

/*
 *
 */

static int
analyseProjection (Query *query, int curId, TransSubInfo *cur)
{
	TransSubInfo *next;

	if ((!query->hasAggs && isProjection(query))
			|| isProjectionUnderAgg(query))
	{
		next = makeTransSubInfo(curId++, SUBOP_Projection);
		ADD_NODE_TO_TRANS(GET_TRANS_INFO(query),cur,next);
	}

	curId = analyseSelection(query, curId, cur);

	return curId;
}

/*
 *
 */

static int
analyseSelection (Query *query, int curId, TransSubInfo *cur)
{
	TransSubInfo *next;

	if(query->jointree->quals && !IsA(query->jointree->quals, Const))
	{
		next = makeTransSubInfo(curId++, SUBOP_Selection);
		ADD_NODE_TO_TRANS(GET_TRANS_INFO(query), cur, next);
	}

	curId = analyseJoinTree(query, curId, cur);

	return curId;
}

/*
 *
 */

static int
analyseJoinTree (Query *query, int curId, TransSubInfo *cur)
{
	ListCell *lc;
	Node *node;
	TransProvInfo *info;

	/* check if there is a root TransSubInfo and if fromlist contains more than
	 * one entry */
	info = GET_TRANS_INFO(query);
	if (!info->root && list_length(query->jointree->fromlist) > 1)
	{
		cur = makeTransSubInfo(curId++, SUBOP_Join_Cross);
		info->root = (Node *) cur;
	}

	/* process each from clause entry */
	foreach(lc, query->jointree->fromlist)
	{
		node = (Node *) lfirst(lc);

		curId = analyseJoinTreeNode (node, curId, cur, query);
	}

	return curId;
}

/*
 *
 */

static int
analyseJoinTreeNode (Node *join, int curId, TransSubInfo *cur, Query *query)
{
	JoinExpr *joinExpr;
	RangeTblRef *rtRef;
	TransSubInfo *next;
	TransProvInfo *info;

	info = GET_TRANS_INFO(query);

	if (IsA(join, JoinExpr)) {
		joinExpr = (JoinExpr *) join;

		switch(joinExpr->jointype)
		{
		case JOIN_INNER:
			next = makeTransSubInfo(curId++, SUBOP_Join_Inner);
			break;
		case JOIN_LEFT:
			next = makeTransSubInfo(curId++, SUBOP_Join_Left);
			break;
		case JOIN_RIGHT:
			next = makeTransSubInfo(curId++, SUBOP_Join_Right);
			break;
		case JOIN_FULL:
			next = makeTransSubInfo(curId++, SUBOP_Join_Full);
			break;
		default:
				//TODO
			break;
		}
		next->rtIndex = joinExpr->rtindex;

		if (cur)
			cur->children = lappend(cur->children, next);
		else
			info->root = (Node *) next;

		curId = analyseJoinTreeNode (joinExpr->larg, curId, next, query);
		curId = analyseJoinTreeNode (joinExpr->rarg, curId, next, query);
	}
	else {
		rtRef = (RangeTblRef *) join;

		curId = analyseRte (rt_fetch(rtRef->rtindex, query->rtable), cur,
				curId, info, rtRef->rtindex);
	}

	return curId;
}

/*
 *
 */

static int
analyseRte (RangeTblEntry *rte, TransSubInfo *cur, int curId,
		TransProvInfo *info, Index rtIndex)
{
	TransSubInfo *next;

	switch(rte->rtekind)
	{
	case RTE_RELATION:
		next = makeTransSubInfo(curId++, SUBOP_BaseRel);
		if (cur)
			cur->children = lappend(cur->children, next);
		else
			info->root = (Node *) next;

		next->rtIndex = rtIndex;
		break;
	case RTE_SUBQUERY:
		curId = analyseOneQueryNode(rte->subquery, curId, cur, rtIndex);
		break;
	default:
			//TODO
		break;
	}

	return curId;
}

/*
 *
 */

static int
analyseSetOp (Query *query, int curId)
{
	curId = analyseOneSetOp (query->setOperations, curId, NULL, query);

	return curId;
}

/*
 *
 */

static int
analyseOneSetOp (Node *node, int curId, TransSubInfo *cur, Query *query)
{
	SetOperationStmt *setOp;
	RangeTblRef *rtRef;
	TransSubInfo *next;
	TransProvInfo *info;

	info = GET_TRANS_INFO(query);

	if(IsA(node, SetOperationStmt))
	{
		setOp = (SetOperationStmt *) node;

		switch(setOp->op)
		{
		case SETOP_UNION:
			next = makeTransSubInfo(curId++, SUBOP_SetOp_Union);
			break;
		case SETOP_INTERSECT:
			next = makeTransSubInfo(curId++, SUBOP_SetOp_Intersect);
			break;
		case SETOP_EXCEPT:
			next = makeTransSubInfo(curId++, SUBOP_SetOp_Diff);
			break;
		default:
			//TODO
			break;
		}

		if(!info->root)
			info->root = (Node *) next;
		else
			cur->children = lappend(cur->children, next);

		curId = analyseOneSetOp (setOp->larg, curId, next, query);
		curId = analyseOneSetOp (setOp->rarg, curId, next, query);
	}
	else
	{
		rtRef = (RangeTblRef *) node;

		curId = analyseRte(rt_fetch(rtRef->rtindex, query->rtable), cur, curId,
				info, rtRef->rtindex);
	}

	return curId;
}

/*
 *
 */

static bool
checkStatic (Query *query)
{
	TransProvInfo *info;
	RangeTblEntry *rte;
	ListCell *lc;

	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		if (rte->rtekind == RTE_SUBQUERY)
			checkStatic (rte->subquery);
	}

	info = GET_TRANS_INFO(query);
	info->isStatic = checkStaticTreeNode((Node *) info->root);

	return info->isStatic;
}

/*
 *
 */

static bool
checkStaticTreeNode (Node *node)
{
	Node *sub;
	TransSubInfo *info;
	ListCell *lc;

	/* we have descended into a new query node. Do not descend any further
	 * but just use the precomputed static value.
	 */
	if (IsA(node,TransProvInfo))
		return ((TransProvInfo *) node)->isStatic;

	info = (TransSubInfo *) node;

	switch(info->opType)
	{
	case SUBOP_Selection:
	case SUBOP_Projection:
	case SUBOP_Join_Inner:
	case SUBOP_Aggregation:
	case SUBOP_Having:
	case SUBOP_BaseRel:
	case SUBOP_SetOp_Intersect:
	case SUBOP_Join_Cross:
		info->isStatic = true;
		break;
	case SUBOP_SetOp_Diff:
		sub = (Node *) linitial(info->children);
		info->isStatic = checkStaticTreeNode(sub);
		sub = (Node *) lsecond(info->children);
		checkStaticTreeNode(sub);
		return info->isStatic;
	default:
		info->isStatic = false;
		break;
	}

	foreach(lc, info->children)
	{
		sub = (Node *) lfirst(lc);
		info->isStatic = checkStaticTreeNode(sub) && info->isStatic;
	}

	return info->isStatic;
}

/*
 * Compute the annotation and bitset for a TransSubInfo. If the node has not
 * static transformation provenance, then the set contains only the identifier
 * for this node. For nodes with static transformation provenance the set
 * also includes the sets for its children (or its left child in case of
 * set difference).
 */

static void
computeSets (TransSubInfo *node, int numNodes)
{
	ListCell *lc;
	TransSubInfo *childSub;
	Node *child;

	node->setForNode = generateVarbitSetElem(numNodes, node->id);

	foreach(lc, node->children)
	{
		child = (Node *) lfirst(lc);

		if (IsA(child, TransProvInfo))
			childSub = getRootSubForNode((TransProvInfo *) child);
		else
			childSub = (TransSubInfo *) child;

		computeSets (childSub, numNodes);

		if (node->isStatic)
		{
			// for set difference only add the set of the first child.
			if (node->opType != SUBOP_SetOp_Diff
					|| lc == list_head(node->children))
			{
				node->setForNode = varBitOr(node->setForNode,
						childSub->setForNode);
				node->annot = list_concat(node->annot, childSub->annot);
			}
		}
	}
}




