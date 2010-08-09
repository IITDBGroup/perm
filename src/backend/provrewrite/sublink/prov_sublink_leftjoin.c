/*-------------------------------------------------------------------------
 *
 * prov_sublink_leftjoin.c
 *	  POSTGRES C a sublink rewrite variant that uses leftjoins to "join" rewritten sublink queries to the original query
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/prov_sublink_leftjoin.c,v 1.542 2008/08/05 19:55:08 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "nodes/makefuncs.h"
#include "nodes/parsenodes.h"
#include "nodes/primnodes.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/provlog.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_sublink_leftjoin.h"

/* prototypes */
static Index addLeftJoinWithRewrittenSublink (Query *query, SublinkInfo *info);
static void createJoinCondition (Query *query, SublinkInfo *info, bool isTargetRewrite);
static Node *generateCsub (SublinkInfo *info);
static Node *generateCsubPlus (SublinkInfo *info, Index rtIndex);

/*
 * rewrites a sublink by joining the original query with the rewritten sublink query.
 */

Query *
rewriteSublinkUsingLeftJoin (Query *query, SublinkInfo *info, Index subList[])
{
	Query *rewrittenSublink;
	Index subIndex;

	/* rewrite Sublink query */
	rewrittenSublink = rewriteQueryNode(copyObject(info->sublink->subselect));
	info->rewrittenSublinkQuery = rewrittenSublink;

	/* join query RTEs */
	joinQueryRTEs(query);

	/* add left join for subink */
	subIndex =  addLeftJoinWithRewrittenSublink (query, info);
	subList[info->sublinkPos] = subIndex;

	/* create the join condition for the left join with the rewritten sublink */
	createJoinCondition (query, info, false);

	return query;
}

/*
 *	Rewrites a sublink by joining the original query with the rewritten sublink query. This method is called by the Move strategy and expects that
 *	the sublink to be rewritten is located in the target list of "query". the left join is added to newTop (which has
 */

Query *
rewriteTargetSublinkUsingLeftJoin (Query *newTop, Query *query, SublinkInfo *info, Index subList[])
{
	Query *rewrittenSublink;
	Index subIndex;

	/* here we are sure that the sublink is rewritten using MOVE-strategy there say so */
	LOGNOTICE("use Move");
	addUsedMethod("Move");

	/* rewrite Sublink query */
	rewrittenSublink = rewriteQueryNode(copyObject(info->sublink->subselect));
	info->rewrittenSublinkQuery = rewrittenSublink;

	/* join query RTEs */
	joinQueryRTEs(query);

	/* add left join for subink */
	subIndex = addLeftJoinWithRewrittenSublink (newTop, info);
	subList[info->sublinkPos] = subIndex;

	/* create the join condition for the left join with the rewritten sublink */
	createJoinCondition (newTop, info, true);

	return query;
}



/*
 * join the query with the rewritten sublink query.
 */

static Index
addLeftJoinWithRewrittenSublink (Query *query, SublinkInfo *info)
{
	JoinExpr *joinExpr;
	RangeTblRef *rtRef;
	Index sublinkIndex;

	/* add the rewritten sublink query to the queries range table */
	addSubqueryToRT(query, info->rewrittenSublinkQuery, appendIdToString("rewrittenSublink", &curUniqueRelNum));
	correctRTEAlias((RangeTblEntry *) lfirst(query->rtable->tail));

	sublinkIndex = list_length(query->rtable);

	rtRef = makeNode(RangeTblRef);
	rtRef->rtindex = sublinkIndex;

	/* if original query has a range table entry, join it with the rewriten sublink */
	if (sublinkIndex > 1)
	{
		/* create JoinExpr for left join */
		joinExpr = createJoinExpr(query, JOIN_LEFT);
		joinExpr->larg = (Node *) linitial(query->jointree->fromlist);
		joinExpr->rarg = (Node *)  rtRef;

		query->jointree->fromlist = list_make1(joinExpr);

		/* adapt join RTE for left join */
		adaptRTEsForJoins(list_make1(joinExpr), query, "query_leftjoin_sublink");
	}
	/* original query does not have any range table entry, set rtRef for rewritten sublink as fromlist */
	else
	{
		query->jointree->fromlist = list_make1(rtRef);
	}

	return sublinkIndex;
}

