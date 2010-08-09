/*-------------------------------------------------------------------------
 *
 * prov_sublink_unnest.c
 *	  PERM C - unnests and decorrelates sublinks for provenance rewrite.
 *	  The following rewrite strategies are implemented
 *	  in this file:
 *	  		1) JA strategy
 *	  		2) EXISTS strategy
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/prov_sublink_unnest.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "utils/lsyscache.h"
#include "catalog/pg_type.h"
#include "utils/syscache.h"
#include "nodes/makefuncs.h"
#include "optimizer/clauses.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "parser/parse_expr.h"
#include "parser/parse_coerce.h"
#include "parser/parse_type.h"
#include "parser/parsetree.h"
#include "parser/parse_relation.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_sublink_unnest.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/provstack.h"

/* prototypes */
static bool checkCorrVarsInEqualityWhere (SublinkInfo *info);
static bool hasCorrVarsBelowAggOrSet (SublinkInfo *info);
static bool hasCorrSkippingSublinks (SublinkInfo *info);
static bool checkCorrVarsInOpWhere (SublinkInfo *info);
static bool existsOrNotExistsInAnd (SublinkInfo *info);

static RangeTblRef *rewriteAggregateSublinkQuery (Query *query,
		SublinkInfo *info, Index subList[], List **corrPos);
static RangeTblRef *rewriteExistsSublinkQuery (Query *query, SublinkInfo *info,
		Index subList[]);
static Query *rewriteExistsWithoutInject (Query *query, SublinkInfo *info);
static RangeTblRef *rewriteNotToLeftJoin (Query *query, SublinkInfo *info,
		Index subList[], bool exists);
static Node *createCorrelationPredicates (SublinkInfo *info, List *corrVarPos,
		Index rangeTblPos);
static Query *rewriteWithInject (Query *query, SublinkInfo *info,
		List **corrPos);
static Query *rewriteWithoutInject (Query *query, SublinkInfo *info,
		List **corrPos);
static Query *rewriteExistsWithInject (Query *query, SublinkInfo *info);
static List *generateGroupBy (SublinkInfo *info);
static void replaceSubExprInJoinTree (Query *query, List *searchList,
		List *replaceList);
static List *generateGroupByForExists (SublinkInfo *info);
static Node *generateCsubPlus (SublinkInfo *info, Index rtIndex, Query *query);
static void replaceSublinkWithLeftJoinForExists (SublinkInfo *info,
		Query *query, RangeTblRef *rtRef, List *corrPos);
static void replaceSublinkWithCsubPlus(SublinkInfo *info, Query *query,
		RangeTblRef *rtRef, List *corrPos);
static void replaceSublinkWithCsubPlusForExists (SublinkInfo *info, Query *query,
		RangeTblRef *rtRef);
static bool parentIsNot (SublinkInfo *info);
static bool rewriteNeedsInject (SublinkInfo *info);
static Node *replaceSublinksWithTrueMutator (Node *node, void *context);
static List *addJoinForCorrelationPredicates (Query *query, SublinkInfo *info);
static List *generateGroupByForInject (SublinkInfo *info, List *predVars);
static List *generateGroupByForExistsInject (SublinkInfo *info, List *predVars);
static Query *generateInjectQuery (Query *query, SublinkInfo *info,
		Index *newCorrVarnos, Index *newCorrAttrNums);
static bool checkInjectNeedsOnlyCorrelatedRTEs (Query *query, SublinkInfo *info);
static Node *replaceAggsWithConstsMutator (Node *node, void *context);
static Node *replaceParamsWithNodeMutator (Node *node, Node *context);

/*
 *	Checks if the preconditions to apply the JA strategy are fulfilled.
 *
 *	These are:
 *		1) Sublink is an ANY- or EXPR-sublink or EXISTS sublink
 *		2) The Sublink's query is an aggregation without group by and having
 *		3) The sublink is used in WHERE in an AND-tree
 *		4) All correlation predicates are equality and are used in WHERE in an
 *			AND tree.
 *		5) Sublink is correlated (no required precondition for applicability of
 *			the method, but if the Sublink is uncorrelated we can use another
 *			more efficient method)
 *		6) Sublink does not contain sublinks that have to be rewritten using
 *			GEN-strategy (the correlations used there wouldn't work in a
 *			FROM-clause item)
 *		7) Test expression of sublink does not contain a sublink
 */

bool
checkJAstrategyPreconditions(SublinkInfo *info)
{
	bool result;

	result = info->category == SUBCAT_CORRELATED;
	result = result && info->location == SUBLOC_WHERE;
	result = result && SublinkInAndOrTop(info);
	result = result && (info->sublink->subLinkType == ANY_SUBLINK
			|| info->sublink->subLinkType == EXPR_SUBLINK
			|| info->sublink->subLinkType == EXISTS_SUBLINK);
	result = result && queryIsAggWithoutGroupBy(
			(Query *) info->sublink->subselect);
	result = result && checkCorrVarsInOpWhere(info);
	result = result && !containsGenCorrSublink(info);
	result = result && !sublinkHasSublinksInTestOrInTest(info);

	return result;
}

