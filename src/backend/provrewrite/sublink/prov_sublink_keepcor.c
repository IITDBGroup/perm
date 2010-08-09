/*-------------------------------------------------------------------------
 *
 * prov_sublink_keepcor.c
 *	  POSTGRES C a sublink rewrite variant that uses correlation to "join" the base relations accessed by a sublink with
 * 		the the original query
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/prov_sublink_keepcor.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
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
#include "optimizer/clauses.h"
#include "parser/parse_relation.h"
#include "parser/parse_expr.h"
#include "parser/parsetree.h"
#include "utils/guc.h"

#include "provrewrite/provrewrite.h"
#include "provrewrite/provlog.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_sublink_util_search.h"
#include "provrewrite/prov_sublink_util_mutate.h"
#include "provrewrite/prov_sublink_util_analyze.h"
#include "provrewrite/prov_sublink_keepcor.h"

/* prototypes */
//static void joinQueryRTEs(Query *query);
static Index addJoinWithBaseRelations (Query *query, List *baseRels);
static void addConditionForBaseRelJoin (Query *query, SublinkInfo *info);
//static JoinExpr *createJoinExpr (Query *query);
static Query *rewriteGenSublinkQuery (Query *query);
static Query *createBaseRelUnionNull (RangeTblEntry *rte, Query *query, List *provAttrs);
static Query *generateNullQuery (RangeTblEntry *rte);
static Query *generateBaseRelQuery (RangeTblEntry *rte, List *provAttrs);
static Node *generateCsub (SublinkInfo *info);
static Node *generateCsubPlus (SublinkInfo *info);
static List * createJoinExprsForBaseRelJoin (Query *query, Index startRTindex, Index rtIndex);
static void createPlistForBaseRelJoin (Query *query, Index rtIndex, Index curResno);

/* macros */
#define IS_UNION_TOP(query) \
	(((Query *) query)->setOperations != NULL \
	&& ((SetOperationStmt *)((Query *) query)->setOperations)->op == SETOP_UNION \
	&& prov_use_wl_union_semantics \
	)

/*
 * Rewrite a sublink by
 * 1) Joining all RTEs
 * 2) rewrite the sublink's query
 * 3) add cross product with all base relations unioned with a null tuple
 *    accessed by the sublink
 * 4) add a new sublinks that simulates a left join between the rewritten
 *    sublink query T_sub+ and the base relations such that a tuple from the
 *    normal query is joined with the base relations if there is a tuple from
 *    T_sub+ that has the same provenance attribute values as the base relation
 *    attribute values
 */

void
rewriteSublinkWithCorrelationToBase (Query *query, SublinkInfo *info,
		Index subPos[])
{
	Query *rewrittenSub;
	List *accessedBaseRels;
	bool inSublink;
	int baseRelStackPos;
	int subListPos;

	/* check if we are processing a top level sublink and active the base
	 * rel stack */
	inSublink = baseRelStackActive;
	baseRelStackActive = true;
	baseRelStackPos = list_length(baseRelStack);

	/* join RTEs from query */
	joinQueryRTEs(query);

	/* rewrite sublink query T_sub */
	rewrittenSub = copyObject((Query *) info->sublink->subselect);
	rewrittenSub = rewriteGenSublinkQuery(rewrittenSub);
	info->rewrittenSublinkQuery = rewrittenSub;

	/* add base relations */
	if (inSublink)
	{
		/* if we are processing a nested sublink do not remove base relations
		 * from stack because we need them for rewrite of the parent sublink.
		 */
		accessedBaseRels = getAllUntil(baseRelStack, baseRelStackPos);
	}
	else
		accessedBaseRels = popAllUntil(&baseRelStack, baseRelStackPos);

	subListPos = addJoinWithBaseRelations (query, accessedBaseRels);

	/* add sublink for join condition for the join with sublinks base
	 * relations */
	addConditionForBaseRelJoin (query, info);

	/* if sublink is a top level sublink (it is not contained in another
	 * sublink) deactivate the baseRelStack */
	if (!inSublink)
		deactiveBaseRelStack ();

	LOGNODE(query, "query after rewritten sublink");

	subPos[info->sublinkPos] = subListPos;
}

/*
 *
 */

