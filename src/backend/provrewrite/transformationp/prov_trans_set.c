/*-------------------------------------------------------------------------
 *
 * prov_trans_set.c
 *	  POSTGRES C - Transformation provenance rewrites for set operations.
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/transformationp/prov_trans_set.c,v 1.542 02.09.2009 17:24:30 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "utils/guc.h"
#include "utils/datum.h"
#include "nodes/parsenodes.h"
#include "parser/parsetree.h"

#include "provrewrite/prov_aggr.h"
#include "provrewrite/provlog.h"
#include "provrewrite/prov_trans_util.h"
#include "provrewrite/prov_trans_main.h"
#include "provrewrite/prov_set.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_trans_set.h"
#include "provrewrite/prov_trans_bitset.h"
#include "provrewrite/prov_sublink_util_mutate.h"



/* macros */
#define ASET_LARG(sub) \
	((Node **) &(((TransSubInfo *) sub)->children->head->data.ptr_value))

#define ASET_RARG(sub) \
	((Node **) &(((TransSubInfo *) sub)->children->head->next->data.ptr_value))


/* functions */
static void reduceRtindexForTrans(TransSubInfo *info);
static void rewriteUnionWithWlCS (Query *query, RangeTblEntry *queryRte);
static void addTransProvToSetOp (SetOperationStmt *setOp, Index attrNum);
static void addBitsetsToChildren (Query *query, TransSubInfo *subInfo,
		Datum bitset);
static void restructureSetOperationQueryNode (Query *query, Node *node,
		TransSubInfo *nodeInfo, Node **infoPointer, Node **parentPointer,
		SetOperation rootType);
static void replaceSetOperatorSubtree (Query *query, SetOperationStmt *setOp,
		Node **parent, TransSubInfo *nodeInfo, Node **infoPointer);
static void findSetOpInfo (TransSubInfo *node, List **result);
static void rewriteUsingSubInfo (TransSubInfo *setInfo, Query *query,
		Index offset);
static void createJoins (Query *top, Query *query);
static void createSetTransProvAttr (Query *newTop);
static Node *buildComputationForSetOps (Query *query, Node *setInfo);

/*
 *
 */

Query *
rewriteTransSet (Query *query, Node **parent, RangeTblEntry *queryRte)
{
	Query *newTop;
	Query *orig;
	TransProvInfo *info;
	TransProvInfo *topInfo;
	int beforeRteNum;

	orig = copyObject(query);
	info = GET_TRANS_INFO(query);

	/* remove dummy RTEs produced by postgres view expansion */
	beforeRteNum = list_length(query->rtable);
	removeDummyRewriterRTEs(query);
	if (list_length(query->rtable) != beforeRteNum)
		reduceRtindexForTrans((TransSubInfo *) info->root);

	/* if necessary spread the set operation tree over several query nodes */
	restructureSetOperationQueryNode(query,
			query->setOperations,
			(TransSubInfo *) info->root,
			(Node **) &info->root,
			&query->setOperations,
			((SetOperationStmt *) query->setOperations)->op);

	/* check if the alternative union semantics is activate
	 * and we are rewriting a union node. If so use all the join
	 * stuff falls apart and we just have to rewrite the original query
	 */
	if (prov_use_wl_union_semantics &&
			((SetOperationStmt *) query->setOperations)->op == SETOP_UNION)
	{
		rewriteUnionWithWlCS (query, queryRte);

		return query;
	}

	/* create new top query node */
	newTop = makeQuery();
	topInfo = SET_TRANS_INFO(newTop);
	topInfo->root = (Node *) info;
	topInfo->rtIndex = info->rtIndex;
	info->rtIndex = 1;

	/* create rtable and attrs of new top query node */

	/* if set operation is a set difference only add the first range table
	 * entry. */
	if (((SetOperationStmt *) query->setOperations)->op == SETOP_EXCEPT)
		newTop->rtable = list_make1(copyObject(linitial(query->rtable)));
	else
		newTop->rtable = copyObject(query->rtable);
	newTop->targetList = copyObject(query->targetList);
	newTop->intoClause = copyObject(query->intoClause);
	SetSublinkRewritten(newTop, true);

	/* add original query as first range table entry */
	addSubqueryToRTWithParam (newTop, orig, "originalSet", false,
			ACL_NO_RIGHTS, false);

	/* rewrite RTEs of query */
	rewriteUsingSubInfo((TransSubInfo *) info->root, newTop, 1);

	/* correct alias of rewritten subqueries */
	correctSubQueryAlias (newTop);

	/* create joins between original set and rewritten children */
	createJoins (newTop, query);

	/* add transprov attr */
	createSetTransProvAttr (newTop);
	*parent = (Node *) topInfo;

	return newTop;
}

