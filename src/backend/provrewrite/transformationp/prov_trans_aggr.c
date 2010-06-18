/*-------------------------------------------------------------------------
 *
 * prov_trans_aggr.c
 *	  POSTGRES C -
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/transformationp/prov_trans_aggr.c,v 1.542 03.09.2009 09:47:58 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "nodes/makefuncs.h"

#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_aggr.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_trans_aggr.h"
#include "provrewrite/prov_trans_main.h"
#include "provrewrite/prov_trans_util.h"

/*
 *
 */

Query *
rewriteTransAgg (Query *query, Node **parentPointer)
{
	Query *newRewriteQuery;
	Query *newTopQuery;
	TransProvInfo *topInfo;
	TransProvInfo *subInfo;
	TransSubInfo *aggInfo;
	List *groupByTles;
	Index rtIndex;
	RangeTblEntry *rte;
	TargetEntry *newTarget;

	groupByTles = getGroupByTLEs(query);

	/* generate new query node with aggregation stripped of */
	newRewriteQuery = copyObject(query);
	newRewriteQuery->limitCount = NULL;
	newRewriteQuery->limitOffset = NULL;

	rewriteAggrSubqueryForRewrite (newRewriteQuery, false);

	/* generate new top query node that joins original aggregation with rewritten query without aggregtion */
	newTopQuery = makeQuery ();

	/* strip of the aggregation TransSubInfo node from the subInfo */
	topInfo = SET_TRANS_INFO(newTopQuery);
	subInfo = GET_TRANS_INFO(newRewriteQuery);

	aggInfo = (TransSubInfo *) subInfo->root;
	topInfo->root = (Node *) aggInfo;
	subInfo->root = linitial(((TransSubInfo *) subInfo->root)->children);
	aggInfo->children = list_make1(subInfo);
	topInfo->isStatic = subInfo->isStatic;
	topInfo->rtIndex = subInfo->rtIndex;
	subInfo->rtIndex = 2;

	/* copy ORDER BY clause */
	checkOrderClause(newTopQuery, query);

	/* add original query to rtable of new top query */
	addSubqueryToRT (newTopQuery, query, appendIdToString("originalAggr", &curUniqueRelNum));

	/* add result attributes of original query to targetList */
	rtIndex = 1;
	addSubqueryTargetListToTargetList (newTopQuery, rtIndex);

	/* add join on groupby attributes */
	addJoinOnAttributes (newTopQuery, groupByTles, newRewriteQuery);

	/* rewrite new subquery without aggregation */
	newRewriteQuery = rewriteQueryNodeTrans (newRewriteQuery, NULL, (Node **) &(list_head(aggInfo->children)->data.ptr_value));

	addSubqueryToRT (newTopQuery, newRewriteQuery, appendIdToString("rewrittenAggrSubquery", &curUniqueRelNum));

	/* correct eref for sub query entries */
	correctSubQueryAlias (newTopQuery);

	/* add range table entry for join */
	rte = makeRte(RTE_JOIN);
	rte->jointype = JOIN_LEFT;
	newTopQuery->rtable = lappend(newTopQuery->rtable, rte);

	SetProvRewrite(newRewriteQuery,false);
	SetProvRewrite(query,false);
	adaptRTEsForJoins(list_make1(linitial(newTopQuery->jointree->fromlist)), newTopQuery, "joinAggAndRewrite");

	/* add trans prov attr */
	newTarget = MAKE_TRANS_PROV_ATTR(newTopQuery, (Node *) getSubqueryTransProvUnionSet(newTopQuery, 2, aggInfo->setForNode));
	newTopQuery->targetList = lappend(newTopQuery->targetList, newTarget);
	topInfo->transProvAttrNum = list_length(newTopQuery->targetList);

	if (parentPointer)
		*parentPointer = (Node *) topInfo;

	return newTopQuery;
}