/*
 * Checks if the preconditions of the EXISTS strategy are fulfilled.
 *
 * These are:
 * 		1) Sublink is an EXISTS sublink
 * 		2) The sublink's query is not an aggregation
 * 		3) The sublink is used in WHERE in an AND-tree (a NOT a parent of the
 * 			sublink is allowed)
 * 		4) All correlation predicates are equality and are used in WHERE in an
 * 			AND tree.
 * 		5) Sublink is correlated (no required precondition for the method, but
 * 			is the Sublink is uncorrelated we can use another more efficient
 * 			method)
 *		6) Sublink does not contain sublinks that have to be rewritten using
 *			GEN-strategy (the correlations used there wouldn't work in a
 *			FROM-clause item)
 */

bool
checkEXISTSstrategyPreconditions(SublinkInfo *info)
{
	bool result;

	result = info->category == SUBCAT_CORRELATED;
	result = result && info->location == SUBLOC_WHERE;
	result = result && existsOrNotExistsInAnd(info);
	result = result && !hasCorrVarsBelowAggOrSet(info);
	result = result && checkCorrVarsInOpWhere(info);
	result = result && !hasCorrSkippingSublinks(info);
	result = result && !containsGenCorrSublink(info);

	return result;
}

/*
 * Checks if sublink is an EXISTS sublink in an AND tree (with direct parent
 * allowed to be a NOT).
 */

static bool
existsOrNotExistsInAnd(SublinkInfo *info)
{
	BoolExpr *expr;

	if (info->sublink->subLinkType != EXISTS_SUBLINK)
		return false;

	if (exprInAndOrTop((Node *) info->sublink, info->exprRoot))
		return true;

	if (IsA(info->parent, BoolExpr))
	{
		expr = (BoolExpr *) info->parent;

		if (expr->boolop == NOT_EXPR)
			return exprInAndOrTop(info->parent, info->exprRoot);
	}
	return false;
}

/*
 *	Checks if all correlated variables of a sublink are used in the WHERE
 *	clause in an equality condition.
 */

static bool
checkCorrVarsInEqualityWhere(SublinkInfo *info)
{
	ListCell *lc;
	CorrVarInfo *corrVar;

	foreach(lc, info->corrVarInfos)
	{
		corrVar = (CorrVarInfo *) lfirst(lc);

		if (corrVar->location != SUBLOC_WHERE)
			return false;
		if (!corrIsEquality(corrVar))
			return false;
	}

	return true;
}

/*
 *
 */

static bool
hasCorrSkippingSublinks(SublinkInfo *info)
{
	ListCell *lc;
	CorrVarInfo *corrVar;

	foreach(lc, info->corrVarInfos)
	{
		corrVar = (CorrVarInfo *) lfirst(lc);
		if (corrVar->trueVarLevelsUp > 1)
			return true;
	}

	return false;
}

/*
 * Checks if at least one of the correlated variables is below an aggregation
 * or set operation.
 */

static bool
hasCorrVarsBelowAggOrSet(SublinkInfo *info)
{
	ListCell *lc;
	CorrVarInfo *corrVar;

	foreach(lc, info->corrVarInfos)
	{
		corrVar = (CorrVarInfo *) lfirst(lc);
		if (corrVar->belowAgg || corrVar->belowSet)
			return true;
	}

	return false;
}

/*
 * Checks if all correlated variables of a sublink are used in the WHERE
 * clause in an op.
 */

static bool
checkCorrVarsInOpWhere(SublinkInfo *info)
{
	ListCell *lc;
	CorrVarInfo *corrVar;

	foreach(lc, info->corrVarInfos)
	{
		corrVar = (CorrVarInfo *) lfirst(lc);

		if (corrVar->location != SUBLOC_WHERE)
			return false;
		if (!corrIsOp(corrVar))
			return false;
	}

	return true;
}

/*
 * Transforms a JA-Query into a join.
 */

Query *
rewriteJAstrategy(Query *query, SublinkInfo *info, Index subList[])
{
	RangeTblRef *rtRef;
	List *corrPos;

	corrPos = NIL;

	info->unnested = true;
	info->rewrittenSublinkQuery
			= (Query *) copyObject(info->sublink->subselect);

	rtRef = rewriteAggregateSublinkQuery(query, info, subList, &corrPos);

	/* for EXISTS an aggregation means that the sublink condition
	 * is allways true, because the aggregation always returns a tuple.
	 * Therefore we have to use an left join to handle the case where
	 * the correlated input of the aggregation produces no tuples.
	 */
	if (info->sublink->subLinkType == EXISTS_SUBLINK)
		replaceSublinkWithLeftJoinForExists(info, query, rtRef, corrPos);
	else
		replaceSublinkWithCsubPlus(info, query, rtRef, corrPos);

	return query;
}

/*
 * Transform an EXISTS sublink into an count(*) on the sublink query and
 * replace EXISTS in condition with 0 < count(*).
 */

Query *
rewriteEXISTSstrategy(Query *query, SublinkInfo *info, Index subList[])
{
	Query *sublink;
	RangeTblRef *rtRef;

	info->unnested = true;
	info->rewrittenSublinkQuery
			= (Query *) copyObject(info->sublink->subselect);
	sublink = (Query *) info->rewrittenSublinkQuery;

	/* rewrite sublink query */
	if (parentIsNot(info))
	{
		joinQueryRTEs(query);
		rtRef = rewriteNotToLeftJoin(query, info, subList, true);
	}
	else
	{
		rtRef = rewriteExistsSublinkQuery(query, info, subList);

		/* create join condition */
		replaceSublinkWithCsubPlusForExists(info, query, rtRef);
	}

	return query;
}