/*
 *
 */
static void
reduceRtindexForTrans(TransSubInfo *info)
{
	ListCell *lc;
	Node *child;
	TransSubInfo *subChild;
	TransProvInfo *infoChild;

	if (info->rtIndex > 1)
		info->rtIndex -= 2;

	foreach(lc, info->children)
	{
		child = (Node *) lfirst(lc);

		if (IsA(child, TransProvInfo))
		{
			infoChild = (TransProvInfo *) child;
			if (infoChild->rtIndex > 1)
				infoChild->rtIndex -= 2;
		}
		else
		{
			subChild = (TransSubInfo *) child;
			reduceRtindexForTrans(subChild);
		}
	}
}

/*
 *
 */

Query *
rewriteStaticSetOp (Query *query, Node **parentInfo)
{
	Query *newTop;
	TransProvInfo *subInfo;
	TransProvInfo *info;

	newTop = makeQuery();
	addSubqueryToRT(newTop, query, appendIdToString("originalSet",
			&curUniqueRelNum));
	addSubqueryTargetListToTargetList(query, 1);

	DO_SET_TRANS_INFO(newTop);
	info = GET_TRANS_INFO(newTop);
	subInfo = GET_TRANS_INFO(query);
	info->isStatic = subInfo->isStatic;
	info->root = (Node *) subInfo;
	info->rtIndex = subInfo->rtIndex;
	subInfo->rtIndex = 1;

	if (parentInfo)
		*parentInfo = (Node *) info;

	addStaticTransProvAttr(newTop);

	return newTop;
}


/*
 *
 */

static void
rewriteUnionWithWlCS (Query *query, RangeTblEntry *queryRte)
{
	TransSubInfo *root;
	TargetEntry *newTarget;
	Var *var;

	root = (TransSubInfo *) GET_TRANS_INFO(query)->root;

	/* rewrite the children */
	rewriteUsingSubInfo (root, query, 0);

	/* add the the union nodes bitset's to the rewritten rte's */
	addBitsetsToChildren(query, root, 0);

	/* add the trans prov attr to the set operations */
	addTransProvToSetOp ((SetOperationStmt *) query->setOperations,
			list_length(query->targetList) + 1);

	/* correct alias of rewritten subqueries */
	correctSubQueryAlias (query);

	/* adapt set op query target list:
	 * Add the trans_prov attribute
	 */
	var = makeVar(1, list_length(query->targetList) + 1, VARBITOID , -1, 0);
	newTarget = MAKE_TRANS_PROV_ATTR(query, var);
	query->targetList = lappend(query->targetList, newTarget);
	GET_TRANS_INFO(query)->transProvAttrNum = list_length(query->targetList);

	queryRte->eref->colnames = lappend(queryRte->eref->colnames,
			makeString(newTarget->resname));
}

/*
 *
 */

static void
addTransProvToSetOp (SetOperationStmt *setOp, Index attrNum)
{
	setOp->colTypes = lappend_oid(setOp->colTypes, VARBITOID);
	setOp->colTypmods = lappend_int(setOp->colTypmods, -1);

	if (IsA(setOp->larg, SetOperationStmt))
		addTransProvToSetOp ((SetOperationStmt *) setOp->larg, attrNum);
	if (IsA(setOp->rarg, SetOperationStmt))
		addTransProvToSetOp ((SetOperationStmt *) setOp->rarg, attrNum);
}

/*
 *
 */

static void
addBitsetsToChildren (Query *query, TransSubInfo *subInfo, Datum bitset)
{
	Datum newBitSet;
	ListCell *lc;
	Node *child;
	RangeTblEntry *rte;
	TransProvInfo *childInfo;
	TargetEntry *te;

	newBitSet = datumCopy(subInfo->setForNode, false, -1);

	if (bitset)
		newBitSet = varBitOr(newBitSet, bitset);

	foreach(lc, subInfo->children)
	{
		child = (Node *) lfirst(lc);

		if (IsA(child, TransSubInfo))
			addBitsetsToChildren(query, (TransSubInfo *) child, newBitSet);
		else
		{
			childInfo = (TransProvInfo *) child;
			rte = rt_fetch(childInfo->rtIndex, query->rtable);

			te = (TargetEntry *) list_nth(rte->subquery->targetList,
					childInfo->transProvAttrNum - 1);
			te->expr = (Expr *) MAKE_SETOR_FUNC(list_make2(
					te->expr,
					MAKE_VARBIT_CONST(newBitSet)));
		}
	}

	pfree(DatumGetPointer(newBitSet));
}

