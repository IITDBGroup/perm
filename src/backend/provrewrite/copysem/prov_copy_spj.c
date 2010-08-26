/*-------------------------------------------------------------------------
 *
 * provrewrite.c
 *	  POSTGRES C Backend provenance extension
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

/*
 * Rewrite a SPJ query node using copy contribution semantics (C-CS).
 */

Query *
rewriteSPJQueryCopy (Query *query)
{
	List *pList;
	List *subList;

	pList = NIL;
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

	/* correct join RTEs (add provenance attributes) */
	correctJoinRTEs (query, &subList);

	/* add provenance attributes of sub queries to targetlist */
	pList = copyAddProvAttrs (query, subList, pList);

	/* push list of provenance attributes to pStack */
	push(&pStack, pList);

	/* if a distinct clause is present include provenance attributes
	 * otherwise we would incorrectly omit duplicate result tuples with
	 * different provenance
	 */

	if (query->distinctClause != NIL)
		rewriteDistinctClause (query);

	return query;
}

/*
 *
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

	/* Walk through range table and rewrite rte's if some new rte's were added during
	 * sublink rewrite we ignore them.
	 */

	for(lc = query->rtable->head, i = 0; lc != NULL && i < maxRtindex; lc = lc->next, i++)
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
			//	rewriteRTEwithProvenance (rtindex, rte, query);
				;
			/* rte is base relation */
			else if (rte->rtekind == RTE_RELATION || rte->isProvBase)
				rewriteCopyBaseRel (rte, rtindex, GET_COPY_MAP(query));

			/* rte is subquery */
			else if (rte->rtekind == RTE_SUBQUERY)
				rte->subquery = rewriteQueryNodeCopy (rte->subquery);

		}
		/* add empty provenace attr list to stack */
		else if (rte->rtekind != RTE_JOIN)
			push(&pStack, NIL);

		/* rte is not a join RTE so add it to subList*/
		if (rte->rtekind != RTE_JOIN)
			*subList = lappend_int(*subList, rtindex);

		rtindex++;
	}
}

/*
 *
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
 *
 */

static void
rewriteCopyBaseRel (RangeTblEntry *rte, Index rtindex, CopyMap *map)
{
	ListCell *lc;
	List *pList;
	TargetEntry *te;
	Expr *expr;
	char *namestr;
	AttrNumber curResno;
	CopyMapRelEntry *entry;
	CopyMapEntry *attr;

	pList = NIL;

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

		pList = lappend(pList, te);
	}

	/* push the provenance attrs on pStack */
	push(&pStack, pList);

	/* if the baseRelStack is activated push a RTE for this base relation on the baseRelStack */
	if (baseRelStackActive)
		push(&baseRelStack, copyObject(rte));
}