/*
 *
 */

static RangeTblRef *
rewriteNotToLeftJoin(Query *query, SublinkInfo *info, Index subList[],
		bool exists)
{
	RangeTblRef *rtRef;
	Index sublinkIndex;
	JoinExpr *joinExpr;
	List *corrPos;
	List *predVars;
	Node *corrPred;
	NullTest *nullTest;
	TargetEntry *te;

	/* rewrite the sublink query */
	if (rewriteNeedsInject(info))
	{
		predVars = addJoinForCorrelationPredicates(query, info);
		if (exists)
			corrPos = generateGroupByForExistsInject(info, predVars);
		else
			corrPos = generateGroupByForInject(info, predVars);
	}
	else
	{
		if (exists)
			corrPos = generateGroupByForExists(info);
		else
			corrPos = generateGroupBy(info);
	}

	info->rewrittenSublinkQuery = rewriteQueryNode(info->rewrittenSublinkQuery);

	corrPred = createCorrelationPredicates(info, corrPos, list_length(
			query->rtable) + 1);

	/* add rewriten sublink to RT */
	addSubqueryToRT(query, info->rewrittenSublinkQuery, appendIdToString(
			"rewrittenSublink", &curUniqueRelNum));
	correctRTEAlias((RangeTblEntry *) lfirst(query->rtable->tail));

	sublinkIndex = list_length(query->rtable);
	subList[info->sublinkPos] = sublinkIndex;

	/* add RangeTblRef for new RTE to jointree */
	rtRef = makeNode(RangeTblRef);
	rtRef->rtindex = sublinkIndex;

	/* create left join with original query */
	joinExpr = createJoinExpr(query, JOIN_LEFT);
	joinExpr->larg = (Node *) linitial(query->jointree->fromlist);
	joinExpr->rarg = (Node *) copyObject(rtRef);
	joinExpr->quals = corrPred;

	query->jointree->fromlist = list_make1(joinExpr);//CHECK ok to add as top level join
	recreateJoinRTEs(query);

	/* add is null predicate */
	te = (TargetEntry *) linitial((info->rewrittenSublinkQuery->targetList));

	nullTest = makeNode(NullTest);
	nullTest->nulltesttype = IS_NULL;
	nullTest->arg = (Expr *) makeVar(sublinkIndex, 1, exprType(
			(Node *) te->expr), exprTypmod((Node *) te->expr), 0);

	query->jointree = (FromExpr *) replaceSubExpression(
			(Node *) query->jointree, list_make1(info->parent),
			list_make1((Node *) nullTest), REPLACE_SUB_EXPR_QUERY);

	return rtRef;
}

/*
 * Rewrite the aggregation sublink query and add it to the range table of
 * "query". Returns a RangeTblRef for the rewritten sublink query.
 */

static RangeTblRef *
rewriteAggregateSublinkQuery(Query *query, SublinkInfo *info, Index subList[],
		List **corrPos)
{
	RangeTblRef *rtRef;
	Index sublinkIndex;

	/* rewrite the sublink query */
	if (rewriteNeedsInject(info))
		rewriteWithInject(query, info, corrPos);
	else
		rewriteWithoutInject(query, info, corrPos);

	/* add a dummy constant value attr for use in CsubPlus */
	addDummyAttr(info->rewrittenSublinkQuery);

	/* add rewriten sublink to RT */
	addSubqueryToRT(query, info->rewrittenSublinkQuery, appendIdToString(
			"rewrittenSublink", &curUniqueRelNum));
	correctRTEAlias((RangeTblEntry *) lfirst(query->rtable->tail));

	sublinkIndex = list_length(query->rtable);
	subList[info->sublinkPos] = sublinkIndex;

	/* add RangeTblRef for new RTE to jointree */
	MAKE_RTREF(rtRef, sublinkIndex);

	return rtRef;
}

/*
 *
 */

static RangeTblRef *
rewriteExistsSublinkQuery(Query *query, SublinkInfo *info, Index subList[])
{
	RangeTblRef *rtRef;
	Index sublinkIndex;

	/* rewrite the sublink query */
	if (rewriteNeedsInject(info))
		rewriteExistsWithInject(query, info);
	else
		rewriteExistsWithoutInject(query, info);

	/* add rewriten sublink to RT */
	addSubqueryToRT(query, info->rewrittenSublinkQuery, appendIdToString(
			"rewrittenSublink", &curUniqueRelNum));
	correctRTEAlias((RangeTblEntry *) lfirst(query->rtable->tail));

	sublinkIndex = list_length(query->rtable);
	subList[info->sublinkPos] = sublinkIndex;

	/* add RangeTblRef for new RTE to jointree */
	MAKE_RTREF(rtRef, sublinkIndex);

	return rtRef;
}

/*
 *
 */

static Query *
rewriteExistsWithInject(Query *query, SublinkInfo *info)
{
	List *predVars;
	List *corrPos;
	Node *corrPred;

	predVars = addJoinForCorrelationPredicates(query, info);

	corrPos = generateGroupByForExistsInject(info, predVars);

	info->rewrittenSublinkQuery = rewriteQueryNode(info->rewrittenSublinkQuery);

	corrPred = createCorrelationPredicates(info, corrPos, list_length(
			query->rtable) + 1);
	addConditionToQualWithAnd(query, corrPred, true);

	return info->rewrittenSublinkQuery;
}