/*
 * Traverse the TransSubInfo tree of a set operation query node and rewrite
 * each leaf child node's RTE.
 */

static void
rewriteUsingSubInfo (TransSubInfo *setInfo, Query *query, Index offset)
{
	ListCell *lc;
	Node *child;
	RangeTblEntry *rte;
	Index rtIndex;

//
//	if (setInfo->opType == SUBOP_SetOp_Diff)
//	{
//		child = (Node *) linitial(setInfo->children);
//
//		if (IsA(child, TransProvInfo))
//		{
//			rtIndex = TRANS_GET_RTINDEX(child) + offset;
//			TRANS_SET_RTINDEX(child, rtIndex);
//			rte = rt_fetch(rtIndex, query->rtable);
//			SET_TRANS_INFO_TO(rte->subquery, child);
//			rte->subquery = rewriteQueryNodeTrans (rte->subquery, rte,
//					(Node **) &(setInfo->children->head->data.ptr_value));
//		}
//		else
//			rewriteUsingSubInfo((TransSubInfo *) child, query, offset);
//
//		return;
//	}

	foreach(lc, setInfo->children)
	{
		child = (Node *) lfirst(lc);

		// For set difference only rewrite the left input.
		if (setInfo->opType == SUBOP_SetOp_Diff
				&& prov_use_wl_union_semantics
				&& lc != list_head(setInfo->children))
			break;

		// rewrite child
		if (IsA(child, TransProvInfo))
		{
			rtIndex = TRANS_GET_RTINDEX(child) + offset;
			TRANS_SET_RTINDEX(child, rtIndex);
			rte = rt_fetch(rtIndex, query->rtable);
			SET_TRANS_INFO_TO(rte->subquery, child);
			rte->subquery = rewriteQueryNodeTrans (rte->subquery, rte,
					(Node **) &(lc->data.ptr_value));
		}
		else
			rewriteUsingSubInfo((TransSubInfo *) child, query, offset);
	}
}

/*
 *
 */
static void
createJoins (Query *top, Query *query)
{
	JoinExpr *newJoin;
	JoinType joinType;
	RangeTblRef *rtRef;
	int origRtableLength, i;

	/* define join type to use in join expression */
	switch (((SetOperationStmt *) query->setOperations)->op)
	{
		case SETOP_UNION:
			joinType = JOIN_LEFT;
		break;
		case SETOP_INTERSECT:
		case SETOP_EXCEPT:
			joinType = JOIN_INNER;
		break;
		default:
			//TODO error
		break;
	}

	/* create joins */
	origRtableLength = list_length(top->rtable);

	MAKE_RTREF(rtRef, 1);
	top->jointree->fromlist = list_make1(rtRef);

	for(i = 2; i <= origRtableLength; i++) {
		newJoin = createJoinExpr(top, joinType);

		createSetJoinCondition (top, newJoin, 0, i - 1, false);
		newJoin->larg =  linitial(top->jointree->fromlist);
		MAKE_RTREF(rtRef, i);
		newJoin->rarg = (Node *) rtRef;

		top->jointree->fromlist = list_make1(newJoin);
	}

	/* create correct join RTEs for the new joins */
	recreateJoinRTEs(top);
}


/*
 * Walks through a set operation tree of a query. If only one type of operators
 * (union or intersection) is found nothing is done. If a different operator or
 * a set difference operator is found, the whole subtree under this operator is
 * extracted into a new query node.
 */

