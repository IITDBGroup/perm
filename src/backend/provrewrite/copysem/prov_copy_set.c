/*-------------------------------------------------------------------------
 *
 * prov_copy_set.c
 *	  POSTGRES C
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/copysem/prov_copy_set.c,v 1.542 23.06.2009 11:36:20 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "nodes/nodes.h"
#include "utils/guc.h"
#include "parser/parsetree.h"
#include "optimizer/clauses.h"

#include "provrewrite/provlog.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_set.h"
#include "provrewrite/prov_sublink_util_mutate.h"
#include "provrewrite/prov_copy_set.h"
#include "provrewrite/prov_copy_map.h"
#include "provrewrite/prov_copy_util.h"
#include "provrewrite/provrewrite.h"

/* prototypes */
static void rewriteSetRTEs (Query *newTop, List **subList);
static void createRangeTable (Query *top, Query *query);
static void restructureSetOperationQueryNode (Query *top);
static void createCopyJoinsForSetOp (Query *top, Query *query);
static SetOperation getSetOpType (SetOperationStmt *setOp);

static void replaceSetOperationSubTrees (Query *top, Node *node, Node **parentPointer, SetOperation rootType);
static void replaceSetOperatorSubtree (Query *top, SetOperationStmt *setOp, Node **parent);
static void correctChildMapRtindex(Query *query);
static void correctNoRewriteRelEntry (CopyMapRelEntry *rel);

static void setRelMapsAttrno (List *relMaps, int newVarno);
static void adaptCondVarnos (InclusionCond *cond, int varno);
static bool adaptVarnoWalker (Node *node, int *context);

/*
 *
 */

Query *
rewriteCopySetQuery (Query *query)
{
	List *subList;
	Query *newTop;
	Query *orig;
	int numSubs;
	int *context;

	subList = NIL;

	orig = copyObject(query);

	/* Should the query be rewritten at all? If not fake provenance attributes. */
	if (!shouldRewriteQuery(query))//CHECK can this happen at all, because we would never rewrite the set query node anyway? Could happen after restructure??
		return query;

	/* remove RTEs produced by postgres rewriter */
	removeDummyRewriterRTEs(query);

	/* restructure set operation query if necessary */
	restructureSetOperationQueryNode (query);

	/* create new top node */
	newTop = makeQuery();
	newTop->targetList = copyObject(query->targetList);
	newTop->intoClause = copyObject(query->intoClause);
	Provinfo(newTop)->copyInfo = (Node *) GET_COPY_MAP(query);
	SetSublinkRewritten(newTop, true);
	createRangeTable(newTop, query);

	/* add original query as first range table entry */
	addSubqueryToRTWithParam (newTop, orig, "originalSet", false, ACL_NO_RIGHTS, false);

	/* rewrite the subqueries used in the set operation */
	numSubs = list_length(query->rtable);
	rewriteSetRTEs (newTop, &subList);

	/* correct alias of rewritten subqueries */
	correctSubQueryAlias (newTop);

	/* join the original query with all rewritten subqueries */
	createCopyJoinsForSetOp (newTop, query);

	/* add provenance attributes from subqueries to target list */
	copyAddProvAttrs(newTop, subList);

	/* increase sublevelsup of vars in case we are rewriting in an sublink query */
	context = (int *) palloc(sizeof(int));
	*context = -1;
	increaseSublevelsUpMutator((Node *) orig, context);
	pfree(context);

	return newTop;
}


/*
 * Create the joins for a rewritten set operation node
 */

