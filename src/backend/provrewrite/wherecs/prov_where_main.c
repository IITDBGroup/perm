/*-------------------------------------------------------------------------
 *
 * prov_where_main.c
 *	  PERM C 
 *
 * Portions Copyright (c) 2010 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $Perm: pgsql//SF_Perm/src/backend/provrewrite/wherecs/prov_where_main.c,v 1.542 Oct 4, 2010 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/pg_type.h"
#include "nodes/value.h"
#include "nodes/makefuncs.h"
#include "optimizer/clauses.h"
#include "parser/parse_func.h"
#include "parser/parse_expr.h"
#include "parser/parse_oper.h"
#include "parser/parsetree.h"

#include "provrewrite/provattrname.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_sublink_util_analyze.h"
#include "provrewrite/prov_where_map.h"
#include "provrewrite/prov_where_spj.h"
#include "provrewrite/prov_where_set.h"
#include "provrewrite/prov_where_util.h"
#include "provrewrite/prov_where_main.h"


/* prototypes */
static Query *generateAnnotationSetWrapper (Query *query, List *origAttrs);
static Node *getAnnotAgg (TargetEntry *te);
static Query *unionQueryList (List *queries, Query *outer);
static List *translatePsqlIntoDirectPropagators (List *queries);
static void addAnnotationAttrs (Query *query, ListCell **setPointers,
		List *whereInfos, bool usePStack);

static Query *checkAndNormalizeQuery (Query *query, bool belowSetOp);
static void checkQueryOkForWhere (Query *query, bool belowSetop);
static void checkWhereSetOps (Query *query, Node *setOp);
static bool checkOkForWhereWalker (Node *node, void *context);
static void checkJoinTree (Node *joinItem);


/*
 *
 */

Query *
rewriteQueryWhere (Query *query)
{
	List *psqlQs;
	Query *unions;
	List *origAttrs;

	origAttrs = copyObject(query->targetList);
	query = checkAndNormalizeQuery(query, false);

	if (query->setOperations)
		psqlQs = rewriteSetWhere(query);
	else
		psqlQs = list_make1(rewriteWhereSPJQuery(query));

	psqlQs = translatePsqlIntoDirectPropagators(psqlQs);

	if (list_length(psqlQs) > 1)
		unions = unionQueryList(psqlQs, NULL);
	else
		unions = (Query *) linitial(psqlQs);

	query = generateAnnotationSetWrapper (unions, origAttrs);

	return query;
}

/*
 *
 */

Query *
rewriteQueryWhereInSen (Query *query)
{
	List *origAttrs;
	List *psqlQs;
	Query *unions;

	origAttrs = copyObject(query->targetList);
	query = checkAndNormalizeQuery(query, false);

	makeRepresentativeQuery (query);

	if (query->setOperations)
		psqlQs = rewriteWhereInSetQuery(query);
	else
		psqlQs = generateAuxQueries (list_make1(query), true);

	psqlQs = translatePsqlIntoDirectPropagators (psqlQs);

	if (list_length(psqlQs) > 1)
		unions = unionQueryList(psqlQs, NULL);
	else
		unions = (Query *) linitial(psqlQs);

	query = generateAnnotationSetWrapper (unions, origAttrs);

	return query;
}

/*
 *
 */

Query *
rewriteQueryWhereInSenNoUnion (Query *query)
{
	List *psqlQs;
	Query *unions;
	List *origAttrs;

	origAttrs = copyObject(query->targetList);
	query = checkAndNormalizeQuery(query, true);

	makeRepresentativeQuery (query);

	psqlQs = generateAuxQueries (list_make1(query), false);
	psqlQs = translatePsqlIntoDirectPropagators (psqlQs);

	if (list_length(psqlQs) > 1)
		unions = unionQueryList(psqlQs, NULL);
	else
		unions = (Query *) linitial(psqlQs);

	query = generateAnnotationSetWrapper (unions, origAttrs);

	return query;
}

/*
 *
 */

static Query *
checkAndNormalizeQuery (Query *query, bool belowSetOp)
{
	WhereProvInfo *provInfo;

	checkQueryOkForWhere(query, belowSetOp);
	query = pullUpSubqueries(query);

	query->distinctClause = NIL;

	provInfo = (WhereProvInfo *) makeNode(WhereProvInfo);
	provInfo->attrInfos = NIL;
	Provinfo(query)->copyInfo = (Node *) provInfo;

	return query;
}

