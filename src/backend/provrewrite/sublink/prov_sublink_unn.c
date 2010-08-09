/*-------------------------------------------------------------------------
 *
 * prov_sublink_unn.c
 *	  PERM C - Unnests sublinks for provenance rewrite. The following rewrite
 *	  strategies are implemented in this file:
 *	  		1) Unn
 *	  		2) Unn-Not
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/prov_sublink_unn.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
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
#include "parser/parse_expr.h"
#include "parser/parsetree.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/provlog.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_sublink_unn.h"

/* prototypes */
static bool inOuterJoin (SublinkInfo *info, Query *query);
static Node *generateCsubPlus (SublinkInfo *info, Index rtIndex, bool forNot);
static void replaceSublinkWithCsub (SublinkInfo *info, Query *query,
									List *infos, RangeTblRef *rtRef);
static List *findSublinksWithSameExprRoot (List *infos, SublinkInfo *info);

/*
 * Checks if a sublink fullfills the conditions for the
 * rewriteSingelAnyTopLevel rewrite method. These are:
 * 		1) Sublink is uncorrelated
 * 		2) Sublink is in WHERE in an AND-tree
 * 		3) Sublink is an ANY, EXISTS or scalar Sublink
 * 		4) The Sublink is not used in an OUTER JOIN qual
 * 		5) Sublink does not contain sublinks that have to be rewritten using GEN-strategy
 * 			(the correlations used there wouldn't work in a FROM-clause item)
 *		6) Sublink does not have sublinks in its test expression or is used in
 *		   the test expression of another sublink
 *
 */

bool
checkUnnStrategyPreconditions (SublinkInfo *info, Query *query)
{
	bool result;

	result = info->category == SUBCAT_UNCORRELATED
			|| (getSublinkTypeAfterChildRewrite(info) == SUBCAT_UNCORRELATED);
	result = result && info->location == SUBLOC_WHERE;
	result = result && (info->sublink->subLinkType == ANY_SUBLINK
			|| info->sublink->subLinkType == EXISTS_SUBLINK
			|| info->sublink->subLinkType == EXPR_SUBLINK);
	result = result && SublinkInAndOrTop (info);
	result = result && !inOuterJoin (info, query);
	result = result && !containsGenCorrSublink(info);
	result = result && !sublinkHasSublinksInTestOrInTest(info);

	return result;
}

/*
 *	Rewrites a single uncorrelated ANY sublink in an AND-tree by transforming the sublink into a join.
 */

Query *
rewriteUnnStrategy (Query *query, SublinkInfo *info,  Index subList[], List *infos)
{
	Query *sublinkQuery;
	Index sublinkIndex;
	RangeTblRef *rtRef;

	info->unnested = true;

	/* create range table entry for the sublink query */
	sublinkQuery = (Query *) copyObject(info->sublink->subselect);
	info->rewrittenSublinkQuery = rewriteQueryNode(sublinkQuery);
	addSubqueryToRT(query, info->rewrittenSublinkQuery, appendIdToString("rewrittenSublink",&curUniqueRelNum));
	correctRTEAlias((RangeTblEntry *) lfirst(query->rtable->tail));
	IGNORE_RTE_INDEX (query, list_length(query->rtable));

	sublinkIndex = list_length(query->rtable);
	subList[info->sublinkPos] = sublinkIndex;

	/* add RangeTblRef for new RTE to jointree */
	rtRef = makeNode(RangeTblRef);
	rtRef->rtindex = sublinkIndex;

	replaceSublinkWithCsub(info, query, infos, rtRef);

	return query;
}

/*
 * Checks the if a NOT / ANY or ALL sublink fulfills the preconditions to be unnested using the
 * rewriteUncorrNotAnyOrAll rewrite method. These are:
 * 		1) Sublink has no correlations
 * 		2) Sublink is used in WHERE clause
 * 		3) is an ANY or ALL(//TODO) sublink
 * 		4) Sublink is not used in an OUTER JOIN qual
 * 		5) Sublink does not contain sublinks that have to be rewritten using GEN-strategy (the correlations used
 *			there wouldn't work in a FROM-clause item)
 */