static void
createCopyJoinsForSetOp (Query *top, Query *query)
{
	RangeTblRef *rtRef;
	JoinExpr *newJoin;
	JoinType joinType;
	int i, origRtableLength;

	/* define join type to use in join expression */
	switch (getSetOpType((SetOperationStmt *) query->setOperations))
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
 * If set operation tree contains UNION/INTERSECTION with possibly an EXCEPT return UNION or INTERSECTION. If
 * set operation tree contains only EXCEPTs return EXCEPT.
 */

static SetOperation
getSetOpType (SetOperationStmt *setOp)
{
	if (setOp->op == SETOP_EXCEPT)
	{
		if (IsA(setOp->larg, RangeTblRef))
			return SETOP_EXCEPT;

		return getSetOpType((SetOperationStmt *) setOp->larg);
	}

	return setOp->op;
}


/*
 * Create the range table for the new top query joining the original set
 * operation with the rewritten input's.
 */

static void
createRangeTable (Query *top, Query *query)
{
	ListCell *lc;
	RangeTblEntry *rte;
	Index curRte;
	CopyMap *map;
	List *origRtable;
//	List *relMaps;
	List *rtIndexMapping = NIL;
	CopyMapRelEntry *relEntry;
	int i, rtIndex;

	curRte = 2;
	map = GET_COPY_MAP(top);
	origRtable = query->rtable;
	query->rtable = NIL;

	// add subqueries that should be rewritten to top range table.
	foreachi(lc, i, origRtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		query->rtable = lappend(query->rtable, copyObject(rte));

		/* rte should be rewritten add it to new top rtable and store its old
		 * rtindex */
		if(shouldRewriteRTEforMap(map, i + 1))
		{
			top->rtable = lappend(top->rtable, rte);
			rtIndexMapping = lappend_int(rtIndexMapping, i + 1);
			curRte++;
		}
	}

	foreach(lc, map->entries)
	{
		relEntry = (CopyMapRelEntry *) lfirst(lc);
		rtIndex = listPositionInt(rtIndexMapping, relEntry->child->rtindex) + 2;

		if (rtIndex != 1)
			setRelMapsAttrno(list_make1(relEntry), rtIndex);
		else
			correctNoRewriteRelEntry (relEntry);
	}
}

/*
 *
 */

static void
correctNoRewriteRelEntry (CopyMapRelEntry *rel)
{
	rel->rtindex = -1;
	//CHECK necessary at all???
}

/*
 *
 */

static void
setRelMapsAttrno (List *relMaps, int newVarno)
{
	ListCell *lc, *innerLc, *attrLc, *condLc, *inCondLc;
	CopyMapRelEntry *rel;
	CopyMapEntry *attr;
	AttrInclusions *attIncl, *innerIncl;
	InclusionCond *cond, *innerCond;
//	Var *var;

	foreach(lc, relMaps)
	{
		rel = (CopyMapRelEntry *) lfirst(lc);
		rel->child->rtindex = newVarno;

		foreach(innerLc, rel->attrEntries)
		{
			attr = (CopyMapEntry *) lfirst(innerLc);

			foreach(attrLc, attr->outAttrIncls)
			{
				attIncl = (AttrInclusions *) lfirst(attrLc);

				foreach(condLc, attIncl->inclConds)
				{
					cond = (InclusionCond *) lfirst(condLc);

					if (IsA(cond->existsAttr, AttrInclusions))
					{
						innerIncl = (AttrInclusions *) cond->existsAttr;

						innerIncl->attr->varno = newVarno;

						foreach(inCondLc, innerIncl->inclConds)
						{
							innerCond = (InclusionCond *) lfirst(inCondLc);

							adaptCondVarnos(innerCond, newVarno);
						}
					}
				}
			}
		}
	}
}

/*
 *
 */

static void
adaptCondVarnos (InclusionCond *cond, int varno)
{
	Var *var;
	ListCell *lc;

	if (IsA(cond->existsAttr,Var))
	{
		var = (Var *) cond->existsAttr;
		var->varno = varno;
	}

	if (cond->inclType == INCL_EQUAL)
	{
		foreach(lc, cond->eqVars)
		{
			var = (Var *) lfirst(lc);
			var->varno = varno;
		}
	}

	if (cond->inclType == INCL_IF)
		adaptVarnoWalker(cond->cond, &varno);
}

/*
 *
 */

static bool
adaptVarnoWalker (Node *node, int *context)
{
	Var *var;

	if (node == NULL)
		return false;

	if (IsA(node, Var))
	{
		var = (Var *) node;

//TODO		if (var->varlevelsup == 0) only if not in sublink
		var->varno = *context;
	}

	return expression_tree_walker(node, adaptVarnoWalker, (void *) context);
}


/*
 * Rewrites the range table entries of a set query.
 */

static void
rewriteSetRTEs (Query *newTop, List **subList)
{
	ListCell *lc;
	RangeTblEntry *rte;
	Query *query;
	int i;

	for(lc = newTop->rtable->head->next, i = 2; lc != NULL; lc = lc->next, i++)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		if (shouldRewriteRTEforMap(GET_COPY_MAP(newTop), i))
		{
			query = rewriteQueryNodeCopy (rte->subquery);
			rte->subquery = query;

			*subList = lappend_int(*subList, i);
		}
	}
}




