/*-------------------------------------------------------------------------
 *
 * prov_set_util.c
 *	  PERM C 
 *
 * Portions Copyright (c) 2012 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $Perm: pgsql//SF_Perm/src/backend/provrewrite/set/prov_set_util.c,v 1.542 2012-03-20 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "optimizer/clauses.h"
#include "nodes/makefuncs.h"			// needed to create new nodes
#include "parser/parse_expr.h"			// expression transformation used for expression type calculation
#include "parser/parsetree.h"
#include "parser/parse_relation.h"
#include "utils/guc.h"

#include "provrewrite/prov_copy_map.h"
#include "provrewrite/prov_sublink_util_mutate.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/provlog.h"
#include "provrewrite/prov_set_util.h"

/* prototypes */
static bool substractRangeTblRefValuesWalker (Node *node, void *context);
static void replaceSetOperatorSubtree (Query *query, SetOperationStmt *setOp,
		Node **parent);
static Node *createEqualityCondition (List* leftAttrs, List* rightAttrs,
		Index leftIndex, Index rightIndex, BoolExprType boolOp, bool neq);
static List *getAttributesForJoin (RangeTblEntry *rte);

/*
 * Removes any superficial RTEs produced by postgres rewriter (view expansion), because this RTE's mess up the set rewrite.
 * CHECK if these things aren't needed by the optimizer or SQL reconstructor
 * TODO do this for the whole query before provenance rewrite?
 */

void
removeDummyRewriterRTEs (Query *query)
{
	RangeTblEntry *rte;
	ListCell *lc;
	TargetEntry *te;
	Var *var;

	/* check if first range table entry is a dummy entry */
	rte = (RangeTblEntry *) linitial (query->rtable);
	if (strcmp(rte->alias->aliasname, "*OLD*") == 0)
	{
		/* remove RTEs from range table */
		pfree(rte);

		rte = (RangeTblEntry *) lsecond (query->rtable);
		pfree(rte);

		lc = query->rtable->head;
		query->rtable->head = query->rtable->head->next->next;
		query->rtable->length = query->rtable->length - 2;

		pfree(lc->next);
		pfree(lc);

		/* adapt Target list */
		foreach(lc, query->targetList)
		{
			te = (TargetEntry *) lfirst(lc);
			var = (Var *) te->expr;
			var->varno -= 2;
		}

		/* adapt set operation tree */
		substractRangeTblRefValuesWalker (query->setOperations, NULL);

		/* for copy cs adapt the copy map entries */
		if (IS_COPY(query))
			adaptCopyMapForDummyRTERemoval(query);
	}
}

/*
 *	Walks a join tree and substract two from every RangeTblRef rtindex.
 *	Needed by the rewrite that changes the structure of the query.
 */

static bool
substractRangeTblRefValuesWalker (Node *node, void *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, RangeTblRef))
	{
		RangeTblRef *rtRef;

		rtRef = (RangeTblRef *) node;
		rtRef->rtindex -= 2;
	}

	return expression_tree_walker(node, substractRangeTblRefValuesWalker,
			context);
}

/*
 *	Returns lists with RTindexes and RangeTblEntries that are accessed by set
 *	operation tree under setTreeNode. If rtes or rtindex is null only the
 *	other list is filled.
 */

void
findSetOpRTEs (List *rtable, Node *setTreeNode, List **rtes, List **rtindex)
{
	SetOperationStmt *setOp;
	Index rtIndex;
	RangeTblEntry *rtEntry;

	if (IsA(setTreeNode, SetOperationStmt))
	{
		setOp = (SetOperationStmt *) setTreeNode;
		findSetOpRTEs (rtable, setOp->larg, rtes, rtindex);
		findSetOpRTEs (rtable, setOp->rarg, rtes, rtindex);
	}
	else if (IsA(setTreeNode, RangeTblRef))
	{
		rtIndex = ((RangeTblRef *) setTreeNode)->rtindex;
		rtEntry = (RangeTblEntry *) list_nth(rtable, (rtIndex - 1));
		if (rtes)
			*rtes = lappend(*rtes, rtEntry);
		if (rtindex)
			*rtindex = lappend_int(*rtindex, rtIndex);
	}
	else
		elog(ERROR,
				"Unexpected node of type %d in set operation tree",
				setTreeNode->type);
}

/*
 * For a node in a Set operation tree return a list with the
 * range table references stored in the leaf nodes of the sub tree
 * under this node.
 */

List *
getSetOpRTRefs (Node *setTreeNode)
{
	List *result;
	SetOperationStmt *setOp;
	RangeTblRef *rtRef;

	result = NIL;
	if (IsA(setTreeNode, SetOperationStmt))
	{
		setOp = (SetOperationStmt *) setTreeNode;
		result = list_concat(result, getSetOpRTRefs (setOp->larg));
		result = list_concat(result, getSetOpRTRefs (setOp->rarg));
	}
	else if (IsA(setTreeNode, RangeTblRef))
	{
		rtRef = ((RangeTblRef *) setTreeNode);
		result = lappend(result, rtRef);
	}
	else
		elog(ERROR,
				"Unexpected node of type %d in set operation tree",
				setTreeNode->type);

	return result;
}