/*
 * Creates the join condition for the left join with the rewritten sublink query
 */

static void
createJoinCondition (Query *query, SublinkInfo *info, bool isTargetRewrite)
{
	JoinExpr *join;
	Node *condition;
	Node *Csub;
	Node *CsubPlus;

	if (info->sublink->subLinkType == ANY_SUBLINK || info->sublink->subLinkType == ALL_SUBLINK)
	{
		/* generate Csub and CsubPlus from sublink condition */
		if (info->targetVar)
		{
			Csub = copyObject(info->targetVar);
		}
		else
		{
			Csub = generateCsub (info);
		}

		CsubPlus = generateCsubPlus (info, list_length(query->rtable) - 1);

		/* create condition */
		if (info->sublink->subLinkType == ANY_SUBLINK)
		{
			/* C_sub' OR NOT C_sub */
			condition = (Node *) makeBoolExpr(NOT_EXPR, list_make1(Csub));
			condition = (Node *) makeBoolExpr(OR_EXPR, list_make2(CsubPlus, condition));
		}
		if (info->sublink->subLinkType == ALL_SUBLINK)
		{
			/* C_sub OR NOT C_sub' */
			condition = (Node *) makeBoolExpr(NOT_EXPR, list_make1(CsubPlus));
			condition = (Node *) makeBoolExpr(OR_EXPR, list_make2(Csub, condition));
		}
	}
	else
	{
		condition = makeBoolConst(true, false);
	}

	if (list_length(query->rtable) > 1)
	{
		join = (JoinExpr *) linitial(query->jointree->fromlist);
		join->quals = condition;
	}
	else
	{
		query->jointree->quals = condition;
	}
}

/*
 * copies a sublink node but increases varlevelsup from the testexpr and in the sublink query
 */

static Node *
generateCsub (SublinkInfo *info)
{
	Node *result;
	//ReplaceParamsContext *context;
	SubLink *sublink;
	//int *increaseSublevelsContext;

	/* copy sublink */
	sublink = copyObject(info->sublink);
	result = (Node *) sublink;

	//CHECK that its ok to not increase sublevelup for left join (should be because we are rewritting uncorrelated sublinks anyway, NO because we might be in another sublink)
//	/* increase varlevelsup in sublink query of Csub */
//	increaseSublevelsContext = (int *) palloc(sizeof(int));
//	*increaseSublevelsContext = -1;
//	sublink->subselect = increaseSublevelsUpMutator(sublink->subselect, increaseSublevelsContext);
//	pfree(increaseSublevelsContext);

//	/* increase varlevelsup in sublink test expression */
//	context = (ReplaceParamsContext *) palloc(sizeof(ReplaceParamsContext));
//	context->addVarSublevelsUp = 1;
//	context->touchParams = false;
//	context->touchAggs = false;
//	context->varSublevelsUp = -1;
//
//	 ((SubLink *) result)->testexpr = replaceParamsMutator (((SubLink *) result)->testexpr, context);
//	 pfree(context);

	return result;
}

/*
 * For a ANY- or ALL-sublink we create a condition that resembles the sublink test expression.
 * For example if the sublink is:
 * 		a = ANY (SELECT b FROM R)
 * the new condition is
 * 		a = b
 */

static Node *
generateCsubPlus (SublinkInfo *info, Index rtIndex)
{
	Node *result;
	ReplaceParamsContext *context;

	context = (ReplaceParamsContext *) palloc(sizeof(ReplaceParamsContext));
	context->paramSublevelsUp = 0;
	context->addVarSublevelsUp = 0;//CHECK thats ok to not add anything to varlevelsup
	context->varSublevelsUp = -1;
	context->touchParams = true;
	context->touchAggs = false;
	context->sublink = info->rewrittenSublinkQuery;
	context->useVarnoValue = rtIndex;

	result = copyObject(info->sublink->testexpr);
	result = replaceParamsMutator (result, context);

	pfree(context);

	return result;
}