/*
 *
 */

static Query *
rewriteExistsWithoutInject(Query *query, SublinkInfo *info)
{
	List *corrPos;
	Node *corrPred;

	corrPos = generateGroupByForExists(info);

	info->rewrittenSublinkQuery = rewriteQueryNode(info->rewrittenSublinkQuery);

	corrPred = createCorrelationPredicates(info, corrPos, list_length(
			query->rtable) + 1);
	addConditionToQualWithAnd(query, corrPred, true);

	return info->rewrittenSublinkQuery;
}

/*
 *
 */

static Query *
rewriteWithInject(Query *query, SublinkInfo *info, List **corrPos)
{
	List *predVars;
	Node *corrPred;

	predVars = addJoinForCorrelationPredicates(query, info);

	*corrPos = generateGroupByForInject(info, predVars);

	info->rewrittenSublinkQuery = rewriteQueryNode(info->rewrittenSublinkQuery);

	if (info->sublink->subLinkType != EXISTS_SUBLINK)
	{
		corrPred = createCorrelationPredicates(info, *corrPos, list_length(
				query->rtable) + 1);
		addConditionToQualWithAnd(query, corrPred, true);
	}

	return info->rewrittenSublinkQuery;
}

/*
 * For sublinks that are decorrelated by injection of the outer query into the
 * sublink query, the correlation predicates are transformed into join
 * conditions. This method generates the join between the outer query and the
 * sublink query on the correlation predicates.
 */

static List *
addJoinForCorrelationPredicates(Query *query, SublinkInfo *info)
{
	Query *rewritten;
	ListCell *lc;
	CorrVarInfo *corrVar;
	List *joinAddedQueries;
	Query *varQuery;
	JoinExpr *join;
	RangeTblRef *rtRef;
	Var *var;
	List *predVars;
	List *newCorrVars;
	Index *newCorrAttrNums;
	Index *newCorrVarnos;
	int i;

	predVars = NIL;
	newCorrVars = NIL;
	joinAddedQueries = NIL;
	newCorrAttrNums
			= (Index *) palloc(sizeof(Index) * list_length(info->corrVarInfos));
	newCorrVarnos
			= (Index *) palloc(sizeof(Index) * list_length(info->corrVarInfos));

	/* remove sublinks from the original query */
	//rewritten = copyObject(query); //TODO construct rewritten from accessed range table entries (maybe decide this on quiery complexity)
	rewritten
			= generateInjectQuery(query, info, newCorrVarnos, newCorrAttrNums);

	/* inject top query for each correlated attribute ref and add join condition */
	foreachi(lc, i, info->corrVarInfos)
	{
		corrVar = (CorrVarInfo *) lfirst(lc);
		varQuery = corrVar->parentQuery;//TODO find this query in info->rewrittenSublink

		varQuery = (Query *) findSubExpression(
				(Node *) info->rewrittenSublinkQuery, (Node *) varQuery, 0);

		/* if we have not added the top query as a range table entry to the
		 * parent query of the correlated var add a join
		 */
		if (!list_member(joinAddedQueries, varQuery))
		{
			joinQueryRTEs(varQuery);
			addSubqueryToRT(varQuery, rewritten, appendIdToString(
					"topForCorrelatedVar", &curUniqueRelNum));
			setIgnoreRTE(
					rt_fetch(list_length(varQuery->rtable), varQuery->rtable));
			correctRTEAlias(
					rt_fetch(list_length(varQuery->rtable), varQuery->rtable));

			join = createJoinExpr(varQuery, JOIN_INNER);
			join->larg = (Node *) linitial(varQuery->jointree->fromlist);
			MAKE_RTREF(rtRef, list_length(varQuery->rtable) - 1);
			join->rarg = (Node *) rtRef;

			varQuery->jointree->fromlist = list_make1(join);
			recreateJoinRTEs(varQuery);

			joinAddedQueries = lappend(joinAddedQueries, varQuery);
		}
		else
		{
			join = (JoinExpr *) linitial(varQuery->jointree->fromlist);
		}

		/* create correlation predicate *///TODO will break if in corrVar used in join condition
		var = makeVar(list_length(varQuery->rtable) - 1, newCorrAttrNums[i],
				corrVar->corrVar->vartype, corrVar->corrVar->vartypmod, false);

		corrVar->vars = list_make1(var);
		varQuery->jointree = (FromExpr *) replaceSubExpression(
				(Node *) varQuery->jointree, list_make1(corrVar->corrVar),
				list_make1(var), 0);

		predVars = lappend(predVars, var);
	}

	return predVars;
}

/*
 * Generates a cross product of the range table entries used that are accessed
 * by correlated attributes. This is done to increase performance of processing
 * the injected top query.
 */

