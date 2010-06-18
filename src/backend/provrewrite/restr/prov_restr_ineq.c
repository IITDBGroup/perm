/*-------------------------------------------------------------------------
 *
 * prov_restr_ineq.c
 *	  POSTGRES C
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/prov_restr_ineq.c,v 1.542 08.01.2009 17:44:19 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "nodes/makefuncs.h"
#include "optimizer/clauses.h"

#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_restr_ineq.h"
#include "provrewrite/prov_restr_util.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/provstack.h"

/* marcos */
#define IsAConstNode(node) \
	(list_length(((InequalityGraphNode *) node)->consts))

/* prototypes */
static InequalityGraphNode *getNodeForExpr (InequalityGraph *graph, Node *expr, PushdownInfo *info);
static bool graphNodeContainsEL (Node *node, EquivalenceList *context);
static bool computeReachableNodes (InequalityGraph *graph, InequalityGraphNode *currentNode,
	 InequalityGraphNode *start, bool smaeq, List *path, List **cycles);
static void mergeCycles (InequalityGraph *graph, List *mergeLists);
static InequalityGraphNode *mergeCycle (InequalityGraph *graph, List *path);
static void removeParalellEdges (InequalityGraph *graph);
static void removeRedundentEdgesToNodes (InequalityGraphNode *node);
static void removeEdgesToConsts (InequalityGraphNode *node, ComparisonType edgeType, InequalityGraphNode **maxMinNode, List **remove);
static void mergeGraphNodeEquivalenceLists (InequalityGraph *graph, PushdownInfo *info);
static bool checkNodeConsts (InequalityGraphNode *node);
static bool examineSelfEdges (InequalityGraph *graph);
static SelectionInfo *generateInequality (InequalityGraphNode *from, InequalityGraphNode *to, ComparisonType type);
static void removeEdgesFromList (InequalityGraphNode *node, List *edges, bool bigger);
static bool computeConstantsForGraph (InequalityGraph *graph, PushdownInfo *info);
static bool checkInequality (InequalityGraphNode *left, InequalityGraphNode *right, ComparisonType type);

//TODO provide flat free for graph

/*
 * Compute the inequality graph for a pushdown info and a list of inequalities
 */

InequalityGraph *
computeInequalityGraph (PushdownInfo *info)
{
	List *ineqs;
	InequalityGraph *graph;
	InequalityGraphNode *node;
	EquivalenceList *equi;
	ListCell *lc;
	Node *from;
	Node *to;
	InequalityGraphNode *fromNode;
	InequalityGraphNode *toNode;
	SelectionInfo *sel;

	ineqs = getSimpleInequalities (info);

	graph = makeInequalityGraph ();

	/* generate a node in the graph for each equivalence list from info */
	foreach(lc,info->equiLists)
	{
		equi = (EquivalenceList *) lfirst(lc);

		node = makeInequalityGraphNode ((Node *) equi);
		graph->nodes = lappend(graph->nodes, node);
		graph->equiLists = lappend(graph->equiLists, equi);
	}

	/* generate a edge for each inequality */
	foreach(lc, ineqs)
	{
		sel = (SelectionInfo *) lfirst(lc);
		from = getLeftSelOp (sel);
		to = getRightSelOp (sel);
		fromNode = getNodeForExpr (graph, from, info);
		toNode = getNodeForExpr (graph, to, info);

		/* the type of inequality determines the type of edge we have to add */
		switch(getTypeForIneq (sel))
		{
			case COMP_SMALLER:
				fromNode->lessThen = list_append_unique(fromNode->lessThen, toNode);
				toNode->greaterEqThen = list_append_unique(toNode->greaterEqThen, fromNode);
			break;
			case COMP_SMALLEREQ:
				fromNode->lessEqThen = list_append_unique(fromNode->lessEqThen, toNode);
				toNode->greaterThen = list_append_unique(toNode->greaterThen, fromNode);
			break;
			case COMP_BIGGEREQ:
				toNode->lessThen = list_append_unique(toNode->lessThen, fromNode);
				fromNode->greaterEqThen = list_append_unique(fromNode->greaterEqThen, toNode);
			break;
			case COMP_BIGGER:
				toNode->lessEqThen = list_append_unique(toNode->lessEqThen, fromNode);
				fromNode->greaterThen = list_append_unique(fromNode->greaterThen, toNode);
			break;
			default:
				//TODO error
			break;
		}
	}

	return graph;
}

