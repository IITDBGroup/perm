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

#include "provrewrite/prov_util.h"
#include "provrewrite/provlog.h"
#include "provrewrite/provattrname.h"
#include "provrewrite/prov_copy_util.h"
#include "provrewrite/prov_copy_map.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/provstack.h"


static void addProvAttrFromStack (Query *query, Index rtindex, List **pList,
		List **subPstack);
static void addDummyProvenanceAttributesForRTE (Query *query, Index rtindex,
		List **pList);
static void addDummyProvenanceAttrsForBaseRel (CopyMapRelEntry *rel,
		Query *query, List **pList);

/*
 *
 */

List *
copyAddProvAttrsForSet (Query *query, List *subList, List *pList)
{
	CopyMap *map;
	CopyMapRelEntry *rel;
	ListCell *lc;
	List *curPstack;
	Index rtIndex;

	map = GetInfoCopyMap(query);
	curPstack = popListAndReverse (&pStack, list_length(subList));

	foreach(lc, map->entries)
	{
		rel = (CopyMapRelEntry *) lfirst(lc);

		/* is propagating? then we have to have an rte for it */
		if (rel->propagate)
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

	foreach(lc, GetInfoCopyMap(query)->entries)
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

	subPStack = popListAndReverse (&pStack, list_length(subList));

	/* for each subquery of query ... */
	foreach (subqLc, subList)
	{
		curSubquery = (Index) lfirst_int(subqLc);

		/* if current rte contains parts that are rewritten, then obtain
		 * provenance attributes from this rte's subquery.*/
		if (shouldRewriteRTEforMap(GetInfoCopyMap(query), curSubquery))
			addProvAttrFromStack (query, curSubquery, &pList, &subPStack);
		/* is a not rewritten RTE. Create dummy provenance attributes */
		else
		{
			pop(&subPStack);
			addDummyProvenanceAttributesForRTE(query, curSubquery, &pList);
		}
	}

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

	entries = getAllEntriesForRTE(GetInfoCopyMap(query), rtindex);

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