static Query *
rewriteGenSublinkQuery (Query *query)
{
	Query *result;
	TargetEntry *te;
	TargetEntry *newTe;
	Var *var;
	ListCell *lc;
	RangeTblRef *rtRef;
	int i;
	int varUp = -1;

	/* no limit just use normal rewrite */
	if (!query->limitCount && !query->limitOffset && !IS_UNION_TOP(query))
		return rewriteQueryNode(query);

	/* query has a limit clause. This means we have to introduce a new dummy
	 * top query node or otherwise the additional correlation predicates will
	 * mess up the LIMIT clause.
	 */
	result = makeQuery();
	addSubqueryToRT(result, query,
			appendIdToString("orig_with_limit", &curUniqueRelNum));

	foreachi(lc, i, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);

		var = makeVar(1, i + 1, exprType((Node *) te->expr),
				exprTypmod((Node *) te->expr), 0);
		newTe = makeTargetEntry((Expr *) var, i + 1,
				pstrdup(te->resname), false);

		result->targetList = lappend(result->targetList, newTe);
	}

	MAKE_RTREF(rtRef, 1);
	result->jointree->fromlist = list_make1(rtRef);

	result = (Query *) query_tree_mutator ((Query *) result,
			increaseSublevelsUpMutator, &varUp,
			QTW_IGNORE_JOINALIASES | QTW_DONT_COPY_QUERY);

	return rewriteQueryNode(result);
}

/*
 *	Joins query with all base relations accessed by a sublink query.
 */

static Index
addJoinWithBaseRelations (Query *query, List *baseRels)
{
	RangeTblEntry *rte;
	ListCell *lc;
	List *provAttrs;
	Index rtIndex;
	Index startRTindex;
	Index curResno;
	Query *baseRel;
	List *subJoins;

	/*
	 *	get number of attributes before we add base relations. This might be
	 *	different from target list length, because rewriting another sublink
	 *	might have added attributes to the top level join, but not to the query
	 *	target list.
	 */
	if (list_length(query->rtable) == 0) //TODO will brreak if constants are used
	{
		curResno = list_length(query->targetList);
	}
	else if(list_length(query->rtable) > 1)
	{
		rtIndex = ((JoinExpr *) linitial(query->jointree->fromlist))->rtindex;
		rte = rt_fetch(rtIndex, query->rtable);
		curResno = list_length(rte->joinaliasvars);
	}
	else {
		rtIndex = ((RangeTblRef *) linitial(query->jointree->fromlist))->rtindex;
		rte = rt_fetch(rtIndex, query->rtable);
		curResno = list_length(rte->eref->colnames);
	}

	/* initialize variables */
	rtIndex = list_length(query->rtable) + 1;
	startRTindex = rtIndex;
	provAttrs = (List *) copyObject(linitial(pStack));

	/* add base relations unioned with null-tuples to the queries range table */
	foreach(lc,baseRels)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		baseRel = createBaseRelUnionNull (rte, query, provAttrs);

		addSubqueryToRT (query, baseRel,
				appendIdToString("base_rel_union_null",&curUniqueRelNum));
		correctRTEAlias (llast(query->rtable));
		rtIndex++;
	}

	subJoins = createJoinExprsForBaseRelJoin(query, startRTindex, rtIndex);

    /* adapt RTEs for the new joins */
	adaptRTEsForJoins(subJoins, query, "query_rte_joined");

	/*
	 * add provenance attributes from top level join to pList of sublink
	 */
	createPlistForBaseRelJoin (query, list_length(query->rtable), curResno);

	if (rtIndex == 2)
		rtIndex--;

	return list_length(query->rtable);
}

/*
 *
 */

static void
createPlistForBaseRelJoin (Query *query, Index rtIndex, Index curResno)
{
	Var *var;
	RangeTblEntry *rte;
	TargetEntry *te;
	TargetEntry *newTe;
	ListCell *lc;
	ListCell *nameLc;
	List *pList;
	int i;

	pList = NIL;

	/* if there is only one base relation and no original query RTE we have no
	 * join */
	if (rtIndex == 1)
	{
		for(nameLc = ((List *) linitial(pStack))->head, i = 1; nameLc != NULL;
				nameLc = nameLc->next, i++)
		{
			curResno++;

			te = (TargetEntry *) lfirst(nameLc);

			var = makeNode(Var);
			var->varattno = i;
			var->varno = 1;
			var->vartype = exprType((Node *) te->expr);
			var->vartypmod = exprTypmod((Node *) te->expr);
			var->varlevelsup = 0;

			newTe = makeTargetEntry((Expr *) var, i, pstrdup(te->resname),false);
			pList = lappend(pList, newTe);
		}
	}
	/* */
	else {
		/* get top level join */
		rte = (RangeTblEntry *) rt_fetch(rtIndex, query->rtable);

		/* get the vars from the top level join and the names from the pList
		 * of T_sub+ */
		lc = rte->joinaliasvars->head;
		nameLc = ((List *) linitial(pStack))->head;

		/* skip attributes from original query (with possibly added attributes
		 * from sublinks processed before) */
		for(i = 0; i < curResno; i++)
		{
			lc = lc->next;
		}

		/* create provenance attribute TargetEntries for top level join */
		for(; lc != NULL && nameLc != NULL; lc = lc->next, nameLc = nameLc->next)
		{
			curResno++;

			var = (Var *) lfirst(lc);
			var = copyObject(var);
			var->varattno = curResno;
			var->varno = rtIndex;

			te = (TargetEntry *) lfirst(nameLc);

			newTe = makeTargetEntry((Expr *) var, curResno,
					pstrdup(te->resname),false);
			pList = lappend(pList, newTe);
		}
	}

	push(&pStack, pList);
}