/*
 * Searches for the node that represent the equivalence list of an expression.
 */

static InequalityGraphNode *
getNodeForExpr (InequalityGraph *graph, Node *expr, PushdownInfo *info)
{
	EquivalenceList *equi;

	equi = getEqualityListForExpr (info, expr);

	if (equi == NULL)
		return NULL;

	return (InequalityGraphNode *) getFirstNodeForPred(graph->nodes, graphNodeContainsEL, (void *) equi);
}

/*
 *
 */

static bool
graphNodeContainsEL (Node *node, EquivalenceList *context)
{
	InequalityGraphNode *gNode;
	EquivalenceList *equi;
	ListCell *lc;

	gNode = (InequalityGraphNode *) node;

	foreach(lc, gNode->equis)
	{
		equi = (EquivalenceList *) lfirst(lc);

		if (equal(equi, context))
			return true;
	}

	return false;
}

/*
 *	Computes the transitive closure of the edge relations of an inequality graph. This is done by:
 *		1) Add a new edge between E1 and E2 if there is a path from E1 to E2. The label of the edge is determined by:
 *			1) all edges on the path are labeled as <= --> label is <=
 *			2) else --> label is <
 */

InequalityGraph *
computeTransitiveClosure (InequalityGraph *graph)
{
	ListCell *lc;
	InequalityGraphNode *node;
	List *cycles;


	cycles = NIL;

	/* for each node in the graph find all reachable nodes */
	foreach(lc, graph->nodes)
	{
		node = (InequalityGraphNode *) lfirst(lc);

		if (computeReachableNodes(graph, node, node, true, NIL, &cycles))
			return NULL;
	}

	/* if has mergeable cycles then merge the nodes of each cycle */
	mergeCycles(graph, cycles);

	return graph;
}

/*
 * Recursively computes the nodes reachable from one node.
 */

static bool
computeReachableNodes (InequalityGraph *graph, InequalityGraphNode *currentNode,
		InequalityGraphNode *start, bool smaeq, List *path, List **cycles)
{
	InequalityGraphNode *node;
	ListCell *lc;
	List *newPath;
	bool alreadyThere;

	alreadyThere = false;

	/*
	 * Check for a cycle. If we have a cycle check if it represents an contradiction or results in merge of the
	 * equivalence classes of the cycle.
	 */
	if (list_length(path) != 0 && start == currentNode)
	{
		if (smaeq)
			*cycles = list_append_unique (*cycles, path);
		else
			return true;
	}

	/*
	 * add edge from start node to current node. If there is already an edge do not go any further, because
	 * we are sure that we have done this the first time we reached this node.
	 */
	if (smaeq)
	{
		if (list_member(start->lessEqThen, currentNode))
			alreadyThere = true;

		start->lessEqThen = list_append_unique (start->lessEqThen, currentNode);
		currentNode->greaterThen = list_append_unique (currentNode->greaterThen, start);
	}
	else
	{
		if (list_member(start->lessThen, currentNode))
			alreadyThere = true;

		start->lessThen = list_append_unique (start->lessThen, currentNode);
		currentNode->greaterEqThen = list_append_unique (currentNode->greaterEqThen, start);
	}

	if (alreadyThere)
		return false;

	/* no cycle proceed with all directly connected nodes */
	foreach(lc, currentNode->lessThen)
	{
		node = (InequalityGraphNode *) lfirst(lc);
		newPath = list_copy(path);
		newPath = lappend(newPath, node);

		if (computeReachableNodes (graph, node, start, false, newPath, cycles))
			return true;
	}

	foreach(lc, currentNode->lessEqThen)
	{
		node = (InequalityGraphNode *) lfirst(lc);
		newPath = list_copy(path);
		newPath = lappend(newPath, node);

		if (computeReachableNodes (graph, node, start, smaeq, newPath, cycles))
			return true;
	}

	return false;
}

/*
 * Given a list of lists of nodes to merge. Iterate through this list and for each sublist merge the nodes in this list, apdapt
 * incoming and outgoing edges and adapt the nodes-list of the inequality graph.
 */