/*
 * Walks through a set operation tree of a query. If only one type of operators
 * (union or intersection) is found nothing is done. If a different operator or
 * a set difference operator is found, the whole subtree under this operator is extracted into
 * a new query node. E.g.:
 * 		SELECT * FROM r UNION (SELECT * FROM s UNION SELECT * FROM t); would be left unchanged.
 * 		SELECT * FROM r UNION (SELECT * FROM s INTERSECT SELECT * FROM t); would be changed into
 * 		SELECT * FROM r UNION (SELECT * FROM (SELECT * FROM s INTERSECT SELECT * FROM t)) AS sub;
 *
 * If the GUC parameter prov_use_set_optimization is not set, then each set operator is placed in
 * its own query node regardless of its type.
 */

void
replaceSetOperationSubTrees (Query *query, Node *node, Node **parentPointer, SetOperation rootType)
{
	SetOperationStmt *setOp;

	/* RangeTblRef are ignored */
	if (!IsA(node,SetOperationStmt))
		return;

	/* cast to set operation stmt */
	setOp = (SetOperationStmt *) node;

	/* if the user deactivated the optimized set operation rewrites we outsource each set operation into
	 * a separate query.
	 */
	if (!prov_use_set_optimization)
	{
		if (IsA(setOp->larg, SetOperationStmt))
			replaceSetOperatorSubtree (query, (SetOperationStmt *) setOp->larg, &(setOp->larg));

		if (IsA(setOp->rarg, SetOperationStmt))
			replaceSetOperatorSubtree (query, (SetOperationStmt *) setOp->rarg, &(setOp->rarg));
	}

	/*
	 *  Optimization is activated. Keep original set operation tree, if it just contains only union or only intersection operations.
	 *  For a set difference operation both operands are outsourced into a separate query (If they are not simple range table references).
	 *  For a mixed unions and intersections we have to outsource sub trees under set operations differend from the root set operation.
	 */

	switch (setOp->op)
	{
		/* union or intersect, do not change anything */
		case SETOP_UNION:
		case SETOP_INTERSECT:
			/* check if of the same type as parent */
			if (setOp->op == rootType)
			{
				replaceSetOperationSubTrees (query, setOp->larg, &(setOp->larg), rootType);
				replaceSetOperationSubTrees (query, setOp->rarg, &(setOp->rarg), rootType);
			}
			/* another type replace subtree */
			else
				replaceSetOperatorSubtree(query, setOp, parentPointer);
		break;
		/* set difference, replace subtree with new query node */
		case SETOP_EXCEPT:
			/* if is root set operation replace left and right sub trees */
			if (rootType == SETOP_EXCEPT) {
				if (IsA(setOp->larg, SetOperationStmt))
					replaceSetOperatorSubtree (query, (SetOperationStmt *)
							setOp->larg, &(setOp->larg));

				/* is wl semantics is used the right subtree can be left
				 * untouched */
				if (IsA(setOp->rarg, SetOperationStmt)
						&& !prov_use_wl_union_semantics)
					replaceSetOperatorSubtree (query, (SetOperationStmt *)
							setOp->rarg, &(setOp->rarg));
			}
			/* is not root operation process as for operator change */
			else
				replaceSetOperatorSubtree(query, setOp, parentPointer);
		break;
		default:
			elog(ERROR,
					"Unknown set operation type: %d",
					setOp->op);
		break;
	}
}

/*
 * Replaces a subtree in an set operation tree with a new subquery that represents the
 * set operations performed by the sub tree.
 */

