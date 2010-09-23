/*-------------------------------------------------------------------------
 *
 * copy/prov_copy_util.c
 *	  PERM C - Utility functions for copy contribution semantics
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/provrewrite.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "nodes/makefuncs.h"
#include "parser/parsetree.h"
#include "parser/parse_expr.h"
#include "optimizer/clauses.h"

#include "provrewrite/prov_util.h"
#include "provrewrite/provlog.h"
#include "provrewrite/provattrname.h"
#include "provrewrite/prov_copy_util.h"
#include "provrewrite/prov_copy_map.h"
#include "provrewrite/prov_copy_inclattr.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/provstack.h"

static List *getRTindicesForJoin (Node *joinTreeNode);
static void addProvAttrFromStack (Query *query, Index rtindex, List **pList,
		List **subPstack);
static void addDummyProvenanceAttributesForRTE (Query *query, Index rtindex,
		List **pList);
static void addDummyProvenanceAttrsForBaseRel (CopyMapRelEntry *rel,
		Query *query, List **pList);


/*
 * Replace all Var nodes in a condition with base RTE vars. This means all
 * references to join Vars are replaced by the base RTE var they represent.
 */

Node *
conditionOnRteVarsMutator (Node *node, Query *context)
{
	if (node == NULL)
		return NULL;

	if (IsA(node, Var))
	{
		Var *in;

		in = (Var *) node;

		return copyObject(resolveToRteVar(in, context));
	}

	return expression_tree_mutator(node, conditionOnRteVarsMutator,
			(void *) context);
}

/*
 * Resolve the join alias jars to the RTE vars they are derived from.
 */

List *
getRteVarsForJoin (Query *query, JoinExpr *join)
{
	List *result;
	ListCell *lc;
	RangeTblEntry *rte;
	Var *in;

	rte = rt_fetch(join->rtindex, query->rtable);

	foreach(lc, rte->joinaliasvars)
	{
		in = (Var *) lfirst(lc);
		result = lappend(result, copyObject(resolveToRteVar(in, query)));
	}

	return result;
}

/*
 *
 */

List *
getCopyRelsForRtindex (Query *query, Index rtindex)
{
	CopyMap *map = GET_COPY_MAP(query);
	List *result = NIL;
	ListCell *lc;
	CopyMapRelEntry *rel;
	RangeTblEntry *rte;

	// get RTE
	rte = rt_fetch(rtindex, query->rtable);

	/* if it's a join get the indicies of the base rte accessed by the join and
	 * try for each CopyMapRelEntry if it belongs to one of these indices. */
	if (rte->rtekind == RTE_JOIN)
	{
		List *rtIndices;
		ListCell *innerLc;
		RangeTblEntry *baseRte;

		rtIndices = getRTindicesForJoin(getJoinTreeNode(query, rtindex));

		foreach(lc, map->entries)
		{
			rel = (CopyMapRelEntry *) lfirst(lc);

			foreach(innerLc, rtIndices)
			{
				if (rel->rtindex == lfirst_int(innerLc))
				{
					result = lappend(result, rel);
					break;
				}
			}
		}
	}

	foreach(lc, map->entries)
	{
		rel = (CopyMapRelEntry *) lfirst(lc);

		if (rel->rtindex == rtindex)
			result = lappend(result, rel);
	}

	return result;
}

/*
 * Get the range table indices for all range table entries accessed by a join.
 */

static List *
getRTindicesForJoin (Node *joinTreeNode)
{
	if (IsA(joinTreeNode, JoinExpr))
	{
		JoinExpr *join = (JoinExpr *) joinTreeNode;

		return list_concat(getRTindicesForJoin(join->larg),
				getRTindicesForJoin(join->rarg));
	}
	else
	{
		RangeTblRef *rtRef = (RangeTblRef *) joinTreeNode;

		return list_make1_int(rtRef->rtindex);
	}
}

/*
 *
 */

List *
copyAddProvAttrsForSet (Query *query, List *subList, List *pList)//TODO add copy map attributes
{
	CopyMap *map;
	CopyMapRelEntry *rel;
	ListCell *lc;
	List *curPstack;
	Index rtIndex;

	map = GET_COPY_MAP(query);
	curPstack = popListAndReverse (&pStack, list_length(subList));

	foreach(lc, map->entries)
	{
		rel = (CopyMapRelEntry *) lfirst(lc);

		/* is propagating? then we have to have an rte for it */
		if (!rel->noRewrite)
		{
			rtIndex = rel->rtindex;

			while(lc->next && ((CopyMapRelEntry *)
					lc->next->data.ptr_value)->rtindex == rtIndex)
					lc = lc->next;

			addProvAttrFromStack (query, rtIndex, &pList, &curPstack);
		}
		else
			addDummyProvenanceAttrsForBaseRel (rel, query, &pList);
	}

	return pList;
}

/*
 * Create fake provenance attributes for a not rewritten query.
 */