/*
 * Generates the final wrapper query that generates annotation sets from the
 * single attribute annotations of all duplicates of a tuple and reorders the
 * attributes to interleave the original attributes with the annotation
 * attributes aggregated into sets:
 * 		SELECT a1, SETAGG(annot_a1), a2, SETAGG(annot_a2), ...
 * 		...
 * 		GROUP BY a1, a2, ...
 */

static Query *
generateAnnotationSetWrapper (Query *query, List *origAttrs)
{
	List *pList;
	ListCell *lc, *annLc;
	Query *wrapper;
	TargetEntry *origTe, *newTe;
	int curResno, curGroupRef;
	Var *var;
	GroupClause *groupBy;
	RangeTblRef *rtRef;
	char *attrname;

	pList = (List *) pop(&pStack);

	wrapper = makeQuery();
	wrapper->hasAggs = true;
	addSubqueryToRT(wrapper, query, "AnnotationAggregation");
	correctRTEAlias(rt_fetch(1, wrapper->rtable));
	MAKE_RTREF(rtRef, 1);
	wrapper->jointree->fromlist = list_make1(rtRef);

	/* create target list */
	curResno = curGroupRef = 1;
	forboth(lc, origAttrs, annLc, pList)
	{
		origTe = (TargetEntry *) lfirst(lc);
		attrname = origTe->resname;

		// create original attribute
		var = makeVar(1, origTe->resno, exprType((Node *) origTe->expr),
				exprTypmod((Node *) origTe->expr), 0);
		newTe = makeTargetEntry((Expr *) var, curResno++,
				pstrdup(origTe->resname), false);
		newTe->ressortgroupref = curGroupRef;
		wrapper->targetList = lappend(wrapper->targetList, newTe);

		// create group by for original attribute
		groupBy = makeNode(GroupClause);
		groupBy->tleSortGroupRef = curGroupRef++;
		groupBy->sortop = ordering_oper_opid(var->vartype);
		wrapper->groupClause = lappend(wrapper->groupClause, groupBy);

		// create annotation attribute
		origTe = (TargetEntry *) lfirst(annLc);
		newTe = makeTargetEntry((Expr *) getAnnotAgg(origTe), curResno++,
				getWhereAnnotName(attrname), false);
		wrapper->targetList = lappend(wrapper->targetList, newTe);
	}

	return wrapper;
}

/*
 *
 */

static Node *
getAnnotAgg (TargetEntry *te)
{
	Var *inputVar;
	Aggref *agg;
	Oid argTypes[1] = { TEXTOID };

	inputVar = makeVar(1, te->resno, exprType((Node *) te->expr),
			exprTypmod((Node *) te->expr), 0);

	agg = makeNode(Aggref);
	agg->args = list_make1(inputVar);
	agg->agglevelsup = 0;
	agg->aggstar = false;
	agg->aggdistinct = false;
	agg->aggtype = TEXTARRAYOID;
	agg->aggfnoid = LookupFuncName(
			list_make1(makeString("tarr_conc")), 1, argTypes, false);

	return (Node *) agg;
}

/*
 *
 */

#define MAKE_NEW_UNION(leftArg,rightArg) \
	do { \
		setOp = (SetOperationStmt *) makeNode(SetOperationStmt); \
		setOp->colTypes = copyObject(colTypes); \
		setOp->colTypmods = copyObject(colTypmods); \
		setOp->op = SETOP_UNION; \
		setOp->all = true; \
		setOp->larg = (Node *) leftArg; \
		setOp->rarg = (Node *) rightArg; \
	} while (0)