static void
mergeCycles (InequalityGraph *graph, List *mergeLists)
{
	List *mergeList;
	List *curMergeList;
	ListCell *lc;
	ListCell *innerLc;
	InequalityGraphNode *node;

	/* for each list of nodes that should be merged do...*/
	foreach(lc, mergeLists)
	{
		mergeList = (List *) lfirst(lc);

		/* do not merge a single node */
		if (list_length(mergeList) <= 1)
			continue;

		/* merge the nodes */
		node = mergeCycle (graph, mergeList);

		/* adapt other merge lists and graph node list */
		graph->nodes = list_difference(graph->nodes, mergeList);
		graph->nodes = lappend (graph->nodes, node);

		foreachsince(innerLc, lc)
		{
			curMergeList = (List *) lfirst(innerLc);

			if (list_overlap(curMergeList, mergeList))
			{
				curMergeList = list_difference(curMergeList, mergeList);
				curMergeList = lappend(curMergeList, node);

				lfirst(innerLc) = curMergeList;
			}
		}
	}

}

/*
 * Merges a list of nodes in an inequality graph. All edges from or to a node in the merge list are adapted to point to the
 * merged node. The equivalence lists list of the new created node contains all the equivalence lists from the merged nodes.
 */

static InequalityGraphNode *
mergeCycle (InequalityGraph *graph, List *path)
{
	InequalityGraphNode *mergedNode;
	List *predecessors;
	ListCell *lc;
	InequalityGraphNode *node;

	mergedNode = makeInequalityGraphNodeNIL ();
	predecessors = NIL;

	/* collect incoming and outgoing edges */
	foreach(lc, path)
	{
		node = (InequalityGraphNode *) lfirst(lc);

		mergedNode->lessThen = list_concat_unique (mergedNode->lessThen, node->lessThen);
		mergedNode->lessEqThen = list_concat_unique (mergedNode->lessThen, node->lessEqThen);
		mergedNode->greaterThen = list_concat_unique (mergedNode->greaterThen, node->greaterThen);
		mergedNode->greaterEqThen = list_concat_unique (mergedNode->greaterEqThen, node->greaterEqThen);

		mergedNode->equis = list_concat_unique(mergedNode->equis, node->equis);
	}

	mergedNode->lessThen = list_difference (mergedNode->lessThen, path);
	mergedNode->lessEqThen = list_difference (mergedNode->lessEqThen, path);
	mergedNode->greaterThen = list_difference (mergedNode->greaterThen, path);
	mergedNode->greaterEqThen = list_difference (mergedNode->greaterEqThen, path);

	/* adapt the edges of predecessors */
	foreach(lc, mergedNode->greaterEqThen)
	{
		node = (InequalityGraphNode *) lfirst(lc);

		node->lessEqThen = list_difference (node->lessEqThen, path);
		node->lessEqThen = list_append_unique (node->lessEqThen, mergedNode);
	}

	foreach(lc, mergedNode->greaterThen)
	{
		node = (InequalityGraphNode *) lfirst(lc);

		node->lessThen = list_difference (node->lessThen, path);
		node->lessThen = list_append_unique (node->lessThen, mergedNode);
	}

	return mergedNode;
}

/*
 * Minimizes an inequality graph by merging nodes and removing redundant edges. Returns NULL if a contradiction is found.
 */

InequalityGraph *
minimizeInEqualityGraph (PushdownInfo *info, InequalityGraph *graph)
{
	InequalityGraphNode *node;
	ListCell *lc;

	/* merge the equivalence lists of each node */
	mergeGraphNodeEquivalenceLists (graph, info);

	/* compute constants lists for each node in the graph */
	if (computeConstantsForGraph(graph, info))
		return NULL;

	/* check for redundent self edges and self edges that are contradictions */
	if (examineSelfEdges(graph))
		return NULL;

	/* remove redundend edges for constants */
	foreach(lc, graph->nodes)
	{
		node = (InequalityGraphNode *) lfirst(lc);

		/* if curEqui contains no const search for different consts that are greater/smaller and just keep the edge
		 * to the biggest/smallest one.
		 */
		if (list_length(node->consts) == 0)
			removeRedundentEdgesToNodes(node);
	}

	/* remove parallel edges */
	removeParalellEdges(graph);

	return graph;
}

/*
 *	Merge all equivalence lists of each node in an inequality graph and adapt the pushdown info list of equivalence lists accordingly.
 */