/*
 *	Create the join tree for the crossproduct of the original query and the
 *	base relations.
 */

static List *
createJoinExprsForBaseRelJoin (Query *query, Index startRTindex, Index rtIndex)
{
	JoinExpr *newJoin;
	List *subJoins;
	RangeTblRef *rtRef;
	int i;

	subJoins = NIL;

	/* special case original query has no RTEs (e.g. SELECT 1) and there is
	 * only one base rel */
	if (startRTindex == 1 && rtIndex == 2)
	{
			MAKE_RTREF(rtRef, rtIndex - 1);
			query->jointree->fromlist = list_make1(rtRef);
			return NIL;
	}

	/* normal case proceed by joining the top join tree node with one of the
	 * base relations and add the new join as top join tree node.
	 */
	for(i = startRTindex; i < rtIndex; i++) {
		newJoin = createJoinExpr(query, JOIN_INNER);

		newJoin->quals = (Node *) makeBoolConst(true, false);
		newJoin->larg =  linitial(query->jointree->fromlist);
		MAKE_RTREF(rtRef, i);
		newJoin->rarg = (Node *) rtRef;

		query->jointree->fromlist = list_make1(newJoin);

		subJoins = lappend(subJoins, newJoin);
	}

    return subJoins;
}

/*
 * Creates a query that produces R+ for an base relation R and unions R+ with
 * a tuple consisting only of null tuples.
 */

static Query *
createBaseRelUnionNull (RangeTblEntry *rte, Query *query, List *provAttrs)
{
	ListCell *lc;
	Var *var;
	Query *unionQuery;
	Query *subQuery;
	SetOperationStmt *unionNode;
	RangeTblRef *rtRef;

	/* create Query node */
	unionQuery = makeQuery();

	/* add base table to range table */
	subQuery = generateBaseRelQuery (rte, provAttrs);
	addSubqueryToRT (unionQuery, subQuery,
			appendIdToString("base_rel", &curUniqueRelNum));
	correctRTEAlias((RangeTblEntry *) lfirst(unionQuery->rtable->tail));

	/* create target list from base relation query target list */
	unionQuery->targetList = copyObject(subQuery->targetList);

	/* add null tuple to range table */
	subQuery = generateNullQuery (rte);
	addSubqueryToRT (unionQuery, subQuery,
			appendIdToString("null_constructor", &curUniqueRelNum));
	correctRTEAlias((RangeTblEntry *) lfirst(unionQuery->rtable->tail));

	/* create union */
	unionNode = makeNode(SetOperationStmt);
	unionNode->all = true;
	unionNode->op = SETOP_UNION;
	unionNode->colTypes = NIL;
	unionNode->colTypmods = NIL;

	rtRef = makeNode(RangeTblRef);
	rtRef->rtindex = 1;
	unionNode->larg = (Node *) rtRef;

	rtRef = makeNode(RangeTblRef);
	rtRef->rtindex = 2;
	unionNode->rarg = (Node *) rtRef;

	unionQuery->setOperations = (Node *) unionNode;

	/* create the colTypes and colTypmods lists of the union node */
	foreach(lc, unionQuery->targetList)
	{
		var = (Var *) ((TargetEntry *) lfirst(lc))->expr;
		unionNode->colTypes = lappend_oid(unionNode->colTypes, var->vartype);
		unionNode->colTypmods = lappend_int(unionNode->colTypmods, var->vartypmod);
	}

	return unionQuery;
}