static Query *
unionQueryList (List *queries, Query *outer)
{
	Query *cur;
	Query *result;
	ListCell *lc;
	SetOperationStmt *setOp;
	SetOperationStmt *newOp;
	RangeTblRef *rtRef, *leftRtRef;
	RangeTblEntry *rte;
	List *colTypes = NIL;
	List *colTypmods = NIL;
	int i;
	TargetEntry *te, *newTe;
	Var *var;

	// only one query, just return it
	if (list_length(queries) == 1)
		return (Query *) linitial(queries);

	result = makeQuery();

	if (outer == NULL)
		cur = (Query *) linitial(queries);
	else
		cur = outer;

	/* retrieve colTypes and colTypmods and generate target list */
	foreachi(lc, i, cur->targetList)
	{
		te = (TargetEntry *) lfirst(lc);
		var = makeVar(1, i + 1, exprType((Node *) te->expr),
				exprTypmod((Node *) te->expr), 0);
		newTe = makeTargetEntry((Expr *) var, i + 1,
				pstrdup(te->resname), false);

		result->targetList = lappend(result->targetList, newTe);

		colTypes = lappend_oid(colTypes, exprType((Node *) te->expr));
		colTypmods = lappend_int(colTypmods, exprTypmod((Node *) te->expr));
	}

	/* add queries to range table */
	foreach(lc, queries)
	{
		addSubqueryToRT(result, (Query *) lfirst(lc),
				appendIdToStringPP("DirectPropagator_",&i));
		rte = (RangeTblEntry *) llast(result->rtable);
		correctRTEAlias(rte);
	}

	/* union all queries in the list */
	leftRtRef = makeNode(RangeTblRef);
	leftRtRef->rtindex = 1;
	rtRef = makeNode(RangeTblRef);
	rtRef->rtindex = 2;

	MAKE_NEW_UNION(leftRtRef, rtRef);

	if (list_length(queries) > 2)
	{
		for (i = 3, lc = lnext(lnext(list_head(queries))); lc != NULL; lc = lc->next, i++)
		{
			cur = (Query *) lfirst(lc);

			rtRef = makeNode(RangeTblRef);
			rtRef->rtindex = i;

			newOp = setOp;
			MAKE_NEW_UNION(newOp, rtRef);
		}
	}

	result->setOperations = (Node *) setOp;

	return result;
}

/*
 *
 *
 */

static List *
translatePsqlIntoDirectPropagators (List *queries)
{
	List *result = NIL;
	ListCell *lc, *innerLc;
	ListCell **setPointers;
	int numOutAttr, maxSetSize, i;
	Query *psqlQ;
	Query *newPropQ;
	List *whereInfos;
	WhereAttrInfo *attrInfo;
	bool pStack = true;

	numOutAttr = list_length(((Query *) linitial(queries))->targetList);
	setPointers = (ListCell **) palloc(numOutAttr * sizeof(ListCell*));

	foreach(lc, queries)
	{
		psqlQ = (Query *) lfirst(lc);
		whereInfos = GET_WHERE_ATTRINFOS(psqlQ);
		((ProvInfo *) Provinfo(psqlQ))->copyInfo = NULL;

		/* get maximal set size */
		maxSetSize = 0;
		foreachi(innerLc, i, whereInfos)
		{
			attrInfo = (WhereAttrInfo *) lfirst(innerLc);
			maxSetSize = list_length(attrInfo->annotVars) > maxSetSize ?
					list_length(attrInfo->annotVars) : maxSetSize;
			setPointers[i] = list_head(attrInfo->annotVars);
		}

		/* generate |maxSetSize| copies of the original query with each one
		 * copying from one attribute of each out attribute annotation origin
		 * set (if available).
		 */
		for(i = 0; i < maxSetSize; i++)
		{
			newPropQ = copyObject(psqlQ);

			// add an annotation origin from each attrInfo inVar set
			addAnnotationAttrs(newPropQ, setPointers, whereInfos, pStack);
			pStack = false;

			result = lappend(result, newPropQ);
		}
	}

	return result;
}

/*
 * create a query that copies annotations from one input annotation
 * attribute per query output attribute.
 */

static void
addAnnotationAttrs (Query *query, ListCell **setPointers, List *whereInfos,
		bool usePStack)
{
	TargetEntry *annAttr;
	WhereAttrInfo *attrInfo;
	ListCell *lc;
	List *pList = NIL;
	Node *expr;
	int i = 0;
	int curResno = list_length(query->targetList);

	foreach(lc, whereInfos)
	{
		attrInfo = (WhereAttrInfo *) lfirst(lc);

		// there are more annotations for this attribute to gather
		if (setPointers[i] != NULL)
		{
			expr = (Node *) lfirst(setPointers[i]);
			setPointers[i] = setPointers[i]->next;
		}
		// no annotations left for this attribute add null constant
		else
			expr = (Node *) makeNullConst(TEXTOID, -1);

		annAttr = makeTargetEntry((Expr *) expr, ++curResno,
				appendIdToStringPP("ann_attr", &i), false);

		query->targetList = lappend(query->targetList, annAttr);

		if (usePStack)
			pList = lappend(pList, annAttr);
	}

	if (usePStack)
		push(&pStack, pList);
}