static void
replaceSetOperatorSubtree (Query *query, SetOperationStmt *setOp, Node **parent)
{
	ListCell *lc;
	List *subTreeRTEs;
	List *subTreeRTindex;
	List *subTreeRTrefs;
	List *queryRTrefs;
	List *newRtable;
	Query *newSub;
	RangeTblEntry *rte;
	RangeTblRef *rtRef;
	int counter;
	int *context;

	subTreeRTEs = NIL;
	subTreeRTindex = NIL;

	/* find all range table entries referenced from the subtree under setOp */
	findSetOpRTEs(query->rtable,(Node *) setOp, &subTreeRTEs, &subTreeRTindex);

	/* create new query node for subquery */
	newSub = (Query *) copyObject(query);
	newSub->rtable = NIL;
	newSub->setOperations = (Node *) setOp; //CHECK ok to not copy?

	/* create range table entries for range table entries referenced from set operation in subtree */
	foreach(lc,subTreeRTEs)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		newSub->rtable = lappend(newSub->rtable, (RangeTblEntry *) copyObject(rte));
	}

	/* adapt RTErefs in sub tree */
	subTreeRTrefs = getSetOpRTRefs((Node *) newSub->setOperations);

	counter = 1;
	foreach(lc, subTreeRTrefs)
	{
		rtRef = (RangeTblRef *) lfirst(lc);
		rtRef->rtindex = counter;
		counter++;
	}

	/* add new sub query to range table */
	addSubqueryToRTWithParam (query, newSub, "newSub", false, ACL_NO_RIGHTS, true);

	/* replace subtree with RTE reference */
	MAKE_RTREF(rtRef, list_length(query->rtable));
	*parent = (Node *) rtRef;

	LOGNODE(query, "before range table adapt");

	/* adapt range table and rteRefs for query */
	newRtable = NIL;
	queryRTrefs = getSetOpRTRefs(query->setOperations);

	foreach(lc, queryRTrefs)
	{
		rtRef = lfirst(lc);
		rte = rt_fetch(rtRef->rtindex, query->rtable);
		newRtable = lappend(newRtable, rte);
		rtRef->rtindex = list_length(newRtable);
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
 * Creates the condition to join the original set operation result with
 * one of its rewritten input on equality (or inequality).
 */

void
createSetJoinCondition (Query *query, JoinExpr *join, Index leftIndex, Index rightIndex, bool neq)
{
	List *leftAttrs;
	List *rightAttrs;
	RangeTblEntry *rte;
	Index origTlength;

	origTlength = list_length(query->targetList);

	rte = rt_fetch(leftIndex + 1, query->rtable);
	leftAttrs = getAttributesForJoin(rte);
	leftAttrs = list_truncate (leftAttrs, origTlength);

	rte = rt_fetch(rightIndex + 1, query->rtable);
	rightAttrs = getAttributesForJoin(rte);
	rightAttrs = list_truncate (rightAttrs, origTlength);

	join->quals = createEqualityCondition(leftAttrs, rightAttrs, leftIndex, rightIndex, AND_EXPR, neq);
}


/*
 * Helper method that generates a list of target entries for the join
 * condition created by createSetJoinCondition.
 */

static List *
getAttributesForJoin (RangeTblEntry *rte)
{
	List *result;
	TargetEntry *te;
	ListCell *varLc;
	ListCell *nameLc;
	Value *name;
	Var *var;
	Index curAttno;

	if (rte->rtekind == RTE_SUBQUERY)
		result = copyObject(rte->subquery->targetList);
	else
	{
		result = NIL;

		curAttno = 1;
		forboth(varLc, rte->joinaliasvars, nameLc, rte->eref->colnames)
		{
			var = (Var *) lfirst(varLc);
			name = (Value *) lfirst(nameLc);

			te = makeTargetEntry((Expr *) var,
					curAttno,
					name->val.str,
					false);

			result = lappend(result, te);
			curAttno++;
		}
	}
	return result;
}

/*
 * Creates an equality condition expression for two lists of attributes.
 * E.g. A = (a,b,c) and B = (d,e,f) then the following condition would be created:
 * a = d AND b = e AND c = f. If neq is true the whole condition is negated.
 */

static Node *
createEqualityCondition (List* leftAttrs, List* rightAttrs, Index leftIndex, Index rightIndex, BoolExprType boolOp, bool neq)
{
	ListCell *leftLc;
	ListCell *rightLc;
	TargetEntry *curLeft;
	TargetEntry *curRight;
	OpExpr *equal;
	List *equalConds;
	Var *leftOp;
	Var *rightOp;
	Node *curRoot;

	equalConds = NIL;

	Assert (list_length(leftAttrs) == list_length(rightAttrs));

	/* create List of OpExpr nodes for equality conditions */
	forboth (leftLc, leftAttrs, rightLc, rightAttrs)
	{
		curLeft = (TargetEntry *) lfirst(leftLc);
		curRight = (TargetEntry *) lfirst(rightLc);

		/* create Var for left operand of equality expr */
		leftOp = makeVar (leftIndex + 1,
				curLeft->resno,
				exprType ((Node *) curLeft->expr),
				exprTypmod ((Node *) curLeft->expr),
				0);

		/* create Var for right operand of equality expr */
		rightOp = makeVar (rightIndex + 1,
				curRight->resno,
				exprType ((Node *) curRight->expr),
				exprTypmod ((Node *) curRight->expr),
				0);

		/* get equality operator for the var's type */
		equal = (OpExpr *) createNotDistinctConditionForVars (leftOp, rightOp);

		/* append current equality condition to equalConds List */
		equalConds = lappend (equalConds, equal);
	}

	curRoot = (Node *) createAndFromList(equalConds);

	/* negation required */
	if (neq)
		curRoot = (Node *) makeBoolExpr(NOT_EXPR, list_make1(curRoot));

	return curRoot;
}

/*
 *
 */

void
adaptSetStmtCols (SetOperationStmt *stmt, List *colTypes, List *colTypmods)
{
	stmt->colTypes = colTypes;
	stmt->colTypmods = colTypmods;
	stmt->all = true;
	if (IsA(stmt->larg, SetOperationStmt))
		adaptSetStmtCols ((SetOperationStmt *) stmt->larg, colTypes, colTypmods);
	if (IsA(stmt->rarg, SetOperationStmt))
		adaptSetStmtCols ((SetOperationStmt *) stmt->rarg, colTypes, colTypmods);
}