/*
 *  Generate a query node for a simple SELECT * FROM baserelation; .
 */

static Query *
generateBaseRelQuery (RangeTblEntry *rte, List *provAttrs)
{
	Query *result;
	List *baseRelAttrs;
	List *vars;
	ListCell *varLc;
	ListCell *nameLc;
	Var *var;
	char *name;
	TargetEntry *te;
	TargetEntry *provTe;
	Index resno;
	RangeTblRef *rtRef;

	/* create a new Query node */
	result = makeQuery();
	result->rtable = list_make1(rte);

	/* create jointree */
	rtRef = makeNode(RangeTblRef);
	rtRef->rtindex = 1;
	result->jointree->fromlist = list_make1(rtRef);

	/* get attributes of base relation and add a target entry for each base
	 * relation attr */
	expandRTEWithParam(rte, 1, 0, false, false, &vars, &baseRelAttrs);

	resno = 1;
	forboth(varLc, baseRelAttrs, nameLc, vars)
	{
		var = (Var *) lfirst(varLc);
		provTe = pop(&provAttrs);

		name = pstrdup(provTe->resname);
		var = copyObject(var);

		te = makeTargetEntry((Expr *) var, resno, name, false);
		result->targetList = lappend(result->targetList, te);

		resno++;
	}

	return result;
}

/*
 * For a base relation given as a RangeTblEntry generate query that returns a
 * single tuple with the same schema as the base relation and all attribute
 * values set to null.
 */

static Query *
generateNullQuery (RangeTblEntry *rte)
{
	Query *result;
	ListCell *varLc;
	ListCell *nameLc;
	List *baseRelAttrs;
	List *attrNames;
	Var *var;
	char *name;
	TargetEntry *te;
	Const *nullConst;
	Index resno;

	baseRelAttrs = NIL;
	attrNames = NIL;

	/* create a new Query node */
	result = makeQuery();

	/* get attributes of base relation and add a null constant for each base
	 * relation attr */
	expandRTEWithParam(rte, 1, 0, false, false, &attrNames, &baseRelAttrs);

	resno = 1;
	forboth(varLc, baseRelAttrs, nameLc, attrNames)
	{
		var = (Var *) lfirst(varLc);
		name = pstrdup(((Value *) lfirst(nameLc))->val.str);

		nullConst = makeNullConst(var->vartype, var->vartypmod);
		te = makeTargetEntry((Expr *) nullConst, resno, name, false);
		result->targetList = lappend(result->targetList, te);

		resno++;
	}

	return result;
}



/*
 *	Create a selection condition that filter out tuples from the rewritten
 *	T_sub that do not belong to the actual T_sub^+.
 */