/*
 * Check if the query we want to rewrite has only features supported by where-cs:
 * 		-no functions in SELECT and WHERE clause
 * 		-only equality comparisons in WHERE and no negation or disjunctions
 *
 */

static void
checkQueryOkForWhere (Query *query, bool belowSetOp)
{
	ListCell *lc;
	TargetEntry *te;
	Node *fromItem;
	RangeTblEntry *rte;

	if (query->setOperations)
	{
		if (belowSetOp)
			ereport (ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
							errmsg("WHERE-CS only allowed for set operations "
									"at the top level of the query.")));
		checkWhereSetOps(query, query->setOperations);
	}

	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);

		if (!isVarOrConstWithCast((Node *) te->expr))
			ereport (ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
							errmsg("WHERE-CS only allowed for projection on "
									"variables without functions "
									"expressions.")));
	}

	foreach(lc, query->jointree->fromlist)
	{
		fromItem = (Node *) lfirst(lc);

		checkJoinTree(fromItem);
	}

	if (checkOkForWhereWalker(query->jointree->quals, NULL))
		ereport (ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("WHERE-CS only supports conjunctive equality"
								" comparisons in WHERE clause.")));

	if (query->hasSubLinks)
		ereport (ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("WHERE-CS does not support sublinks.")));

	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		if (rte->rtekind == RTE_SUBQUERY)
			checkQueryOkForWhere(rte->subquery, false);
	}
}

/*
 *
 */

static void
checkWhereSetOps (Query *query, Node *setOp)
{
	if (IsA(setOp, SetOperationStmt))
	{
		SetOperationStmt *oper = (SetOperationStmt *) setOp;

		if (oper->op != SETOP_UNION)
			ereport (ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
							errmsg("WHERE-CS only supports UNION set "
									"operation.")));

		checkWhereSetOps(query, oper->larg);
		checkWhereSetOps(query, oper->rarg);
	}
	else
	{
		RangeTblRef *rtRef = (RangeTblRef *) setOp;
		RangeTblEntry *rte = rt_fetch(rtRef->rtindex, query->rtable);

		if (rte->rtekind == RTE_SUBQUERY)
			checkQueryOkForWhere(rte->subquery, true);
	}
}

/*
 * Throw an error if there is a feature unsupported by where-CS.
 */

static bool
checkOkForWhereWalker (Node *node, void *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, BoolExpr))
	{
		BoolExpr *expr = (BoolExpr *) node;

		if (expr->boolop != AND_EXPR)
			return true;

		return expression_tree_walker(node, checkOkForWhereWalker, context);
	}

	if (isVarOrConstWithCast(node))
		return false;

	if (IsA(node, OpExpr))
	{
		OpExpr *oper = (OpExpr *) node;

		if (!isEqualityOper(oper))
			return true;

		return expression_tree_walker(node, checkOkForWhereWalker, context);
	}

	return true;
}

/*
 * Checks that the join tree only contains inner joins and the quals for all
 * joins are conjunctions of equality comparisons.
 */

static void
checkJoinTree (Node *joinItem)
{
	if (IsA(joinItem, JoinExpr))
	{
		JoinExpr *join = (JoinExpr *) joinItem;

		/* only inner joins are allowed */
		if (join->jointype != JOIN_INNER)
			ereport (ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
							errmsg("WHERE-CS only support inner joins")));

		/* qual has to be conjunction of equalities */
		if (checkOkForWhereWalker(join->quals, NULL))
			ereport (ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
							errmsg("WHERE-CS only supports conjunctive equality"
									" comparisons in join conditions.")));

		checkJoinTree(join->larg);
		checkJoinTree(join->rarg);
	}
	else if (!IsA(joinItem, RangeTblRef))
		ereport (ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
						errmsg("WHERE-CS only allowed for projection on variables"
								" without functions expressions.")));
}

