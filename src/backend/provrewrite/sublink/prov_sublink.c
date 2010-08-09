/*-------------------------------------------------------------------------
 *
 * prov_sublink.c
 *	  POSTGRES C sublink handling for provenance module
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/prov_sublink.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "nodes/makefuncs.h"
#include "nodes/nodes.h"
#include "nodes/primnodes.h"
#include "nodes/parsenodes.h"
#include "nodes/print.h"				// pretty print node (trees)
#include "optimizer/clauses.h"
#include "parser/parse_relation.h"
#include "parser/parse_expr.h"
#include "parser/parsetree.h"
#include "utils/guc.h"
#include "provrewrite/prov_sublink.h"
#include "provrewrite/prov_sublink_totarget.h"
#include "provrewrite/prov_sublink_keepcor.h"
#include "provrewrite/prov_sublink_leftjoin.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/provstack.h"
#include "provrewrite/provlog.h"
#include "provrewrite/prov_sublink_unnest.h"
#include "provrewrite/prov_sublink_unn.h"

#define MAX_SUBLINK 100

/* prototypes */
static Query *unnestAndDecorrelate (Query *query, Index subList[], List *infos, List **rewritePos);
static void setSublinkPositions (List *infos);

/*
 * rewrites a query with sublinks
 */

Query *
rewriteSublinks (Query *query, List **subList)
{
	List *sublinkInfos;
	List *uncorrSublinks;
	ListCell *lc;
	SublinkInfo *info;
	Index subPos[MAX_SUBLINK];
	List *rewritePos;
	int numSublinks;

	rewritePos = NIL;

	/* find sublinks in all possible clause expressions */
	sublinkInfos = findSublinkLocations (query, PROV_SUBLINK_SEARCH_SPJ);

	numSublinks = list_length(sublinkInfos);
	setSublinkPositions (sublinkInfos);

	/* try to unnest and decorrelate sublinks */
	unnestAndDecorrelate (query, subPos, sublinkInfos, &rewritePos);

	/*
	 * if we are processing an uncorrelated sublinks and the move to target list optimization is
	 * activated, then move these sublinks to the target list of the query. This is done by adding
	 * a new top query node that simulates the WHERE clause of the query (HAVING and GROUP BY sublinks
	 * have been eliminated by prov_sublink_agg). Later on we have to use a different form of the
	 * left join rewrite method that is aware of the changed query structure.
	 */
	if (prov_use_sublink_move_to_target)
	{
		uncorrSublinks = findSublinkByCats(sublinkInfos, PROV_SUBLINK_SEARCH_UNCORR);
		uncorrSublinks = findSublinksUnnested (uncorrSublinks, PROV_SUBLINK_SEARCH_NOUNNEST);

		/* if there are uncorrelated sublinks that have not been unnested use MOVE strategy */
		if (uncorrSublinks != NIL)
			return rewriteSublinkQueryWithMoveToTarget(query, sublinkInfos, uncorrSublinks, subPos, &rewritePos);
	}


	/* remove sublinks that have been unnested */
	sublinkInfos = findSublinksUnnested (sublinkInfos, PROV_SUBLINK_SEARCH_NOUNNEST);

	/* rewrite each sublink */
	foreach(lc, sublinkInfos)
	{
		info = (SublinkInfo *) lfirst(lc);
		rewriteSublink(query, info, subPos, &rewritePos);
	}

	/* create sub list */
	createSubList (subPos, subList, rewritePos);

	/* return modified query */
	return query;
}


/*
 *
 */

void
rewriteSublink (Query *query, SublinkInfo *info, Index subList[], List **rewritePos)
{
	/*
	 * if the left join rewrite method is activated and the sublink is uncorrelated
	 * then use this method to rewrite the sublink
	 */
	if (prov_use_sublink_optimization_left_join && info->category == SUBCAT_UNCORRELATED)
	{
		LOGNOTICE("use Left");
		addUsedMethod("Left");
		rewriteSublinkUsingLeftJoin (query, info, subList);
		*rewritePos = lappend_int(*rewritePos, info->sublinkPos);
	}

	/* use cross product simulated left join method */
	else
	{
		LOGNOTICE("use Gen");
		addUsedMethod("Gen");
		rewriteSublinkWithCorrelationToBase (query, info, subList);
		*rewritePos = lappend_int(*rewritePos, info->sublinkPos);
	}

}

/*
 *
 */

static void
setSublinkPositions (List *infos)
{
	ListCell *lc;
	SublinkInfo *info;
	int i;

	i = 0;
	foreach(lc, infos)
	{
		info = (SublinkInfo *) lfirst(lc);
		info->sublinkPos = i;
		i++;
	}
}

/*
 *
 */

void
createSubList (Index subPos[], List **subList, List *rewritePos)
{
	int i;
	int sublinkPos;
	int numSubs;
	List *subStack;
	List *curPlist;

	numSubs = list_length(rewritePos);
	subStack = popListAndReverse(&pStack, numSubs);

	/* re-order pStack items */
	for(i = 0; i < numSubs; i++)
	{
		sublinkPos = listPositionInt(rewritePos,i);
		curPlist = (List *) list_nth(subStack, sublinkPos);
		push(&pStack, curPlist);
	}

	/* create subList */
	for(i = 0; i < numSubs; i++)
	{
		*subList = lappend_int(*subList, subPos[i]);
	}
}

/*
 * Tries to unnest and/or decorrelate sublink queries.
 */

static Query *
unnestAndDecorrelate (Query *query, Index subList[], List *infos, List **rewritePos)
{
	ListCell *lc;
	SublinkInfo *info;

	foreach(lc, infos)
	{
		info = (SublinkInfo *) lfirst(lc);

		/*
		 * If the sublinks is a correlated sublink that fullfills the following conditions:
		 * 		-The correlation predicate is equality and is used in an AND-tree
		 * 		-The
		 */
		if (prov_use_unnest_JA)
		{
			if (checkJAstrategyPreconditions (info))
			{
				LOGNOTICE("use JA strategy");
				addUsedMethod("JA");

				query = rewriteJAstrategy (query, info, subList);
				*rewritePos = lappend_int(*rewritePos, info->sublinkPos);
			}
			else if (checkEXISTSstrategyPreconditions (info))
			{
				LOGNOTICE("use EXISTS strategy");
				addUsedMethod("EXISTS");

				query = rewriteEXISTSstrategy (query, info, subList);
				*rewritePos = lappend_int(*rewritePos, info->sublinkPos);
			}
		}


		/* If the prov_use_sublink_transfrom_top_level_any_to_join option is activated and
		 * there is only a single top level sublink that is uncorrelated to, this sublink is
		 * transformed into a join.
		 */
		if (prov_use_sublink_transfrom_top_level_any_to_join && !info->unnested)
		{
			if (checkUnnStrategyPreconditions (info, query))
			{
				LOGNOTICE("use Unn strategy");
				addUsedMethod("Unn");

				query =  rewriteUnnStrategy (query, info, subList, infos);
				*rewritePos = lappend_int(*rewritePos, info->sublinkPos);
			}
			else if (checkUnnNotStrategyPreconditions(info, query))
			{
				LOGNOTICE("use Unn-Not strategy");
				addUsedMethod("Unn-NOT");

				query =  rewriteUnnNotStrategy (query, info, subList, infos);
				*rewritePos = lappend_int(*rewritePos, info->sublinkPos);
			}
		}
	}

	return query;
}