static void
mergeGraphNodeEquivalenceLists (InequalityGraph *graph, PushdownInfo *info)
{
	ListCell *lc;
	ListCell *innerLc;
	InequalityGraphNode *node;
	EquivalenceList *mergedEqui;
	EquivalenceList *curEqui;

	/* merge the equivalence classes of each graph node */
	foreach(lc, graph->nodes)
	{
		node = (InequalityGraphNode *) lfirst(lc);

		if (list_length(node->equis) > 1)
		{
			mergedEqui = makeEquivalenceList ();

			/* merge equivalence list and remove original lists from PushdownInfo */
			foreach(innerLc, node->equis)
			{
				curEqui = (EquivalenceList *) lfirst(innerLc);

				mergedEqui->exprs = list_union(mergedEqui->exprs, curEqui->exprs);
				removeNodeElem(info->equiLists, (Node *) curEqui);
			}

			/* add merge equivalence list from PushdownInfo */
			node->equis = list_make1(mergedEqui);
			info->equiLists = lappend(info->equiLists, mergedEqui);
		}
	}
}

/*
 * For each node in an inequality graph the list of constants contained in its equalivance lists is computed. Returns true
 * if a contradiction is found.
 */

static bool
computeConstantsForGraph (InequalityGraph *graph, PushdownInfo *info)
{
	ListCell *lc;
	ListCell *innerLc;
	InequalityGraphNode *node;
	InequalityGraphNode *toNode;
	EquivalenceList *equi;
	List *mergeLists;
	Const *cons;

	mergeLists = NIL;

	/* compute the constants lists */
	foreach(lc,graph->nodes)
	{
		node = (InequalityGraphNode *) lfirst(lc);

		/* add the constants for each equivalence list of
		 * the current node to its constants list
		 */
		foreach(innerLc, node->equis)
		{
			equi = (EquivalenceList *) lfirst(innerLc);
			node->consts = list_append_unique(node->consts, getEquivalenceListConsts(equi));

			/* check if set of constants do not
			 * represent a contradiction
			 */
			if (checkNodeConsts(node))
				return true;
		}
	}

	/* find mergeable nodes and merge them */
	foreach(lc, graph->nodes)
	{
		node = (InequalityGraphNode *) lfirst(lc);
		cons = linitial(node->consts);

		/* check for each node the consts it is connected to */
		if (list_length(node->consts) == 1)
		{
			foreach(innerLc, node->lessThen)
			{
				toNode = (InequalityGraphNode *) lfirst(innerLc);

				if (list_length(toNode->consts) != 0)
				{
					/* contradiction (constant smaller than itself) */
					if (equal(cons, linitial(toNode->consts)))
						return true;
				}
			}

			foreach(innerLc, node->lessEqThen)
			{
				toNode = (InequalityGraphNode *) lfirst(innerLc);

				if (list_length(toNode->consts) != 0)
				{
					/* contradiction (constant smaller than itself) */
					if (equal(cons, linitial(toNode->consts)))
					{
						mergeLists = lappend(mergeLists, list_make2(node,toNode));
					}
				}
			}
		}
	}

	mergeCycles(graph, mergeLists);

	/* merge equivalence lists if necessary */
	mergeGraphNodeEquivalenceLists (graph, info);

	return false;
}

/*
 * Checks if an node has more than one constants in his constant list. If this is the case check that all constants are the same.
 * If not return true to indicate an contradiction. If yes remove additional constants and return false.
 */

static bool
checkNodeConsts (InequalityGraphNode *node)
{
	ListCell *lc;
	List *remove;
	Const *constLeft;
	Const *constRight;
	EquivalenceList *equi;

	/* less than one const, so we are safe */
	if (list_length(node->consts) < 2)
		return false;

	/* more than one const, check that all consts are of the same value */
	constLeft = (Const *) linitial(node->consts);
	foreachsince(lc, node->consts->head->next)
	{
		constRight = (Const *) lfirst(lc);

		/* not same value, return true to indicate contradiction */
		if (!equal(constLeft, constRight))		//CHECK the same const are really equal
			return true;
	}

	/* remove additional copies of constant */
	remove = list_copy(node->consts);
	remove->head = remove->head->next;

	foreach(lc, node->equis)
	{
		equi = (EquivalenceList *) lfirst(lc);

		equi->exprs = list_difference(equi->exprs, remove);
	}

	node->consts = list_make1(constLeft);

	return false;
}

/*
 * Remove self edges labeled with "<=". Return true to indicate a contradiction if we find an self edge labeled "<".
 */

