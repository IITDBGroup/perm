/*-------------------------------------------------------------------------
 *
 * prov_where_set.c
 *	  PERM C 
 *
 * Portions Copyright (c) 2010 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $Perm: pgsql//SF_Perm/src/backend/provrewrite/wherecs/prov_where_set.c,v 1.542 Oct 4, 2010 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "nodes/parsenodes.h"

#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_where_spj.h"
#include "provrewrite/prov_where_set.h"

/* prototypes */
static Query *generateRepresentative (Query *query);
static List *generateAuxQueries(List *reps);

/*
 *
 */

List *
rewriteSetWhere (Query *query)
{
	List *result = NIL;
	ListCell *lc;
	Query *curQuery;
	RangeTblEntry *rte;
	WhereProvInfo *provInfo;

	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		curQuery = rte->subquery;
		provInfo = (WhereProvInfo *) makeNode(WhereProvInfo);
		provInfo->attrInfos = NIL;
		Provinfo(curQuery)->copyInfo = (Node *) provInfo;

		curQuery = rewriteWhereSPJQuery(curQuery);

		result = lappend(result, curQuery);
	}

	return result;
}

/*
 *
 */

Query *
rewriteWhereInSetQuery (Query *query)
{
	List *subqueries = NIL;
	List *representatives = NIL;
	List *auxQueries = NIL;
	ListCell *lc;
	Query *sub;
	Query *represen;

	RangeTblEntry *rte;

	/* gather subqueries */
	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		if (rte->rtekind == RTE_RELATION)
			subqueries = lappend(subqueries, generateQueryFromBaseRelation(rte));
		else if (rte->rtekind == RTE_SUBQUERY)
			subqueries = lappend(subqueries, rte->subquery);
		else
			;//TODO ERROR
	}

	/* generate query representative for each subquery */
	foreach(lc, subqueries)
	{
		sub = (Query *) lfirst(lc);
		represen = generateRepresentative (sub);

		representatives = lappend(representatives, represen);
	}

	/* generate auxiliary queries for each representative */
	auxQueries = generateAuxQueries(representatives);

	/* generate union between all auxiliary queries */
	return query;
	//TODO return unionAuxQueries (query, auxQueries);
}


static Query *
generateRepresentative (Query *query)
{

}

static List *
generateAuxQueries(List *reps)
{

}