static Query *
generateInjectQuery(Query *query, SublinkInfo *info, Index *newCorrVarnos,
		Index *newCorrAttrNums)
{
	Query *result;
	CorrVarInfo *corr;
	ListCell *lc;
	List *neededRTEs;
	RangeTblEntry *rte;
	RangeTblRef *rtRef;
	TargetEntry *te;
	int i;
	Var *var;

	/* check if it is ok to just use the correlated range table entries. For
	 * example, if the nullable side of an outer join is correlated, we cannot
	 * do this.
	 */
	if (checkInjectNeedsOnlyCorrelatedRTEs(query, info))
	{
		/* gather range table entries needed for the correlation and create new
		 * query node */
		result = makeQuery();
		neededRTEs = NIL;

		foreachi(lc, i, info->corrVarInfos)
		{
			corr = (CorrVarInfo *) lfirst(lc);

			if (list_member_int(neededRTEs, corr->corrVar->varno))
			{
				newCorrVarnos[i] = listPositionInt(neededRTEs,
						corr->corrVar->varno) + 1;
			}
			else
			{
				rte = rt_fetch(corr->corrVar->varno, query->rtable);

				result->rtable = lappend(result->rtable, copyObject(rte));
				neededRTEs = lappend_int(neededRTEs, corr->corrVar->varno);
				newCorrVarnos[i] = list_length(result->rtable);

				MAKE_RTREF(rtRef, newCorrVarnos[i]);
				result->jointree->fromlist = lappend(
						result->jointree->fromlist, rtRef);
			}
		}
	}
	else
		result = copyObject(query);

	/* check that all correlated vars are actually in the target list and
	 * change corr var to reference the target list entry of the
	 * parent query.
	 */
	foreachi(lc,i, info->corrVarInfos) //TODO use new corr vars from newCorrVars!
	{
		corr = (CorrVarInfo *) lfirst(lc);
		var = copyObject(corr->corrVar);
		var->varlevelsup = 0;
		var->varno = newCorrVarnos[i];

		te = findTeForVar(var, result->targetList);

		/* is the var used as a target entry? */
		if (!te)
		{
			te = makeTargetEntry((Expr *) var, list_length(result->targetList)
					+ 1, appendIdToString("newCorrAttr", &curUniqueAttrNum),
					false);
			result->targetList = lappend(result->targetList, te);
		}

		newCorrAttrNums[i] = te->resno;
	}

	return result;
}

/*
 *
 */

static bool
checkInjectNeedsOnlyCorrelatedRTEs(Query *query, SublinkInfo *info) //TODO less restrictive checks would be ok, do it!
{
	List *joinParents;
	List *joinPath;
	ListCell *lc;
	ListCell *joinLc;
	ListCell *pathLc;
	RangeTblEntry *rte;
	CorrVarInfo *corr;

	/* for each correlated range table entry check if it is used in the
	 * nullable side of an outer join */
	foreach(lc, info->corrVarInfos)
	{
		corr = (CorrVarInfo *) lfirst(lc);
		joinParents = NIL;
		joinPath = NIL;

		findRTindexInFrom(corr->corrVar->varno, query, &joinParents, &joinPath);

		forboth(joinLc, joinParents, pathLc, joinPath)
		{
			rte = rt_fetch(lfirst_int(joinLc), query->rtable);

			if (rte->rtekind == RTE_JOIN)
			{
				switch (rte->jointype)
				{
				case JOIN_LEFT:
					if (lfirst_int(pathLc) == JCHILD_RIGHT)
						return false;
					break;
				case JOIN_RIGHT:
					if (lfirst_int(pathLc) == JCHILD_LEFT)
						return false;
					break;
				case JOIN_FULL:
					return false;
				default:
					break;
				}
			}
		}

	}

	return true;
}

/*
 *
 */

static Node *
replaceSublinksWithTrueMutator(Node *node, void *context)
{
	if (node == NULL)
		return NULL;

	if (IsA(node, BoolExpr))
	{
		BoolExpr *not;
		Node *arg;

		not = (BoolExpr *) node;
		if (not->boolop == NOT_EXPR)
		{
			if (list_length(not->args) == 1)
			{
				arg = (Node *) linitial(not->args);

				if (IsA(arg, SubLink))
					return makeBoolConst(true, false);
			}
		}
	}

	if (IsA(node, OpExpr))
	{
		OpExpr *op;
		ListCell *lc;
		Node *arg;

		op = (OpExpr *) node;

		foreach(lc, op->args) //TODO we should check for if sublink is in And expression and check for casts too
		{
			arg = (Node *) lfirst(lc);

			if (IsA(arg, SubLink))
				return makeBoolConst(true, false);
		}
	}

	if (IsA(node, SubLink))
	{
		SubLink *sub;

		sub = (SubLink *) node;

		if (sub->subLinkType != EXPR_SUBLINK)
			return makeBoolConst(true, false);
	}

	return expression_tree_mutator(node, replaceSublinksWithTrueMutator,
			context);
}

/*
 * Rewrites the sublink query, by grouping on the attributes used in
 * correlation predicates, replacing correlation predicates with true constants
 * and applying normal provenance rewrites.
 */

static Query *
rewriteWithoutInject(Query *query, SublinkInfo *info, List **corrPos)
{
	/*	Node *corrPred;*/

	*corrPos = generateGroupBy(info);

	info->rewrittenSublinkQuery = rewriteQueryNode(info->rewrittenSublinkQuery);

	/*	if (info->sublink->subLinkType != EXISTS_SUBLINK) {
	 corrPred = createCorrelationPredicates(info, *corrPos, list_length(query->rtable) + 1);
	 addConditionToQualWithAnd(query, corrPred, true);
	 }*/

	return info->rewrittenSublinkQuery;
}