static bool
examineSelfEdges (InequalityGraph *graph)
{
	ListCell *lc;
	InequalityGraphNode *node;


	foreach(lc, graph->nodes)
	{
		node = (InequalityGraphNode *) lfirst(lc);

		/* remove self references from lessEqThen */
		removeNodeElem(node->lessEqThen, (Node *) node);

		/* if a node is a member of its lesser than list
		 * we have a contradiction.
		 */
		if (list_member(node->lessThen, node))
			return true;
	}

	return false;
}

/*
 * Search for nodes that are connected by an "<" and an "<=" labeled edge and remove the "<=" labeled edges.
 */

static void
removeParalellEdges (InequalityGraph *graph)
{
	ListCell *lc;
	ListCell *innerLc;
	InequalityGraphNode *fromNode;
	InequalityGraphNode *toNode;

	/* for each node search for paralell edges. (labeled with "<" and "<=") Remove the "<="-labeled edges. */
	foreach(lc, graph->nodes)
	{
		fromNode = (InequalityGraphNode *) lfirst(lc);

		/* walk through "<"-list */
		foreach(innerLc, fromNode->lessThen)
		{
			toNode = (InequalityGraphNode *) lfirst(innerLc);

			if (list_member(fromNode->lessEqThen, toNode))
			{
				removeNodeElem(fromNode->lessEqThen, (Node *) toNode);
				removeNodeElem(toNode->greaterThen, (Node *) toNode);
			}
		}
	}
}

/*
 * Removes redundent incoming and outgoing edges for a node. The following pattern are checked to find
 * redundent edges:
 *		DONT DO IT hinders pushdown! 1) if there are edges from current node E1 to nodes E2 and E3 and an edge from E2 to E3 and the edge
 *		remove edge E1-E3. If edges E1-E2 and E2-E3 are labeled with "<=" and E2-E3 was labeled "<" then relabel E2-E3 with "<="
 *
 *		2) if there are edges from E1 to C1 and C2 and C1 < C2 then remove the edge from E1 to C2
 */

static void
removeRedundentEdgesToNodes (InequalityGraphNode *node)
{
	InequalityGraphNode *minMaxNode;
	List *remove;

	/* find redundent outgoing edges and remove them */
	minMaxNode = NULL;
	remove = NIL;

	removeEdgesToConsts(node, COMP_SMALLER, &minMaxNode, &remove);
	removeEdgesToConsts(node, COMP_SMALLEREQ, &minMaxNode, &remove);

	removeEdgesFromList (node, remove, false);

	/* find redundent incoming edges and remove them */
	minMaxNode = NULL;
	remove = NIL;

	removeEdgesToConsts(node, COMP_BIGGER, &minMaxNode, &remove);
	removeEdgesToConsts(node, COMP_BIGGEREQ, &minMaxNode, &remove);

	removeEdgesFromList (node, remove, true);
}

/*
 *
 */

static void
removeEdgesToConsts (InequalityGraphNode *node, ComparisonType edgeType, InequalityGraphNode **maxMinNode, List **remove)
{
	ListCell *lc;
	InequalityGraphNode *toNode;
	List **edgeList;
	ComparisonType checkType;

	/* get edge list we want to process */
	switch(edgeType)
	{
		case COMP_SMALLER:
			edgeList = &(node->lessThen);
			checkType = COMP_SMALLER;
		break;
		case COMP_SMALLEREQ:
			edgeList = &(node->lessEqThen);
			checkType = COMP_SMALLER;
		break;
		case COMP_BIGGER:
			edgeList = &(node->greaterThen);
			checkType = COMP_BIGGER;
		break;
		case COMP_BIGGEREQ:
			edgeList = &(node->greaterEqThen);
			checkType = COMP_BIGGER;
		break;
		default:
			//TODO error
		break;
	}

	/* find smallest/biggest constant and add other constants to the remove list */
	foreach(lc, *edgeList)
	{
		toNode = (InequalityGraphNode *) lfirst(lc);

		if (IsAConstNode(toNode))
		{
			if (maxMinNode == NULL || checkInequality(*maxMinNode, toNode, checkType))
			{
				*remove = lappend(*remove, *maxMinNode);
				*maxMinNode = toNode;
			}
			else
				*remove = lappend(*remove, toNode);
		}
	}
}

/*
 * given a list of nodes and a start node, this method removes ingoing/outgoing edges from the
 * start node to any node in the list.
 */