/*
 * 	restructureSetOperationQueryNode (query);
 */

static void
restructureSetOperationQueryNode (Query *top)
{
	SetOperation rootType;
	SetOperationStmt *setOp;

	/* get the root node set operation type */
	setOp = (SetOperationStmt *) top->setOperations;
	rootType = setOp->op;

	/* if it is an except try to find a left child thats not an except */
	while (!IsA(setOp->larg, RangeTblRef) && rootType == SETOP_EXCEPT)
	{
		setOp = (SetOperationStmt *) setOp->larg;
		rootType = setOp->op;
	}

	replaceSetOperationSubTrees(top, top->setOperations, &(top->setOperations), rootType);
}

/*
 * Walks through a set operation tree of a query. If only one type of operators
 * (union or intersection) is found nothing is done. If a different operator or
 * a set difference operator is found, the whole subtree under this operator is
 * extracted into a new query node.
 */

static void
replaceSetOperationSubTrees (Query *top, Node *node, Node **parentPointer,
		SetOperation rootType)
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
			replaceSetOperatorSubtree (top, (SetOperationStmt *) setOp->larg,
					&(setOp->larg));

		if (IsA(setOp->rarg, SetOperationStmt))
			replaceSetOperatorSubtree (top, (SetOperationStmt *) setOp->rarg,
					&(setOp->rarg));
	}

	/*
	 *  Optimization is activated. Keep original set operation tree, if it just
	 *  contains only union or only intersection operations. For a set
	 *  difference operation both operands are outsourced into a separate query
	 *  (If they are not simple range table references). For a mixed unions and
	 *  intersections we have to outsource sub trees under set operations
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
				replaceSetOperationSubTrees (top, setOp->larg, &(setOp->larg),
						rootType);
				replaceSetOperationSubTrees (top, setOp->rarg, &(setOp->rarg),
						rootType);
			}
			/* another type replace subtree */
			else
				replaceSetOperatorSubtree(top, setOp, parentPointer);
		break;
		/* set difference, replace subtree with new query node */
		case SETOP_EXCEPT:
			/* if is root set operation ignore it because it only propagates
			 * provenance from its left subtree */
			replaceSetOperationSubTrees (top, setOp->larg, &(setOp->larg),
					rootType);
		break;
		default:
			//TODO error
		break;
	}
}

/*
 * Replaces a subtree in an set operation tree with a new subquery that
 * represents the set operations performed by the sub tree. Adapt Copy Map
 * accordingly.
 */