static void
addConditionForBaseRelJoin (Query *query, SublinkInfo *info)
{
	Query *rewrittenSubquery;
	ListCell *lc;
	ListCell *baseLc;
	List *subPlist;
	List *oldPlist;
	Node *condition;
	BoolExpr *boolNode;
	Node *Csub;
	Node *CsubPlus;
	SubLink *sublink;
	TargetEntry *te;
	Var *tsubVar;
	Var *baseRelVar;
	NullTest *nullTest;


	rewrittenSubquery = info->rewrittenSublinkQuery;

	if (info->sublink->subLinkType == ANY_SUBLINK
			|| info->sublink->subLinkType == ALL_SUBLINK)
	{
		/* generate Csub and CsubPlus from sublink condition */
		Csub = generateCsub (info);
		CsubPlus = generateCsubPlus (info);

		/* create condition */
		if (info->sublink->subLinkType == ANY_SUBLINK)
		{
			/* C_sub' OR NOT C_sub */
			boolNode = (BoolExpr *) makeBoolExpr(NOT_EXPR, list_make1(Csub));
			condition = (Node *) makeBoolExpr(OR_EXPR,
					list_make2(CsubPlus, boolNode));
		}
		if (info->sublink->subLinkType == ALL_SUBLINK)
		{
			/* C_sub OR NOT C_sub' */
			boolNode = (BoolExpr *) makeBoolExpr(NOT_EXPR,
					list_make1(CsubPlus));
			condition = (Node *) makeBoolExpr(OR_EXPR,
					list_make2(Csub, boolNode));
		}

		/* add simulated join condition to rewrittenSubquery qual */
		addConditionToQualWithAnd (rewrittenSubquery, condition, true);
		rewrittenSubquery->hasSubLinks = true;
	}

	/* fetch pList of T_sub+ from pStack and pList for the join of all
	 * base rels */
	subPlist = (List *) linitial(pStack);
	oldPlist = (List *) popNth(&pStack, 1);

	forboth(lc, oldPlist, baseLc, subPlist)
	{
		te = (TargetEntry *) lfirst(lc);
		tsubVar = (Var *) te->expr;

		te = (TargetEntry *) lfirst(baseLc);
		baseRelVar = (Var *) te->expr;
		baseRelVar = copyObject(baseRelVar);
		baseRelVar->varlevelsup = 1;

		/* create an equality (null = null too) condition for the base relation
		 * var and T_sub var */
		condition = createNotDistinctConditionForVars (tsubVar, baseRelVar);

		/* AND new equality condition with base rel attr to where clause */
		addConditionToQualWithAnd(rewrittenSubquery, condition, true);
	}

	/* create the new sublink */
	sublink = makeNode(SubLink);
	sublink->subLinkType = EXISTS_SUBLINK;
	sublink->subselect = (Node *) rewrittenSubquery;
	sublink->operName = NIL;
	sublink->testexpr = NULL;
	condition = (Node *) sublink;

	/*
	 * create a not exists sublink and base rel = null condition for the case
	 * that T_sub is empty.
	 */
	sublink = makeNode(SubLink);
	sublink->subLinkType = EXISTS_SUBLINK;
	sublink->subselect = (Node *) copyObject(info->sublink->subselect);
	sublink->operName = NIL;
	sublink->testexpr = NULL;
	boolNode = (BoolExpr *) makeBoolExpr(NOT_EXPR, list_make1(sublink));

	/* add base rel var = null condition */
	foreach(lc, subPlist)
	{
		te = (TargetEntry *) lfirst(lc);
		tsubVar = (Var *) te->expr;

		nullTest = makeNode(NullTest);
		nullTest->nulltesttype = IS_NULL;
		nullTest->arg = (Expr *) copyObject(tsubVar);

		boolNode = (BoolExpr *) makeBoolExpr(AND_EXPR,
				list_make2(boolNode, nullTest));
	}

	/*
	 * create top level OR between the sublink for joining base relations and
	 * the extra condition for the case that T_sub is empty
	 */

	condition = (Node *) makeBoolExpr(OR_EXPR,
			list_make2(condition, boolNode));

	/* add condition to where clause */
	addConditionToQualWithAnd (query, condition, true);
}

/*
 * Copies a sublink node but increases varlevelsup from the testexpr and in
 * the sublink query.
 */

static Node *
generateCsub (SublinkInfo *info)
{
	Node *result;
	ReplaceParamsContext *context;
	SubLink *sublink;
	int *increaseSublevelsContext;

	/* copy sublink */
	sublink = copyObject(info->sublink);
	result = (Node *) sublink;

	/* increase varlevelsup in sublink query of Csub */
	increaseSublevelsContext = (int *) palloc(sizeof(int));
	*increaseSublevelsContext = -1;
	sublink->subselect = increaseSublevelsUpMutator(sublink->subselect,
			increaseSublevelsContext);
	pfree(increaseSublevelsContext);

	/* increase varlevelsup in sublink test expression */
	context = (ReplaceParamsContext *) palloc(sizeof(ReplaceParamsContext));
	context->addVarSublevelsUp = 1;
	context->touchParams = false;
	context->touchAggs = false;
	context->varSublevelsUp = -1;
	context->useVarnoValue = 0;

	 ((SubLink *) result)->testexpr = replaceParamsMutator (
			 ((SubLink *) result)->testexpr, context);
	 pfree(context);

	return result;
}

/*
 * For a ANY- or ALL-sublink we create a condition that resembles the sublink
 * test expression.
 *
 * For example if the sublink is:
 * 		a = ANY (SELECT b FROM R)
 * the new condition is
 * 		a = b
 */

static Node *
generateCsubPlus (SublinkInfo *info)
{
	Node *result;
	ReplaceParamsContext *context;

	context = (ReplaceParamsContext *) palloc(sizeof(ReplaceParamsContext));
	context->paramSublevelsUp = 0;
	context->addVarSublevelsUp = 1;
	context->varSublevelsUp = -1;
	context->touchParams = true;
	context->touchAggs = false;
	context->sublink = info->rewrittenSublinkQuery;
	context->useVarnoValue = 0;

	result = copyObject(info->sublink->testexpr);
	result = replaceParamsMutator (result, context);

	pfree(context);

	return result;
}
