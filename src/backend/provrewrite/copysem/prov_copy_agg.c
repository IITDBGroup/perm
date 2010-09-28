/*-------------------------------------------------------------------------
 *
 * prov_copy_agg.c
 *	  PERM C - C-CS provenance rewrites for queries with aggregation.
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

/* Prototypes */
static void createWithoutAggCopyMap (Query *rewrite, List *attrMap,
		CopyMap *origMap);

/*
 * Rewrite an aggregation query using copy contribution semantics (C-CS).
 */

Query *
rewriteCopyAggregateQuery (Query *query)
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
	CopyMap *origMap;

	pList = NIL;
	subList = NIL;

	/* Should the query be rewritten at all? If not fake provenance
	 * attributes. */
	if (!shouldRewriteQuery(query))
		return query;

	/* copy query node and strip off aggregation and limit-clause and add a
	 * the CopyMap of the aggregation but adapt it. */
	origMap = GET_COPY_MAP(query);
	newRewriteQuery = query;
	query = copyObject(query);
	Provinfo(query)->copyInfo = NULL;
	newRewriteQuery->limitCount = NULL;
	newRewriteQuery->limitOffset = NULL;
	SetSublinkRewritten(newRewriteQuery, false);

	/* get group by attributes */
	groupByTLEs = getGroupByTLEs (query);

	/* if aggregation query has no sublinks or only sublinks in HAVING
	 * newRewriteQuery has no sublinks */
	if (!hasNonHavingSublink (query))
		newRewriteQuery->hasSubLinks = false;

	attrMap = rewriteAggrSubqueryForRewrite (newRewriteQuery, true);

	createWithoutAggCopyMap(newRewriteQuery, attrMap, origMap);

	/* create new top query node and adapt copy map of aggregation for this
	 * node */
	newTopQuery = makeQuery();
	Provinfo(newTopQuery)->copyInfo = (Node *) origMap;
//	createNewTopCopyMap(newTopQuery, attrMap);

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
createWithoutAggCopyMap (Query *rewrite, List *attrMap, CopyMap *origMap)
{
	CopyMap *map;
	ListCell *lc, *attLc, *oldAttLc, *attInclLc;
	CopyMapRelEntry *rel, *newRel;
	CopyMapEntry *attr, *oldAtt;
	AttrInclusions *attIncl, *newAttrIncl, *innerAIncl;
	InclusionCond *newCond;
	Var *aggSubVar, *topVar;

	map = makeCopyMap();
	map->rtindex = 2;

	/* The new CopyMap is a layer between the origMap and the child of origMap.
	 * Create the same CopyMapRelEntries as found in origMap and copy its basic
	 * properties. */
	foreach(lc, origMap->entries)
	{
		rel = (CopyMapRelEntry *) lfirst(lc);
		COPY_BASIC_COPYREL(newRel, rel);
		newRel->isStatic = rel->isStatic;
		newRel->noRewrite = rel->noRewrite;
		newRel->rtindex = 2;
		newRel->child = rel->child;
		rel->child = newRel;

		/* For each new attr entry add adapted versions of the original
		 * AttrInclusions and for the original attr entry replace the
		 * AttrInclusions with simple copying.*/
		forboth(attLc, newRel->attrEntries, oldAttLc, rel->attrEntries)
		{
			attr = (CopyMapEntry *) lfirst(attLc);
			oldAtt = (CopyMapEntry *) lfirst(oldAttLc);

			attr->outAttrIncls = copyObject(oldAtt->outAttrIncls);
			oldAtt->outAttrIncls = NULL;

			/* For each original AttrInclusions adapt the outVar and
			 * create a new AttrInclusions for the origMap. */
			foreach(attInclLc, attr->outAttrIncls)
			{
				attIncl = (AttrInclusions *) lfirst(attInclLc);

				// adapt the outVar varattno
				topVar = copyObject(attIncl->attr);
				attIncl->attr->varattno =
						listPositionInt(attrMap, topVar->varattno) + 1;

				/* create simple exists inclusion for original used in new top
				 * query */
				aggSubVar = copyObject(attIncl->attr);
				aggSubVar->varno = 2;
				MAKE_EXISTS_INCL(newCond, aggSubVar);
				innerAIncl = makeAttrInclusions();
				innerAIncl->attr = copyObject(aggSubVar);
				innerAIncl->inclConds = list_make1(newCond);
				MAKE_EXISTS_INCL(newCond, innerAIncl);


				newAttrIncl = makeAttrInclusions();
				newAttrIncl->attr = topVar;
				newAttrIncl->isStatic = attIncl->isStatic;
				newAttrIncl->inclConds = list_make1(newCond);

				oldAtt->outAttrIncls = lappend(oldAtt->outAttrIncls, newAttrIncl);
			}
		}

		map->entries = lappend(map->entries, newRel);
	}

	SET_COPY_MAP(rewrite, map);
}