static void
removeEdgesFromList (InequalityGraphNode *node, List *edges, bool bigger)
{
	ListCell *lc;
	InequalityGraphNode *toNode;

	foreach(lc, edges)
	{
		toNode = (InequalityGraphNode *) lfirst(lc);

		if (bigger)
		{
			removeNodeElem(node->greaterThen, (Node *) toNode);
			removeNodeElem(node->greaterEqThen, (Node *) toNode);

			removeNodeElem(toNode->lessThen, (Node *) node);
			removeNodeElem(toNode->lessEqThen, (Node *) node);
		}
		else
		{
			removeNodeElem(node->lessThen, (Node *) toNode);
			removeNodeElem(node->lessEqThen, (Node *) toNode);

			removeNodeElem(toNode->greaterThen, (Node *) node);
			removeNodeElem(toNode->greaterEqThen, (Node *) node);
		}
	}

}

/*
 * Checks if an inequality between two constants holds.
 */

static bool
checkInequality (InequalityGraphNode *left, InequalityGraphNode *right, ComparisonType type)
{
	Node *result;
	Node *op;

	if (type == COMP_SMALLER)
		op = createSmallerCondition((Node *) linitial(left->consts),(Node *) linitial(right->consts));
	else
		op = createBiggerCondition((Node *) linitial(left->consts),(Node *) linitial(right->consts));

	result = eval_const_expressions(op);

	if (equal(result, makeBoolConst(true,false)))
		return true;

	return false;
}

/*
 * Given a inequality graph this method generates a inequality expression for each edge in the graph and stores this
 * expression in the provided PushdownInfo. (The old inequalities are removed)
 */

void
generateInequalitiesFromGraph (PushdownInfo *info, InequalityGraph *graph)
{
	ListCell *lc;
	ListCell *innerLc;
	List *ineqs;
	InequalityGraphNode *fromNode;
	InequalityGraphNode *toNode;
	SelectionInfo *newExpr;

	ineqs = getSimpleInequalities (info);

	/* remove old inequalities */
	info->conjuncts = list_difference(info->conjuncts, ineqs);

	/* generate new expressions for the edges in the inequality graph */
	foreach(lc,graph->nodes)
	{
		fromNode = (InequalityGraphNode *) lfirst(lc);

		foreach(innerLc, fromNode->lessThen)
		{
			toNode = (InequalityGraphNode *) lfirst(innerLc);

			newExpr = generateInequality (fromNode, toNode, COMP_SMALLER);
			info->conjuncts = lappend(info->conjuncts, newExpr);
		}

		foreach(innerLc, fromNode->lessEqThen)
		{
			toNode = (InequalityGraphNode *) lfirst(innerLc);

			newExpr = generateInequality (fromNode, toNode, COMP_SMALLEREQ);
			info->conjuncts = lappend(info->conjuncts, newExpr);
		}
	}
}

/*
 * Generates an inequality expression for two inequality graph nodes and wraps this expression in an SelectionInfo node.
 */

static SelectionInfo *
generateInequality (InequalityGraphNode *from, InequalityGraphNode *to, ComparisonType type)
{
	Node *leftOp;
	Node *rightOp;
	EquivalenceList *left;
	EquivalenceList *right;
	Node *ineq;
	SelectionInfo *result = NULL;

	/* get one expression from the equivalence lists of left and right nodes */
	if (list_length(from->consts) != 0)
		leftOp = (Node *) linitial(from->consts);
	else
	{
		left = (EquivalenceList *) linitial(from->equis);
		leftOp = (Node *) linitial(left->exprs);
	}
	if (list_length(to->consts) != 0)
	{
		right = (EquivalenceList *) linitial(to->consts);
		rightOp = (Node *) linitial(right->exprs);
	}

	/* create a smaller OpExpr */
	switch (type)
	{
		case COMP_SMALLER:
			ineq = createSmallerCondition (leftOp, rightOp);
		break;
		case COMP_SMALLEREQ:
			ineq = createSmallerEqCondition (leftOp, rightOp);
		break;
		default:
			//TODO error
		break;
	}

	/* create a selection info */
	//result = createSelectionInfo(ineq);

	return result;
}




/*
 * Return a list of SelectionInfo's from a pushdown info that are inequalities.
 */

List *
getSimpleInequalities (PushdownInfo *info)
{
	List *result;
	SelectionInfo *sel;
	ListCell *lc;

	result = NIL;

	/* search for inequalities */
	foreach(lc, info->conjuncts)
	{
		sel = (SelectionInfo *) lfirst(lc);

		if (selIsInequality(sel))
		{
			result = lappend(result, sel);
		}
	}

	return result;
}