bool
checkUnnNotStrategyPreconditions (SublinkInfo *info, Query *query)
{
	bool result;

	result = info->category == SUBCAT_UNCORRELATED;
	result = result && info->location == SUBLOC_WHERE;
	result = result && (info->sublink->subLinkType == ANY_SUBLINK); //TODO support for ALL
	result = result && SublinkInNegAndOrTop (info);//TODO check that testexpr is simple
	result = result && !inOuterJoin (info, query);
	result = result && !containsGenCorrSublink(info);

	return result;
}

/*
 *
 */

Query *
rewriteUnnNotStrategy (Query *query, SublinkInfo *info, Index subList[], List *infos)
{
	Index sublinkIndex;
	Index subIndex;
	RangeTblRef *rtRef;
	NullTest *nullTest;
	TargetEntry *te;
	JoinExpr *newJoin;
	Query *adaptedSublinkQuery;

	info->unnested = true;
	info->rewrittenSublinkQuery = (Query *) copyObject(info->sublink->subselect);

	/* add left join between query and sublink query */
	joinQueryRTEs (query);

	adaptedSublinkQuery = copyObject(info->sublink->subselect);
	addDummyAttr(adaptedSublinkQuery);

	addSubqueryToRT(query, adaptedSublinkQuery,
					appendIdToString("sublinkForNotAny", &curUniqueRelNum));
	correctRTEAlias((RangeTblEntry *) lfirst(query->rtable->tail));

	MAKE_RTREF(rtRef, list_length(query->rtable));
	subIndex = rtRef->rtindex;

	newJoin = createJoinExpr(query, JOIN_LEFT);
	newJoin->larg = (Node *) linitial(query->jointree->fromlist);
	newJoin->rarg = (Node *) copyObject(rtRef);
	newJoin->quals = generateCsubPlus(info, rtRef->rtindex, true);

	query->jointree->fromlist = list_make1(newJoin);//CHECK ok to add as top level join

	/* create range table entry for the rewritten sublink query in this case static null values */
	info->rewrittenSublinkQuery = rewriteQueryNode(info->rewrittenSublinkQuery);

	addSubqueryToRT(query, info->rewrittenSublinkQuery,
					appendIdToString("rewrittenSublink", &curUniqueRelNum));
	correctRTEAlias((RangeTblEntry *) lfirst(query->rtable->tail));

	sublinkIndex = list_length(query->rtable);
	IGNORE_RTE_INDEX (query, sublinkIndex);
	subList[info->sublinkPos] = sublinkIndex;

	/* add RangeTblRef for new RTE to jointree */
	MAKE_RTREF(rtRef, sublinkIndex);

	newJoin = createJoinExpr(query, JOIN_LEFT);
	newJoin->larg = (Node *) linitial(query->jointree->fromlist);
	newJoin->rarg = (Node *) rtRef;
	newJoin->quals = makeBoolConst(true, false);

	query->jointree->fromlist = list_make1(newJoin);

	recreateJoinRTEs(query);

	/* replace sublink with Csub+ */
	te = (TargetEntry *) llast(adaptedSublinkQuery->targetList);

	nullTest = makeNode(NullTest);
	nullTest->nulltesttype = IS_NULL;
	nullTest->arg = (Expr *) makeVar(subIndex,
									list_length(adaptedSublinkQuery->targetList),
									exprType((Node *) te->expr),
									exprTypmod((Node *) te->expr),
									0);

	query->jointree = (FromExpr *) replaceSubExpression ((Node *) query->jointree,
			list_make1(info->parent), list_make1((Node *) nullTest),REPLACE_SUB_EXPR_QUERY);

	return query;
}



/*
 * Checks if the sublink is used in an outer join qualification
 */

static bool
inOuterJoin (SublinkInfo *info, Query *query)
{
	Node **join;
	JoinExpr *joinExpr;

	join = getJoinForJoinQual(query, (Node *) info->exprRoot);

	if (join == NULL)
		return false;

	joinExpr = (JoinExpr *) (*join);
	return (joinExpr->jointype == JOIN_LEFT
			|| joinExpr->jointype == JOIN_RIGHT
			|| joinExpr->jointype == JOIN_FULL);
}