List *
copyAddProvAttrForNonRewritten (Query *query)
{
	ListCell *lc, *innerLc;
	List *pList;
	List *targetList;
	CopyMapRelEntry *entry;
	CopyMapEntry *attr;
	TargetEntry *newTe;
	Expr *expr;
	Index curResno;

	pList = NIL;
	curResno = list_length(query->targetList) + 1;

	targetList = query->targetList;

	foreach(lc, GET_COPY_MAP(query)->entries)
	{
		entry = (CopyMapRelEntry *) lfirst(lc);

		/* create proveance attr for each base rel attr */
		foreach(innerLc, entry->attrEntries)
		{
			attr = (CopyMapEntry *) lfirst(innerLc);

			expr = (Expr *) makeNullConst(attr->baseRelAttr->vartype,
					attr->baseRelAttr->vartypmod);

			newTe = makeTargetEntry(expr, curResno, strdup(attr->provAttrName),
					false);

			/* append to targetList and pList */
			targetList = lappend (targetList, newTe);
			pList = lappend (pList, newTe);

			/* increase current resno */
			curResno++;
		}
	}

	query->targetList = targetList;

	return pList;
}


/*
 * Add provenance attributes to a query. The query should have been rewritten
 * beforehand. The provenance attributes are added for each range table entry.
 * If a RTE is rewritten than just add its provenance attributes like for
 * influence contribution semantics. Else add null-constants as Dummy
 * provenance attributes to generate a consistent output schema.
 */

List *
copyAddProvAttrs (Query *query, List *subList, List *pList)
{
	ListCell *subqLc;
	List *subPStack;
	Index curSubquery;
	int origAttrNum = list_length(query->targetList);

	subPStack = popListAndReverse (&pStack, list_length(subList));

	//TODO iterate over rel maps
	/* for each subquery of query ... */
	foreach (subqLc, subList)
	{
		curSubquery = (Index) lfirst_int(subqLc);

		/* if current rte contains parts that are rewritten, then obtain
		 * provenance attributes from this rte's subquery.*/
		if (shouldRewriteRTEforMap(GET_COPY_MAP(query), curSubquery))
			addProvAttrFromStack (query, curSubquery, &pList, &subPStack);
		/* is a not rewritten RTE. Create dummy provenance attributes */
		else
		{
			pop(&subPStack);
			//CHECK ok to not add NULLs here?
			addDummyProvenanceAttributesForRTE(query, curSubquery, &pList);
		}
	}

	/* add copy map attributes */
	generateCopyMapAttributs(query, origAttrNum);

	/* return changed pList */
	return pList;
}

/*
 *
 */

static void
addProvAttrFromStack (Query *query, Index rtindex, List **pList,
		List **subPstack)
{
	RangeTblEntry *rte;
	ListCell *lc;
	List *curPlist;
	Index curResno;
	TargetEntry *newTe, *te;
	Expr *expr;

	/* check if it is a base relation with empty copy map */
	rte = rt_fetch(rtindex, query->rtable);
	curPlist = (List *) pop(subPstack);
	curResno = list_length(query->targetList) + 1;

	/* add each element of pSet to targetList and pList */
	foreach (lc, curPlist)
	{
		te = (TargetEntry *) lfirst(lc);

		/* create new TE */
		expr = (Expr *) makeVar (rtindex,
					te->resno,
					exprType ((Node *) te->expr),
					exprTypmod ((Node *) te->expr),
					0);
		newTe = makeTargetEntry(expr, curResno, te->resname, false);

		/* adapt varno und varattno if referenced rte is used in a join-RTE */
		getRTindexForProvTE (query, (Var *) expr);

		/* append to targetList and pList */
		query->targetList = lappend (query->targetList, newTe);
		*pList = lappend (*pList, newTe);

		/* increase current resno */
		curResno++;
	}
}

/*
 * Create Dummy provenance attributes entries (null constants) for a range
 * table entry of a query.
 */

static void
addDummyProvenanceAttributesForRTE (Query *query, Index rtindex, List **pList)
{
	List *entries;
	ListCell *lc;
	CopyMapRelEntry *entry;

	entries = getAllEntriesForRTE(GET_COPY_MAP(query), rtindex);

	foreach(lc, entries)
	{
		entry = (CopyMapRelEntry *) lfirst(lc);

		addDummyProvenanceAttrsForBaseRel (entry, query, pList);
	}
}

/*
 * Add provenance attributes for one base rel copy map entry.
 */

static void
addDummyProvenanceAttrsForBaseRel (CopyMapRelEntry *rel, Query *query,
		List **pList)
{
	ListCell *lc;
	Expr *expr;
	TargetEntry *newTe;
	CopyMapEntry *attr;
	Index curResno;

	curResno = list_length(query->targetList) + 1;

	foreach(lc, rel->attrEntries)
	{
		attr = (CopyMapEntry *) lfirst(lc);

		expr = (Expr *) makeNullConst(attr->baseRelAttr->vartype,
				attr->baseRelAttr->vartypmod);

		newTe = makeTargetEntry(expr, curResno, strdup(attr->provAttrName),
				false);

		query->targetList = lappend(query->targetList, newTe);

		if (pList)
			*pList = lappend(*pList, newTe);

		curResno++;
	}
}
