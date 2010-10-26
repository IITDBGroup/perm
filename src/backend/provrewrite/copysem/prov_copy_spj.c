/*-------------------------------------------------------------------------
 *
 * prov_copy_spj.c
 *	  PERM C - Backend provenance extension
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/copysem/prov_copy_spj.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "nodes/makefuncs.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_spj.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/provlog.h"
#include "provrewrite/provattrname.h"
#include "provrewrite/prov_copy_spj.h"
#include "provrewrite/prov_copy_util.h"
#include "provrewrite/prov_copy_map.h"
#include "provrewrite/provstack.h"

static void rewriteCopyBaseRel (RangeTblEntry *rte, Index rtindex, CopyMap *map);
static bool rteShouldRewrite (Query *query, Index rtindex);
static void rewriteCopyDistinctClause (Query *query);

/*
 * Rewrite a SPJ query node using copy contribution semantics (C-CS).
 */

Query *
rewriteSPJQueryCopy (Query *query)
{
	List *subList;

	subList = NIL;

	/* if the query has a limit clause we have to preserve the original
	 * query and join it with the rewritten query without limit. This is
	 * necessary because we don't know beforehand how many duplicates of
	 * normal result tuples we will need to "fit" the provenance in.
	 */
	if (checkLimit(query))
		return handleLimit(query);

	/* rewrite RTEs */
	rewriteRTEsCopy (query, &subList, list_length(query->rtable));

	/* correct eref of subqueries */
	correctSubQueryAlias (query);

	/* add provenance attributes of sub queries to targetlist */
	copyAddProvAttrs (query, subList);

	/* if a distinct clause is present include provenance attributes
	 * otherwise we would incorrectly omit duplicate result tuples with
	 * different provenance
	 */

	if (query->distinctClause != NIL)
		rewriteCopyDistinctClause (query);

	return query;
}

/*
 * Rewrite the range table entries of a query for C-CS.
 */

void
rewriteRTEsCopy (Query *query, List **subList, Index maxRtindex)
{
	ListCell *lc;
	RangeTblEntry *rte;
	int i, rtindex;

	/* if none of the RTEs should be rewritten, return to caller */
	if (maxRtindex == 0)
		return;

	rtindex = 1;

	/* Walk through range table and rewrite rte's if some new rte's were added
	 * during sublink rewrite we ignore them. */

	for(lc = query->rtable->head, i = 0; lc != NULL && i < maxRtindex;
			lc = lc->next, i++)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		/* check if rte should be rewritten at all */
		if (strcmp("*NEW*", rte->eref->aliasname) != 0
						&& strcmp("*OLD*", rte->eref->aliasname) != 0
						&& !ignoreRTE(rte)
						&& rteShouldRewrite(query, rtindex))
		{
			/* rte is a stored provenance query */
			if (rte->provAttrs != NIL)
			//TODO	rewriteRTEwithProvenance (rtindex, rte, query);
				;
			/* rte is subquery */
			else if (rte->rtekind == RTE_SUBQUERY)
				rte->subquery = rewriteQueryNodeCopy (rte->subquery);
			/* rte is base relation */
			else if (rte->rtekind == RTE_RELATION || rte->isProvBase)
				rewriteCopyBaseRel (rte, rtindex, GET_COPY_MAP(query));
		}

		/* rte is not a join RTE so add it to subList*/
		if (rte->rtekind != RTE_JOIN)
			*subList = lappend_int(*subList, rtindex);

		rtindex++;
	}
}

/*
 * Checks if a RTE should be rewritten.
 */
static bool
rteShouldRewrite (Query *query, Index rtindex)
{
	CopyMap *map;

	map = GET_COPY_MAP(query);

	if(shouldRewriteRTEforMap(map, rtindex))
		return true;

	return false;
}

/*
 * Rewrite a base relation for C-CS. Creates a CopyMapRelEntry for this base
 * relation and creates the provenance attributes and stores them in the
 * CopyMapRelEntry.
 */

static void
rewriteCopyBaseRel (RangeTblEntry *rte, Index rtindex, CopyMap *map)
{
	ListCell *lc;
	TargetEntry *te;
	Expr *expr;
	char *namestr;
	AttrNumber curResno;
	CopyMapRelEntry *entry;
	CopyMapEntry *attr;

	/* get copy map entry for base relation */
	entry = getEntryForBaseRel(map, rtindex);

	/* create target entries for the provenance attributes */
	foreachi(lc, curResno, entry->attrEntries)
	{
		attr = (CopyMapEntry *) lfirst(lc);

		expr = (Expr *) attr->baseRelAttr;
		namestr = attr->provAttrName;

		te = makeTargetEntry(expr, curResno + 1, namestr, false);

		te->resorigtbl = rte->relid;
		te->resorigcol = attr->baseRelAttr->varattno;

		entry->provAttrs = lappend(entry->provAttrs, te);
	}

	/* if the baseRelStack is activated push a RTE for this base relation on
	 * the baseRelStack */
	if (baseRelStackActive)
		push(&baseRelStack, copyObject(rte));
}

/*
 * Adapts the DISTINCT clause of a rewritten C-CS SPJ query by adding the
 * provenance attributes to the DISTINCT clause.
 */

static void
rewriteCopyDistinctClause (Query *query)
{
	CopyMap *map = GET_COPY_MAP(query);
	List *pList = NIL;
	ListCell *lc;
	CopyMapRelEntry *rel;

	foreach(lc, map->entries)
	{
		rel = (CopyMapRelEntry *) lfirst(lc);

		pList = list_concat(pList, rel->provAttrs);
	}

	push(&pStack, pList);
	rewriteDistinctClause (query);
	pop(&pStack);
}