/*
 * Replaces the original sublink with the modified testexpression CsubPlus. For other sublinks with the same rootExpr adapt the
 * rootExpr. This is nessesary, because otherwise the MOVE strategy would use the unmodified rootExpr to create the top level
 * query qual.
 */

static void
replaceSublinkWithCsub (SublinkInfo *info, Query *query, List *infos, RangeTblRef *rtRef)
{
	Node **join;
	JoinExpr *newJoin;
	Node *cSubPlus;
	List *sameExprRoot;
	List *replaceList;
	List *searchList;
	ListCell *lc;
	SublinkInfo *curInfo;
	TargetEntry *te;
	Node *expr;

	sameExprRoot = findSublinksWithSameExprRoot (infos, info);

	/* create CsubPlus */
	cSubPlus = generateCsubPlus(info, rtRef->rtindex, false);
	if (cSubPlus == NULL)
	{
		if (info->sublink->subLinkType == EXPR_SUBLINK)
		{
			te = (TargetEntry *) linitial(((Query *)info->sublink->subselect)->targetList);
			expr = (Node *) te->expr;

			cSubPlus = (Node *) makeVar(rtRef->rtindex,1, exprType(expr), exprTypmod(expr),0);
		}
		else
			cSubPlus = makeBoolConst(true,false);
	}

	/* search list for replacement */
	searchList = list_make1(info->sublink);

	/* check if sublink is used in an join qualification */
	join = getJoinForJoinQual(query, (Node *) info->exprRoot);
	if (join)
	{
		newJoin = createJoinExpr(query, JOIN_INNER);
		newJoin->larg = *join;
		newJoin->rarg = (Node *) copyObject(rtRef);
		newJoin->quals = cSubPlus;

		(*join) = (Node *) newJoin;

		recreateJoinRTEs(query);

		newJoin = (JoinExpr *) newJoin->larg;
		if (equal(info->exprRoot, info->sublink))
			newJoin->quals = NULL;
		else
		{
			replaceList = list_make1(makeBoolConst(true,false));

			newJoin->quals = replaceSubExpression((Node *) newJoin->quals, searchList, replaceList, REPLACE_SUB_EXPR_QUERY);
		}

	}

	/* else add rtRef to formlist */
	else {
		query->jointree->fromlist = lappend (query->jointree->fromlist, rtRef);

		/* adapt testexpr by substituting references to the new range table entry for the Params */
		if (equal(info->exprRoot, info->sublink))
			query->jointree->quals = cSubPlus;
		else
		{
			replaceList = list_make1(cSubPlus);

			query->jointree = (FromExpr *) replaceSubExpression
					((Node *) query->jointree, searchList, replaceList, REPLACE_SUB_EXPR_QUERY);
		}
	}

	/* adapt rootExpr of Sublinks with the same rootExpr as "info" */
	foreach(lc, sameExprRoot)
	{
		curInfo = (SublinkInfo *) lfirst(lc);

		curInfo->exprRoot = replaceSubExpression(curInfo->exprRoot, searchList, replaceList, REPLACE_SUB_EXPR_QUERY);
	}
}

/*
 * Find sublinks from "infos" that share the same rootExpr with "info".
 */
static List *
findSublinksWithSameExprRoot (List *infos, SublinkInfo *info)
{
	List *result;
	ListCell *lc;
	SublinkInfo *curInfo;

	result = NIL;

	foreach(lc, infos)
	{
		curInfo = (SublinkInfo *) lfirst(lc);

		if (curInfo->sublinkPos != info->sublinkPos && equal(info->exprRoot, curInfo->exprRoot))
			result = lappend(result, curInfo);
	}

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
generateCsubPlus (SublinkInfo *info, Index rtIndex, bool forNot)
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
	// A negated NOT ANY C expression evaluates to
	// NULL which is interpreted as false if none
	// of the C values is true and at least one is
	// NULL. We have to check for this case in the
	// join condition.
	if (forNot)
	{
		NullTest *nullTest = makeNode(NullTest);

		nullTest->nulltesttype = IS_NULL;
		nullTest->arg = (Expr *) copyObject(result);

		result = makeBoolExpr(OR_EXPR, list_make2(result, nullTest));
	}

	result = replaceParamsMutator (result, context);

	pfree(context);

	return result;
}
