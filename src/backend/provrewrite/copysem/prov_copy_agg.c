/*-------------------------------------------------------------------------
 *
 * prov_copy_agg.c
 *	  PERM C - C-CS provenance rewrite for queries with aggregation.
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/copysem/prov_copy_agg.c,v 1.542 23.06.2009 11:35:58 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "nodes/parsenodes.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/prov_aggr.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_copy_agg.h"
#include "provrewrite/prov_copy_map.h"
#include "provrewrite/prov_copy_util.h"
#include "provrewrite/prov_sublink_agg.h"

/* Function declarations */
static bool setRtindexRelEntryWalker (CopyMapRelEntry *entry, void *context);
static bool mapInVarsAttrWalker (CopyMapRelEntry *entry, CopyMapEntry *attr,
		void *context);
static bool mapOutVarsAttrWalker (CopyMapRelEntry *entry, CopyMapEntry *attr,
		void *context);

static void createWithoutAggCopyMap (Query *rewrite, List *attrMap);
static void createNewTopCopyMap (Query *newTop, List *attrMap);

/*
 * Rewrite an aggregation query using copy contribution semantics (C-CS).
 */

Query *
rewriteCopyAggregateQuery (Query *query) //TODO adapt rewritten non-agg copy map!
{
	List *pList;
	List *subList;
	List *groupByTLEs;
	Query *newRewriteQuery;
	Query *newTopQuery;
	Index rtIndex;
	int *context;
	RangeTblEntry *rte;
	List *attrMap;

	pList = NIL;
	subList = NIL;

	/* Should the query be rewritten at all? If not fake provenance
	 * attributes. */
	if (!shouldRewriteQuery(query))
	{
//		pList = copyAddProvAttrForNonRewritten(query);
//		pStack = lcons(pList, pStack);

		return query;
	}

	/* have to rewrite aggregation query */

	/* get group by attributes */
	groupByTLEs = getGroupByTLEs (query);

	/* copy query node and strip off aggregation and limit-clause and add a
	 * the CopyMap of the aggregation but adapt it. */
	newRewriteQuery = copyObject (query);
	newRewriteQuery->limitCount = NULL;
	newRewriteQuery->limitOffset = NULL;
	SetSublinkRewritten(newRewriteQuery, false);

	/* if aggregation query has no sublinks or only sublinks in HAVING
	 * newRewriteQuery has no sublinks */
	if (!hasNonHavingSublink (query))
		newRewriteQuery->hasSubLinks = false;

	attrMap = rewriteAggrSubqueryForRewrite (newRewriteQuery, true);

	createWithoutAggCopyMap(newRewriteQuery, attrMap);//TODO new adaptation of copy map

	/* create new top query node and adapt copy map of aggregation for this
	 * node */
	newTopQuery = makeQuery();
	Provinfo(newTopQuery)->copyInfo = copyObject(GET_COPY_MAP(query)); //CHECK necessary? do queries above access this?
	//TODO recreate child links
	createNewTopCopyMap(newTopQuery, attrMap);

	/* copy ORDER BY clause */
	checkOrderClause(newTopQuery, query);

	/* add original query to rtable of new top query */
	addSubqueryToRT (newTopQuery, query, "originalAggr");

	/* add result attributes of original query to targetList */
	rtIndex = 1;
	addSubqueryTargetListToTargetList (newTopQuery, rtIndex);

	/* add join on groupby attributes */
	addJoinOnAttributes (newTopQuery, groupByTLEs, newRewriteQuery);

	/* rewrite new subquery without aggregation */
	newRewriteQuery = rewriteQueryNodeCopy (newRewriteQuery);

	addSubqueryToRT (newTopQuery, newRewriteQuery, "rewrittenAggrSubquery");

	/* correct eref for sub query entries */
	correctSubQueryAlias (newTopQuery);

	/* add range table entry for join */
	rte = makeRte(RTE_JOIN);
	rte->jointype = JOIN_LEFT;
	newTopQuery->rtable = lappend(newTopQuery->rtable, rte);

	SetProvRewrite(newRewriteQuery,false);
	SetProvRewrite(query,false);
	adaptRTEsForJoins(list_make1(linitial(newTopQuery->jointree->fromlist)),
			newTopQuery, "joinAggAndRewrite");

	/* add provenance attributes of sub queries to targetlist */
//	pList =
	copyAddProvAttrs (newTopQuery, list_make1_int (2));

	/* push list of provenance attributes to pStack */
//	pStack = lcons(pList, pStack);

	/* correct sublevelsup if we are rewritting a sublink query */
	//OPTIMIZE mark sublink queries so we don't have to run increaseSublevelsUpMutator for non sublink queries
	context = (int *) palloc(sizeof(int));

	*context = -1;
	increaseSublevelsUpMutator((Node *) newRewriteQuery, context);
	increaseSublevelsUpMutator((Node *) query, context);

	pfree(context);

	/* push list of provenance attributes to pStack */
//	pStack = lcons(pList, pStack);

	return newTopQuery;
}


/*
 *
 */

static void
createNewTopCopyMap (Query *newTop, List *attrMap)
{
	CopyMap *map;

	map = GET_COPY_MAP(newTop);
	copyMapWalker(map->entries, NULL, attrMap, NULL, setRtindexRelEntryWalker,
			mapInVarsAttrWalker, NULL);
}

/*
 *
 */

static void
createWithoutAggCopyMap (Query *rewrite, List *attrMap)
{
	CopyMap *map;

	map = GET_COPY_MAP(rewrite);
	copyMapWalker(map->entries, NULL, attrMap, NULL, NULL,
			mapOutVarsAttrWalker, NULL);
}

/*
 *
 */

static bool
setRtindexRelEntryWalker (CopyMapRelEntry *entry, void *context)
{
	entry->rtindex = 2;

	return false;
}

/*
 *
 */

static bool
mapInVarsAttrWalker (CopyMapRelEntry *entry, CopyMapEntry *attr, void *context)
{
//	ListCell *lc;
//	Var *var;

//	foreach(lc, attr->inVars)
//	{
//		var = (Var *) lfirst(lc);
//
//		var->varno = 2;
//		var->varattno = listPositionInt((List *) context, var->varattno) + 1;
//	}

	return false;
}

/*
 *
 */

static bool
mapOutVarsAttrWalker (CopyMapRelEntry *entry, CopyMapEntry *attr, void *context)
{
//	ListCell *lc;
//	Var *var;
//
//	foreach(lc, attr->outVars)
//	{
//		var = (Var *) lfirst(lc);
//
//		var->varattno = listPositionInt((List *) context, var->varattno) + 1;
//	}

	return false;
}