/*
 *
 */

static List *
generateGroupByForExists(SublinkInfo *info)
{
	ListCell *lc;
	ListCell *innerLc;
	List *searchList;
	List *replaceList;
	List *resnos;
	CorrVarInfo *corrVar;
	Var *var;
	TargetEntry *te;
	Index curResno;
	Query *query;

	searchList = NIL;
	resnos = NIL;
	query = info->rewrittenSublinkQuery;
	curResno = list_length(query->targetList);

	/* add target entries for vars in correlation predicates and use these in
	 * group by */
	foreach(lc, info->corrVarInfos)
	{
		corrVar = (CorrVarInfo *) lfirst(lc);

		foreach(innerLc, corrVar->vars)
		{
			curResno++;
			var = (Var *) lfirst(innerLc);

			te = makeTargetEntry((Expr *) var, curResno, appendIdToString(
					"groupForDecorr", &curUniqueAttrNum), false);
			query->targetList = lappend(query->targetList, te);

			resnos = lappend_int(resnos, curResno);
		}

		searchList = lappend(searchList, corrVar->parent);
	}

	replaceList = generateDuplicatesList(makeBoolConst(true, false),
			list_length(searchList));

	/* replace correlation predicates by true constants */
	info->rewrittenSublinkQuery->jointree = (FromExpr *) replaceSubExpression(
			(Node *) query->jointree, searchList, replaceList, 0);

	return resnos;
}

/*
 *
 */

static List *
generateGroupByForExistsInject(SublinkInfo *info, List *predVars)
{
	ListCell *lc;
	ListCell *varLc;
	List *resnos;
	CorrVarInfo *corrVar;
	Var *var;
	TargetEntry *te;
	Index curResno;
	Query *query;

	resnos = NIL;
	query = info->rewrittenSublinkQuery;
	curResno = list_length(query->targetList);

	/* add target entries for vars in correlation predicates */
	forboth(lc, info->corrVarInfos, varLc, predVars)
	{
		corrVar = (CorrVarInfo *) lfirst(lc);
		var = (Var *) lfirst(varLc);

		curResno++;

		te = makeTargetEntry((Expr *) var, curResno, appendIdToString(
				"groupForDecorr", &curUniqueAttrNum), false);
		query->targetList = lappend(query->targetList, te);

		resnos = lappend_int(resnos, curResno);
	}

	return resnos;
}

/*
 * Generates a group by on the correlated attributes.
 */

static List *
generateGroupBy(SublinkInfo *info)
{
	ListCell *lc;
	ListCell *innerLc;
	List *groupBy;
	List *searchList;
	List *replaceList;
	List *resnos;
	CorrVarInfo *corrVar;
	Var *var;
	TargetEntry *te;
	Index curResno;
	Index curRessortref;
	Query *query;
	SortClause *group;

	groupBy = NIL;
	searchList = NIL;
	resnos = NIL;
	query = info->rewrittenSublinkQuery;
	curResno = list_length(query->targetList);

	curRessortref = 0;
	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);
		curRessortref
				= (curRessortref > te->ressortgroupref) ? te->ressortgroupref
						: curRessortref;
	}

	/* add target entries for vars in correlation predicates and use these in
	 * group by */
	foreach(lc, info->corrVarInfos)
	{
		corrVar = (CorrVarInfo *) lfirst(lc);

		foreach(innerLc, corrVar->vars)
		{
			curResno++;
			curRessortref++;
			var = (Var *) lfirst(innerLc);

			te = makeTargetEntry((Expr *) var, curResno, appendIdToString(
					"groupForDecorr", &curUniqueAttrNum), false);
			te->ressortgroupref = curRessortref;
			query->targetList = lappend(query->targetList, te);

			group = makeNode(SortClause);
			group->tleSortGroupRef = curRessortref;
			group->nulls_first = false;
			group->sortop = ordering_oper_opid(var->vartype);

			groupBy = lappend(groupBy, group);
			resnos = lappend_int(resnos, curResno);
		}

		searchList = lappend(searchList, corrVar->parent);
	}

	replaceList = generateDuplicatesList(makeBoolConst(true, false),
			list_length(searchList));

	/* replace correlation predicates by true constants at which query node we
	 * have to do this depends on the nesting depth of the correlated vars
	 * depth.
	 */
	replaceSubExprInJoinTree(info->rewrittenSublinkQuery, searchList,
			replaceList);

	/* set group by */
	query->groupClause = groupBy;

	return resnos;
}

/*
 *
 */

static void
replaceSubExprInJoinTree(Query *query, List *searchList, List *replaceList)
{
	ListCell *lc;
	RangeTblEntry *rte;

	query->jointree = (FromExpr *) replaceSubExpression(
			(Node *) query->jointree, searchList, replaceList, 0);

	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		if (rte->rtekind == RTE_SUBQUERY)
			replaceSubExprInJoinTree(rte->subquery, searchList, replaceList);
	}
}

/*
 *
 */