static void
restructureSetOperationQueryNode
		(Query *query, Node *node, TransSubInfo *nodeInfo, Node **infoPointer,
				Node **parentPointer, SetOperation rootType)
{
	SetOperationStmt *setOp;

	/* RangeTblRef are ignored */
	if (!IsA(node,SetOperationStmt))
		return;

	/* cast to set operation stmt */
	setOp = (SetOperationStmt *) node;

	/* if the user deactivated the optimized set operation rewrites we
	 * outsource each set operation into a separate query. */
	if (!prov_use_set_optimization)
	{
		if (IsA(setOp->larg, SetOperationStmt))
			replaceSetOperatorSubtree (query, (SetOperationStmt *) setOp->larg,
					&(setOp->larg), TSET_LARG(nodeInfo), ASET_LARG(nodeInfo));

		if (IsA(setOp->rarg, SetOperationStmt))
			replaceSetOperatorSubtree (query, (SetOperationStmt *) setOp->rarg,
					&(setOp->rarg), TSET_RARG(nodeInfo), ASET_RARG(nodeInfo));
	}

	/*
	 *  Optimization is activated. Keep original set operation tree, if it
	 *  just contains only union or only intersection operations. For a set
	 *  difference operation both operands are outsourced into a separate query
	 *  (If they are not simple range table references). For a mixed unions
	 *  and intersections we have to outsource sub trees under set operations
	 *  different from the root set operation.
	 */

	switch (setOp->op)
	{
		/* union or intersect, do not change anything */
		case SETOP_UNION:
		case SETOP_INTERSECT:
			/* check if of the same type as parent */
			if (setOp->op == rootType)
			{
				restructureSetOperationQueryNode (query, setOp->larg,
						TSET_LARG(nodeInfo), ASET_LARG(nodeInfo),
						&(setOp->larg), rootType);
				restructureSetOperationQueryNode (query, setOp->rarg,
						TSET_RARG(nodeInfo), ASET_RARG(nodeInfo),
						&(setOp->rarg), rootType);
			}
			/* another type replace subtree */
			else
				replaceSetOperatorSubtree(query, setOp, parentPointer,
						nodeInfo, infoPointer);
		break;
		/* set difference, replace subtree with new query node */
		case SETOP_EXCEPT:
			/* if is root set diff operation replace left and right sub trees */
			if (rootType == SETOP_EXCEPT) {
				if (IsA(setOp->larg, SetOperationStmt))
					replaceSetOperatorSubtree (query, (SetOperationStmt *)
							setOp->larg, &(setOp->larg), TSET_LARG(nodeInfo),
							ASET_LARG(nodeInfo));

				if (IsA(setOp->rarg, SetOperationStmt))
					replaceSetOperatorSubtree (query, (SetOperationStmt *)
							setOp->rarg, &(setOp->rarg), TSET_RARG(nodeInfo),
							ASET_RARG(nodeInfo));
			}
			/* is not root operation process as for operator change */
			else {
				replaceSetOperatorSubtree(query, setOp, parentPointer,
						nodeInfo, infoPointer);
			}
		break;
		default:
			//TODO error
		break;
	}
}

/*
 * Replaces a subtree in an set operation tree with a new subquery that
 * represents the set operations performed by the sub tree.
 */