static void
replaceSetOperatorSubtree (Query *top, SetOperationStmt *setOp, Node **parent)
{
	ListCell *lc, *indexLc, *origRelLc, *newRelLc;
	List *subTreeRTEs = NIL;
	List *subTreeRTindex = NIL;
	List *subTreeRTrefs;
	List *queryRTrefs;
	List *newRtable;
	List *newRTposMap;
	Query *newSub;
	RangeTblEntry *rte;
	RangeTblRef *rtRef;
	int rtIndex, i, newSubRTindex, newRtindex;
	int *context;
	CopyMap *newMap, *map;
	CopyMapRelEntry *oldRel, *newRel;
	List *origSubRelEntries;
	List *newSubRelEntries;
	List *individSubEntries = NIL;
	bool notSeen;

	map = GET_COPY_MAP(top);

	/* find all range table entries referenced from the subtree under setOp */
	findSetOpRTEs(top->rtable,(Node *) setOp, &subTreeRTEs, &subTreeRTindex);

	/* create new query node for subquery */
	newSub = flatCopyWithoutProvInfo (top);
	newSub->setOperations = (Node *) copyObject(setOp);
	Provinfo(newSub)->copyInfo = (Node *) makeCopyMap ();
	newMap = GET_COPY_MAP(newSub);

	/* create range table entries for range table entries referenced from set
	 * operation in subtree */
	forboth(lc, subTreeRTEs, indexLc, subTreeRTindex)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		rtIndex = lfirst_int(indexLc);

		newSub->rtable = lappend(newSub->rtable, (RangeTblEntry *) rte);

		origSubRelEntries = getAllEntriesForRTE(map, rtIndex);
		newSubRelEntries = copyObject(origSubRelEntries);
		individSubEntries = lappend(individSubEntries, newSubRelEntries);

		/* Change the child links of the original sub rel entries and create
		 * simple inclusion conditions for the new rel entries of top */
		forboth(origRelLc, origSubRelEntries, newRelLc, newSubRelEntries)
		{
			oldRel = (CopyMapRelEntry *) lfirst(origRelLc);
			newRel = (CopyMapRelEntry *) lfirst(newRelLc);

			/* Link child of old rel entry as the child of the new entry
			 * and set new entry as child of the old entry */
			newRel->child = oldRel->child;
			oldRel->child = newRel;
			newRel->rtindex = newRel->child->rtindex;
		}

		newMap->entries = list_concat(newMap->entries, newSubRelEntries);
	}

	/* adapt RTErefs in sub tree */
	subTreeRTrefs = getSetOpRTRefs((Node *) newSub->setOperations);

	forbothi(lc, indexLc, i, subTreeRTrefs, subTreeRTindex)
	{
		rtRef = (RangeTblRef *) lfirst(lc);
		rtIndex = lfirst_int(indexLc);

		rtRef->rtindex = i + 1;
	}

	/* add new sub query to range table */
	addSubqueryToRTWithParam (top, newSub, "newSub", false, ACL_NO_RIGHTS,
			true);

	/* replace subtree with RTE reference */
	newSubRTindex = list_length(top->rtable);
	MAKE_RTREF(rtRef, newSubRTindex);
	*parent = (Node *) rtRef;

	/* adapt range table and rteRefs for query */
	newRtable = NIL;
	queryRTrefs = getSetOpRTRefs(top->setOperations);

	newRTposMap = NIL;

	/* create new range table and remember the mapping between old and new
	 * range table items */
	notSeen = true;
	foreachi(lc, rtIndex, queryRTrefs)
	{
		rtRef = (RangeTblRef *) lfirst(lc);
		rte = rt_fetch(rtRef->rtindex, top->rtable);

		if (rtRef->rtindex == newSubRTindex && notSeen)
		{
			newSubRTindex = rtIndex + 1;
			notSeen = false;
		}

		newRtable = lappend(newRtable, rte);
		newRTposMap = lappend_int(newRTposMap, rtRef->rtindex);
		rtRef->rtindex = rtIndex + 1;
	}

	top->rtable = newRtable;

	/* Walk through old copy map and adapt copy map rel entries of top query
	 * node by setting the varno's of attributes and inclusion conditions and
	 * the rtindex of the child entry of each rel entry. */
	foreach(lc, map->entries)
	{
		oldRel = (CopyMapRelEntry *) lfirst(lc);

		newRtindex = listPositionInt(newRTposMap, oldRel->child->rtindex) + 1;

		if (newRtindex == 0)
			setRelMapsAttrno(list_make1(oldRel), newSubRTindex);
		else
			setRelMapsAttrno(list_make1(oldRel), newRtindex);
	}

	/* Adapt copy map rel entries of new sub query */
	foreach(lc, newMap->entries)
	{
		oldRel = (CopyMapRelEntry *) lfirst(lc);

		newRtindex =
				listPositionInt(subTreeRTindex, oldRel->child->rtindex) + 1;
		setRelMapsAttrno(list_make1(oldRel), newRtindex);
	}

	correctChildMapRtindex(top);
	correctChildMapRtindex(newSub);

	/* increase sublevelsup of newSub if we are rewritting a sublink query */
	context = (int *) palloc(sizeof(int));
	*context = -1;
	increaseSublevelsUpMutator ((Node *) newSub, context);
	pfree(context);

	correctSubQueryAlias(top);

	LOGNODE(top, "after replace of subtree");
}

/*
 * Adapt the rtindex values of the child Query CopyMaps of a QueryNode.
 */

static void
correctChildMapRtindex(Query *query)
{
	CopyMap *childMap;
	RangeTblEntry *rte;
	ListCell *lc;
	int pos;

	foreachi(lc, pos, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		if (rte->rtekind == RTE_SUBQUERY)
		{
			childMap = GET_COPY_MAP(rte->subquery);
			childMap->rtindex = pos + 1;
		}
	}
}