static List *
generateGroupByForInject(SublinkInfo *info, List *predVars)
{
	ListCell *lc;
	ListCell *varLc;
	List *groupBy;
	List *resnos;
	CorrVarInfo *corrVar;
	Var *var;
	TargetEntry *te;
	Index curResno;
	Index curRessortref;
	Query *query;
	SortClause *group;

	groupBy = NIL;
	resnos = NIL;
	query = info->rewrittenSublinkQuery;
	curResno = list_length(query->targetList);

	curRessortref = 0;
	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);
		curRessortref
				= (curRessortref > te->ressortgroupref) ? te->ressortgroupref
						: curRessortref;
	}

	/* add target entries for vars in correlation predicates and use these in group by */
	forboth(lc, info->corrVarInfos, varLc, predVars)
	{
		corrVar = (CorrVarInfo *) lfirst(lc);
		var = (Var *) lfirst(varLc);

		curResno++;
		curRessortref++;

		te = makeTargetEntry((Expr *) var, curResno, appendIdToString(
				"groupForDecorr", &curUniqueAttrNum), false);
		te->ressortgroupref = curRessortref;
		query->targetList = lappend(query->targetList, te);

		group = makeNode(SortClause);
		group->tleSortGroupRef = curRessortref;
		group->nulls_first = false;
		group->sortop = ordering_oper_opid(var->vartype);

		groupBy = lappend(groupBy, group);
		resnos = lappend_int(resnos, curResno);
	}

	/* set group by */
	query->groupClause = groupBy;

	return resnos;
}

/*
 *
 */

static Node *
createCorrelationPredicates(SublinkInfo *info, List *corrVarPos,
		Index rangeTblPos)
{
	ListCell *lc;
	ListCell *innerLc;
	CorrVarInfo *corrVarInfo;
	Node *predicate;
	List *predicates;
	Var *var;
	Var *corrVar;
	int i;

	predicates = NIL;

	i = 0;
	foreach(lc, info->corrVarInfos)
	{
		corrVarInfo = (CorrVarInfo *) lfirst(lc);

		foreach(innerLc, corrVarInfo->vars)
		{
			var = (Var *) lfirst(innerLc);
			var = copyObject(var);
			var->varattno = list_nth_int(corrVarPos, i);
			var->varno = rangeTblPos;

			corrVar = copyObject(corrVarInfo->corrVar);
			corrVar->varlevelsup = 0;

			predicate = createEqualityConditionForVars(var, corrVar);

			predicates = lappend(predicates, predicate);
			i++;
		}
	}

	return createAndFromList(predicates);
}

/*
 *
 */
static void
replaceSublinkWithLeftJoinForExists(SublinkInfo *info, Query *query,
		RangeTblRef *rtRef, List *corrPos)
{
	Node *corrPreds;
	JoinExpr *join;

	/* generate CsubPlus */
	corrPreds = createCorrelationPredicates(info, corrPos, list_length(
			query->rtable));

	/* join range table entries */
	joinQueryRTEs(query);

	/* create left join */
	join = createJoinExpr(query, JOIN_LEFT);
	join->larg = linitial(query->jointree->fromlist);
	join->rarg = (Node *) rtRef;
	join->quals = corrPreds;

	query->jointree->fromlist = list_make1(join);

	adaptRTEsForJoins(list_make1(join), query, "left_join_for_exists");

	/* replace sublink with true const */
	query->jointree->quals = replaceSubExpression(query->jointree->quals,
			list_make1(info->sublink), list_make1(makeBoolConst(true, false)),
			REPLACE_SUB_EXPR_QUERY);
}

/*
 * Replace the sublink with the Csub condition.
 */

static void
replaceSublinkWithCsubPlus(SublinkInfo *info, Query *query, RangeTblRef *rtRef,
		List *corrPos) //TODO share code with prov_sublink_unn
{
	Node *cSubPlus;
	Node *corrPreds;
	JoinExpr *join;

	/* generate modified correlation predicates for use in left join */
	corrPreds = createCorrelationPredicates(info, corrPos, list_length(
			query->rtable));

	/* join range table entries */
	joinQueryRTEs(query);

	/* create left join */
	join = createJoinExpr(query, JOIN_LEFT);
	join->larg = linitial(query->jointree->fromlist);
	join->rarg = (Node *) rtRef;
	join->quals = corrPreds;

	query->jointree->fromlist = list_make1(join);

	adaptRTEsForJoins(list_make1(join), query, "left_join_for_decor");

	/* generate CsubPlus */
	cSubPlus = generateCsubPlus(info, rtRef->rtindex, query);

	/* adapt testexpr by substituting references to the new range table entry
	 * for the Params */
	query->jointree->quals = replaceSubExpression(query->jointree->quals,
			list_make1(info->sublink), list_make1(cSubPlus),
			REPLACE_SUB_EXPR_QUERY);
}

/*
 *
 */

static void
replaceSublinkWithCsubPlusForExists(SublinkInfo *info, Query *query,
		RangeTblRef *rtRef)
{
	Node *cSubPlus;
	List *searchList;

	cSubPlus = makeBoolConst(true, false);

	/* replace sublink with CsubPlus */
	query->jointree->fromlist = lappend(query->jointree->fromlist, rtRef);

	/* adapt testexpr by substituting references to the new range table entry
	 * for the Params */
	if (parentIsNot(info))
		searchList = list_make1(info->parent);
	else
		searchList = list_make1(info->sublink);

	query->jointree->quals = replaceSubExpression(query->jointree->quals,
			searchList, list_make1(cSubPlus), REPLACE_SUB_EXPR_QUERY);
}