static void
replaceSetOperatorSubtree (Query *query, SetOperationStmt *setOp,
		Node **parent, TransSubInfo *nodeInfo, Node **infoPointer)
{
	ListCell *lc;
	ListCell *rLc;
	List *subTreeRTEs;
	List *subTreeRTindex;
	List *subTreeRTrefs;
	List *queryRTrefs;
	List *queryInfos;
	List *newRtable;
	List *subTreeInfos;
	Query *newSub;
	RangeTblEntry *rte;
	RangeTblRef *rtRef;
	int counter;
	int *context;
	TransProvInfo *subInfo;
	Node *setOpInfo;

	subTreeRTEs = NIL;
	subTreeRTindex = NIL;
	subTreeInfos = NIL;
	queryInfos = NIL;

	/* find all range table entries referenced from the subtree under setOp */
	findSetOpRTEs(query->rtable,(Node *) setOp, &subTreeRTEs, &subTreeRTindex);
	findSetOpInfo(nodeInfo, &subTreeInfos);

	/* create new query node for subquery */
	newSub = (Query *) copyObject(query);
	subInfo = GET_TRANS_INFO(newSub);
	subInfo->root = (Node *) nodeInfo;
	*infoPointer = (Node *) subInfo;
	pfree(newSub->rtable);
	newSub->rtable = NIL;
	newSub->setOperations = (Node *) copyObject(setOp); //CHECK necessary

	/* create range table entries for range table entries referenced from set
	 * operation in subtree */
	foreach(lc,subTreeRTEs)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		newSub->rtable = lappend(newSub->rtable,
				(RangeTblEntry *) copyObject(rte));
	}

	/* adapt RTErefs in sub tree */
	subTreeRTrefs = getSetOpRTRefs((Node *) newSub->setOperations);

	forbothi(lc, rLc, counter, subTreeRTrefs, subTreeInfos)
	{
		rtRef = (RangeTblRef *) lfirst(lc);
		rtRef->rtindex = counter + 1;

		setOpInfo = (Node *) lfirst(rLc);
		if (IsA(setOpInfo, TransSubInfo))
			((TransProvInfo *) setOpInfo)->rtIndex = counter + 1;
		else
			((TransSubInfo *) setOpInfo)->rtIndex = counter + 1;

	}

	/* add new sub query to range table */
	addSubqueryToRTWithParam (query, newSub, "newSub", false, ACL_NO_RIGHTS,
			true);

	/* replace subtree with RTE reference */
	MAKE_RTREF(rtRef, list_length(query->rtable));
	*parent = (Node *) rtRef;

	LOGNODE(query, "before range table adapt");

	/* adapt range table and rteRefs for query */
	newRtable = NIL;
	queryRTrefs = getSetOpRTRefs(query->setOperations);
	findSetOpInfo((TransSubInfo *) GET_TRANS_INFO(query)->root, &queryInfos);

	forboth(lc, queryRTrefs, rLc, queryInfos)
	{
		rtRef = lfirst(lc);
		rte = rt_fetch(rtRef->rtindex, query->rtable);
		newRtable = lappend(newRtable, rte);
		rtRef->rtindex = list_length(newRtable);

		setOpInfo = (Node *) lfirst(rLc);
		TRANS_SET_RTINDEX(setOpInfo, rtRef->rtindex);
	}

	query->rtable = newRtable;

	/* increase sublevelsup of newSub if we are rewritting a sublink query */
	context = (int *) palloc(sizeof(int));
	*context = -1;
	increaseSublevelsUpMutator ((Node *) newSub, context);
	pfree(context);

	logNode(query, "after replace of subtree");
}


/*
 *
 */

static void
findSetOpInfo (TransSubInfo *node, List **result)
{
	ListCell *lc;
	Node *child;
	TransSubInfo *childInfo;

	foreach(lc, node->children)
	{
		child = (Node *) lfirst(lc);

		if (IsA(child, TransSubInfo))
		{
			childInfo = (TransSubInfo *) child;

			findSetOpInfo (childInfo, result);
		}
		else
			*result = lappend(*result, child);
	}
}

/*
 *
 */

static void
createSetTransProvAttr (Query *newTop)
{
	TransProvInfo *info;
	TransProvInfo *topInfo;
	TargetEntry *te;

	topInfo = GET_TRANS_INFO(newTop);
	info = (TransProvInfo *) topInfo->root;

	te = MAKE_TRANS_PROV_ATTR(newTop,
			buildComputationForSetOps (newTop, info->root));
	newTop->targetList = lappend(newTop->targetList, te);
	topInfo->transProvAttrNum = list_length(newTop->targetList);
}

/*
 *
 */

static Node *
buildComputationForSetOps (Query *query, Node *setInfo)
{
	Node *comp;
	TransSubInfo *subInfo;

	if (IsA(setInfo, TransProvInfo))
		return getRealSubqueryTransProvAttr(query, TRANS_GET_RTINDEX(setInfo));

	subInfo = (TransSubInfo *) setInfo;

	comp = (Node *) MAKE_SETOR_FUNC(list_make2(
			buildComputationForSetOps(query, (Node *) TSET_LARG(subInfo)),
			buildComputationForSetOps(query, (Node *) TSET_RARG(subInfo))
			));

	switch(((TransSubInfo *) setInfo)->opType)
	{
	case SUBOP_SetOp_Union:
		return (Node *) MAKE_SETOR_FUNC_NO_NULL(list_make2(
							comp,
							MAKE_VARBIT_CONST(subInfo->setForNode)
							));
	case SUBOP_SetOp_Intersect:
	case SUBOP_SetOp_Diff: //TODO
		return (Node *) MAKE_SETOR_FUNC(list_make2(
				comp,
				MAKE_VARBIT_CONST(subInfo->setForNode)
				));
		break;
	default:
		break;
	}

	return NULL;
}