/*
 * Generate the modified testexpression that is used in the join for ANY-sublinks
 */

static Node *
generateCsubPlus(SublinkInfo *info, Index rtIndex, Query *query)
{
	Node *result;
	Node *cbugExpr;
	NullTest *dummyIsNull;
	Var *dummyVar;
	List *varlist = NIL;
	RangeTblEntry *rte;
	bool isExprSublink;
	ReplaceParamsContext *context;
	TargetEntry *te;
	CaseExpr *ifExpr;
	CaseWhen *ifClause;

	/* replace Params with vars for the join */
	context = (ReplaceParamsContext *) palloc(sizeof(ReplaceParamsContext));
	context->paramSublevelsUp = 0;
	context->addVarSublevelsUp = 0;//CHECK thats ok to not add anything to varlevelsup
	context->varSublevelsUp = -1;
	context->touchParams = true;
	context->touchAggs = false;
	context->sublink = info->rewrittenSublinkQuery;
	context->useVarnoValue = rtIndex;

	result = copyObject(info->sublink->testexpr);
	result = replaceParamsMutator(result, context);

	pfree(context);

	/* get the target entry of the sublink query and replace aggregation
	 * functions with their value over the empty relations (0 for count and
	 * NULL for the others) */
	rte = rt_fetch(rtIndex, query->rtable);
	expandRTE(rte, rtIndex, 0, false, NULL, &varlist);
	dummyVar = (Var *) llast(varlist);

	dummyIsNull = makeNode(NullTest);
	dummyIsNull->arg = (Expr *) dummyVar;
	dummyIsNull->nulltesttype = IS_NULL;

	/* add expression that handles count bug (difference between grouping and
	 * correlation) */
	isExprSublink = (info->sublink->subLinkType == EXPR_SUBLINK);
	te
			= (TargetEntry *) linitial(((Query *)info->sublink->subselect)->targetList);

	cbugExpr = replaceAggsWithConstsMutator(copyObject(te->expr),
			&isExprSublink);

	if (isExprSublink)
	{
		result = (Node *) makeVar(rtIndex, 1, exprType((Node *) te->expr),
				exprTypmod((Node *) te->expr), 0);

		ifClause = makeNode(CaseWhen);
		ifClause->expr = (Expr *) dummyIsNull;
		ifClause->result = (Expr *) cbugExpr;

		ifExpr = makeNode(CaseExpr);
		ifExpr->defresult = (Expr *) result;
		ifExpr->casetype = exprType((Node *) te->expr);
		ifExpr->args = list_make1(ifClause);

		result = (Node *) ifExpr;
	}
	else
	{
		cbugExpr = replaceParamsWithNodeMutator(copyObject(
				info->sublink->testexpr), cbugExpr);
		cbugExpr = (Node *) makeBoolExpr(AND_EXPR,
				list_make2(dummyIsNull, cbugExpr));
		result = (Node *) makeBoolExpr(OR_EXPR, list_make2(result,cbugExpr));
	}

	/* create boolean expression */
	if (result == NULL)
	{
		result = makeBoolConst(true, false);
	}

	return result;
}

/*
 *
 */

static Node *
replaceAggsWithConstsMutator(Node *node, void *context)
{
	if (node == NULL)
		return NULL;

	if (IsA(node, Aggref))
	{
		Aggref *agg;

		agg = (Aggref *) node;
		if (strcmp(get_func_name(agg->aggfnoid), "count") == 0)
			return (Node *) coerce_to_specific_type(NULL, (Node *) makeConst(
					INT4OID, -1, 4, Int32GetDatum(0), false, true),
					agg->aggtype, "");
		else
			return (Node *) makeNullConst(agg->aggtype, InvalidOid);
	}

	// recurse
	return expression_tree_mutator(node, replaceAggsWithConstsMutator,
			(void *) context);
}

/*
 *
 */

static Node *
replaceParamsWithNodeMutator(Node *node, Node *context)
{
	if (node == NULL)
		return NULL;

	if (IsA(node, Param))
		return copyObject(context);

	// recurse
	return expression_tree_mutator(node, replaceParamsWithNodeMutator,
			(void *) context);
}

/*
 * Checks if the parent node of a sublink is a NOT boolean node.
 */

static bool
parentIsNot(SublinkInfo *info)
{
	BoolExpr *expr;

	if (!(info->parent))
		return false;

	if (!IsA(info->parent, BoolExpr))
		return false;

	expr = (BoolExpr *) info->parent;
	return expr->boolop == NOT_EXPR;
}

/*
 * Checks if the rewrite of a sublink requires to inject the original query
 * in the sublink query. This is the case if one of the following conditions
 * are meet:
 * 		For Tsub-Sublinks / ANY-sublinks:
 * 		1) The sublink query contains a count aggregate and the result of the
 * 			sublink is used in an comparison with the other operand might be 0
 * 			or other comparison operator than equality
 * 		2) No count aggregates, but result is compared with an operator that
 * 			is not strict and the other operand might be NULL
 *		For NOT EXISTS-Sublinks:
 *		1) Allways
 */

static bool
rewriteNeedsInject(SublinkInfo *info)
{
	if (!checkCorrVarsInEqualityWhere(info))//TODO other preconds
		return true;
	return false;
}
