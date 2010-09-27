/*-------------------------------------------------------------------------
 *
 * prov_sublink_agg.c
 *	  POSTGRES C rewrites for aggregation queries with sublinks in HAVING,
 *	  			GROUP BY or target list. An aggregation query with these kinds
 *	  			of sublinks is transformed into a normalized query that
 *	  			contains none of these sublinks in the aggregation query node.
 *	  			This is achieved by adding a query node above and/or below the
 *	  			aggregation and moving the sublinks to the new query node(s).
 *	  			These operations produce an equivalent query tree.
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/prov_sublink_agg.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "parser/parse_expr.h"
#include "parser/parse_oper.h"
#include "parser/parse_relation.h"
#include "nodes/nodes.h"
#include "nodes/makefuncs.h"
#include "nodes/primnodes.h"
#include "optimizer/clauses.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/provlog.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_sublink_agg.h"

/* prototypes */
//static bool hasHavingSublink (Query *query);
static Query *createNewTopQueryForSublinks (Query *query, List* infos);
static Query *createNewBottomQueryForSublinks (Query *query, List *infos);
static void createNewTopTargetList (Query *newTop, Query *query);
static List *createTargetEntriesForNewAgg (Query *query, List *aggsAndVars);
static void adaptNewTopExprs (Query *query, List *exprs,
		List *mapAggsNewTargets);
static Node *replaceSubExprTopMutator (Node *node,
		ReplaceSubExprTopMutatorContext *context);
static bool checkVarLevelsUp(Node *node, int nestDepth);
static bool checkVarLevelsUpWalker (Node *node,
		CheckVarLevelsWalkerContext *context);
//static void getHavingClauseTEs (Node *having, Query *query, Query *newTop);
static bool aggOutputAndGroupByWalker (Node *node,
		AggOutputAndGroupByWalkerContext *context);
static List *getAggsAndVars (Query *query);
static Node *replaceAggsAndGroupbyMutator (Node *node, List **context);
static List *getSublinksForNewQueryNodes (Query *query, List *infos,
		List **newTopSublinks, List **newBottomSublinks);
static void getTargetSublinkTypes (List *sublinks, Query *query);
static bool getTargetSublinkTypeWalker (Node *node,
		GetLocationTypeContext *context);
//static Node *replaceAggsAndSublevelsUpMutator (Node *node, List **context);
static void adaptNewBottomExprs (Query *query, List *exprs);
static Node *replaceSubExprMutator (Node *node, List *context);
static List *getAggFuncInputs (Query *query);
static bool aggInputWalker (Node *node, List **context);

/*
 * Checks if a aggregation query has sublinks that require a rewrite as
 * executed by procedure transformAggWithSublinks.
 */

bool
hasHavingOrTargetListOrGroupBySublinks (Query *query)
{
	List *result;

	result = findSublinkLocations(query, PROV_SUBLINK_SEARCH_AGG);

	return (list_length(result) > 0);
}

/*
 * Checks if an aggregation query has a sublink in WHERE or ORDER BY clause.
 */

bool
hasNonHavingSublink (Query *query)
{
	List *result;

	if (!query->hasSubLinks)
		return false;

	result = findSublinkLocations(query, PROV_SUBLINK_SEARCH_WHERE
			| PROV_SUBLINK_SEARCH_ORDER);

	return (result != NIL);
}

/*
 * Transforms an aggregation query with sublinks in HAVING, GROUP BY or target
 * list into a query without these kinds of sublinks. This requires to introduce
 * a new query node above and/or below the aggregation query node and move the
 * sublinks to one of these nodes.
 */

Query *
transformAggWithSublinks (Query *query)
{
	List *sublinkInfos;
	List *newTopSublinks;
	List *newBottomSublinks;

	newTopSublinks = NIL;
	newBottomSublinks = NIL;

	sublinkInfos = findSublinkLocations(query, PROV_SUBLINK_SEARCH_AGG);

	getTargetSublinkTypes(sublinkInfos, query);

	getSublinksForNewQueryNodes(query, sublinkInfos, &newTopSublinks,
			&newBottomSublinks);

	if (newBottomSublinks != NIL)
	{
		query = createNewBottomQueryForSublinks (query, newBottomSublinks);
	}

	if (newTopSublinks != NIL)
	{
		query = createNewTopQueryForSublinks (query, newTopSublinks);
	}

	return query;
}

/*
 * Returns all sublink infos from a list of sublink infos that require us to
 * introduce a new Top query node above the aggregation.
 */

static List *
getSublinksForNewQueryNodes (Query *query, List *infos, List **newTopSublinks,
		List **newBottomSublinks)
{
	ListCell *lc;
	List *result;
	SublinkInfo *info;

	result = NIL;

	foreach(lc, infos)
	{
		info = (SublinkInfo *) lfirst(lc);

		if (info->location == SUBLOC_HAVING)
		{
			*newTopSublinks = lappend(*newTopSublinks,info);
		}
		else if (info->location == SUBLOC_SELECT)
		{
			if (info->aggLoc == SUBAGG_OUTSIDE)
			{
				*newTopSublinks = lappend(*newTopSublinks,info);
			}
			else
			{
				*newBottomSublinks = lappend(*newBottomSublinks, info);
			}
		}
		else if (info->location == SUBLOC_GROUPBY)
		{
			*newBottomSublinks = lappend(*newBottomSublinks, info);
		}
	}

	return result;
}


/*
 *
 */

static void
getTargetSublinkTypes (List *sublinks, Query *query)
{
	ListCell *lc;
	SublinkInfo *info;
	GetLocationTypeContext *context;

	foreach(lc, sublinks)
	{
		info = (SublinkInfo *) lfirst(lc);

		if (info->location == SUBLOC_SELECT)
		{
			context = (GetLocationTypeContext *)
					palloc(sizeof(GetLocationTypeContext));
			context->inAggrOrGroupBy = false;
			context->info = info;
			context->groupByExprs = query->groupClause;

			getTargetSublinkTypeWalker(info->exprRoot, context);
		}
	}
}

/*
 * Walks through a target expression which contains a sublink and returns the
 * type of the sublink location. Types are:
 *
 * 1) Used inside an aggregation function:
 * 		e.g. sum(i + (SELECT * FROM s))
 * 2) Used inside an expression that is a group by expression:
 * 		e.g. NOT (r.i IN (SELECT * FROM s)), ...
 * 						GROUP BY (r.i IN (SELECT * FROM s))
 * 3) Not contained in an aggregation or group by expression:
 * e.g. sum(i) * (SELECT * FROM s)
 */

static bool
getTargetSublinkTypeWalker (Node *node, GetLocationTypeContext *context)
{
	GetLocationTypeContext *newContext;
	ListCell *lc;
	Node *expr;

	if (node == NULL)
		return false;

	// compare to group by expressions
	foreach(lc, context->groupByExprs)
	{
		expr = (Node *) lfirst(lc);

		if (equal(expr,node)) {
			/* check if groupby is the sublink we are searching for */
			if (IsA(node, SubLink))
			{
				if (equal(node, context->info->sublink))
				{
					context->info->aggLoc = SUBAGG_IN_GROUPBY;
				}
				return true;
			}

			/* otherwise recurse into expression to check if the sublink is
			 * used in the group by expression */
			newContext = (GetLocationTypeContext *)
					palloc(sizeof(GetLocationTypeContext));
			newContext->inAggrOrGroupBy = true;
			newContext->info = context->info;
			newContext->groupByExprs = context->groupByExprs;

			return expression_tree_walker(node, getTargetSublinkTypeWalker,
					(void *) newContext);
		}
	}

	// check for aggref nodes
	if (IsA(node, Aggref))
	{
		newContext = (GetLocationTypeContext *)
				palloc(sizeof(GetLocationTypeContext));
		newContext->inAggrOrGroupBy = true;
		newContext->info = context->info;
		newContext->groupByExprs = context->groupByExprs;

		return expression_tree_walker(node, getTargetSublinkTypeWalker,
				(void *) newContext);
	}

	// check for sublinks
	if (IsA(node, SubLink))
	{
		SubLink *sublink;

		sublink = (SubLink *) node;

		/* check if we found the correct sublink */
		if (equal(sublink, context->info->sublink))
		{
			if (context->inAggrOrGroupBy)
			{
				context->info->aggOrGroup = NULL;
				context->info->aggOrGroupPointer = NULL;
				context->info->aggLoc = SUBAGG_IN_AGG;
			}
			else {
				context->info->aggLoc = SUBAGG_OUTSIDE;
			}

			return false;
		}

		return expression_tree_walker(node, getTargetSublinkTypeWalker,
				(void *) context);
	}

	// recurse
	return expression_tree_walker(node, getTargetSublinkTypeWalker,
			(void *) context);
}

/*
 *	Creates a new query node for an aggregation query that is just the execution
 *  of the aggregation, group by and having. The original query is stripped of
 *  aggregation, grouping and having and used as the input for the new query.
 *  This transformation is needed if a group by expression or input to an
 *  aggregation functions contains sublinks.
 *
 *	E.g. SELECT sum(i + (SELECT * FROM s)) FROM r
 *		 -->
 *		 SELECT sum(new) FROM (SELECT i + (SELECT * FROM s) AS new FROM r) AS sub;
 *
 */

static Query *
createNewBottomQueryForSublinks (Query *query, List *infos)
{
	ListCell *lc;
	TargetEntry *te;
	Node *expr;
	Query *newBottom;
	RangeTblRef *rtRef;
	List *inputsAndGroupBys;
	Index resno;

	/* create new Bottom query node */
	newBottom = makeQuery();
	newBottom->hasSubLinks = true;
	newBottom->targetList = query->targetList;
	newBottom->havingQual = query->havingQual;
	newBottom->groupClause = query->groupClause;
	newBottom->hasAggs = true;
	newBottom->hasSubLinks = false;

	/* get groupby expressions and aggregate functions inputs */
	inputsAndGroupBys = getGroupByExpr(query);
	inputsAndGroupBys = list_concat(inputsAndGroupBys, getAggFuncInputs(query));

	//TODO remove duplicate expressions
	/* create new target list of query */
	query->havingQual = NULL;
	query->groupClause = NIL;
	query->targetList = NIL;
	query->hasSubLinks = true;
	query->hasAggs = false;

	resno = 1;

	foreach(lc, inputsAndGroupBys)
	{
		expr = (Node *) lfirst(lc);

		te = makeTargetEntry ((Expr *) copyObject(expr), resno,
				appendIdToString("newAggExprTarget", &curUniqueAttrNum), false);

		query->targetList = lappend(query->targetList, te);
		resno++;
	}

	/* adapt target list, having and group by to refer to new target list entries */
	adaptNewBottomExprs (newBottom, inputsAndGroupBys);

	/* add query to newBottom range table */
	addSubqueryToRT (newBottom, query,
			appendIdToString(
					"outsourced_aggregation_inputs_and_group_by_expressions",
					&curUniqueRelNum));

	rtRef = makeNode(RangeTblRef);
	rtRef->rtindex = 1;
	newBottom->jointree->fromlist = list_make1(rtRef);

	correctSubQueryAlias(newBottom);

	/* log rewritten query */
	LOGNODE(newBottom,appendIdToString("new bottom for aggregation",
			&curUniqueRelNum));

	return newBottom;
}

/*
 * adapts the references to expressions used in newBottom target list entries with
 * simple Var nodes.
 */

static void
adaptNewBottomExprs (Query *query, List *exprs)
{
	ListCell *lc;
	TargetEntry *te;

	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);

		te->expr = (Expr *) replaceSubExprMutator((Node *) te->expr, exprs);
	}

	query->havingQual = replaceSubExprMutator(query->havingQual, exprs);
}

/*
 * Walks an expression tree and replaces expressions from list context with
 * Var nodes.
 */

static Node *
replaceSubExprMutator (Node *node, List *context)
{
	Index position;
	Var *newVar;
	Node *expr;

	if (node == NULL)
		return false;

	if (nodeInList(context, node))
	{
		position = nodePositionInList(context, node);
		expr = (Node *) list_nth(context, position);

		newVar = makeVar(1, position + 1, exprType(expr), exprTypmod(expr),0);	//OPTIMIZE compute types and typemods beforehand

		return (Node *) newVar;
	}

	if (IsA(node, Query))
	{
		return (Node *) query_tree_mutator((Query *) node,
				replaceSubExprMutator, (void *) context,
				QTW_IGNORE_JOINALIASES);
	}

	// recurse
	return expression_tree_mutator(node, replaceSubExprMutator,
			(void *) context);
}

/*
 * transforms an ASPJ query with sublinks in having clause.
 * a new Top query is created that is a projection on the aggregations result
 * attributes and the expressions from the having clause that are needed
 * for the join with the rewritten sublink query.
 */

static Query *
createNewTopQueryForSublinks (Query *query, List *infos)
{
	Query *newTop;
	RangeTblEntry *rte;
	RangeTblRef *rtRef;
	Alias *alias;
	List *aggsAndVars;
	List *mapAggsNewTargets;

	/* set provRewrite for aggregation query to false
	 * otherwise we would get problems with expandRTE
	 */
	//TODO this is a workaround should set a phase marker or something that deactivates the provenance expansion in expandRTE
	SetProvRewrite(query,false);

	/* create new top query */
	newTop = makeQuery();
	newTop->hasSubLinks = true;

	rtRef = makeNode(RangeTblRef);
	rtRef->rtindex = 1;
	newTop->jointree->fromlist = list_make1(rtRef);

	/* add range table entry for aggregation query */
	alias = makeAlias(appendIdToString("aggregation_query", &curUniqueRelNum),
			NIL);
	rte = addRangeTableEntryForSubquery(NULL, query, alias, true);

	newTop->rtable = list_make1(rte);

	/* copy target list and having qual */
	newTop->jointree->quals = query->havingQual;
	createNewTopTargetList(newTop, query);

	/* find aggregation functions and group by vars */
	aggsAndVars = getAggsAndVars (query);

	/* create new target list for the aggregation */
	mapAggsNewTargets = createTargetEntriesForNewAgg (query, aggsAndVars);

	/* replace aggregation functions and group by vars in new top target list
	 * and where qual */
	adaptNewTopExprs (newTop, aggsAndVars, mapAggsNewTargets);

	/* correct the alias of the aggregation query */
	correctSubQueryAlias(newTop);

	/* mark query so we don't rewrite it again and set having qual to null */
	SetSublinkRewritten(query,true);
	query->havingQual = NULL;

	LOGNODE(newTop, "new top for sublink having ");

	return newTop;
}

/*
 * Create new Top query target list. This is just a copy of the aggregation
 * target list, just that resjunc entries are skipped.
 */

static void
createNewTopTargetList (Query *newTop, Query *query)
{
	ListCell *lc;
	Index newResno;
	TargetEntry *te;
	TargetEntry *newTe;

	for (newResno = 1, lc = query->targetList->head; lc != NULL; lc = lc->next)
	{
		te = (TargetEntry *) lfirst(lc);

		// copy only non resjunc target entries
		if (te->resname)
		{
			newTe = makeTargetEntry(te->expr, newResno, strdup(te->resname),
					false);
			//newTe->ressortgroupref = te->ressortgroupref;
			//newTe->resorigcol = te->resorigcol;
			//newTe->resorigtbl = te->resorigtbl;

			newResno++;
			newTop->targetList = lappend(newTop->targetList, newTe);
		}
	}

}


/*
 * Create new target list of aggregation query after expressions on aggregate
 * functions or group by attrs have been outsourced to the new top query node.
 * For a single expression that is present more than once in aggsAndVars we
 * create only one target entry. A integer list that maps the expressions from
 * aggsAndVars and the target entries is returned.
 */

static List *
createTargetEntriesForNewAgg (Query *query, List *aggsAndVars)
{
	ListCell *lc;
	ListCell *innerLc;
	List *result;
	List *oldTargetList;
	TargetEntry *te;
	Var *var;
	GroupClause *groupBy;
	Node *node;
	Index resno;
	Index sortGroup;
	ReplaceParamsContext *context;
	int found;

	result = NIL;
	resno = 1;
	sortGroup = 1;

	/* backup query target list and start off with empty list */
	oldTargetList = query->targetList;
	query->targetList = NIL;
	query->groupClause = NIL;
	query->sortClause = NIL; 	//TODO adapt it instead of simply deletion

	/* create context for replaceParamsMutator */
	context = (ReplaceParamsContext *) palloc(sizeof(ReplaceParamsContext));
	context->touchParams = false;
	context->touchAggs = true;
	context->aggSublevelsUp = 0;
	context->varSublevelsUp = 0;
	context->addVarSublevelsUp = 0;
	context->useVarnoValue = 0;

	/*
	 * for each agg and var expression needed in the new Top query
	 * 		-if we have added a target entry for this expression before,
	 * 				then just update the mapping between expression and target
	 * 				 entries
	 * 		-else add a new target entry for the expression too
	 */
	foreach(lc, aggsAndVars)
	{
		node = (Node *) copyObject(lfirst(lc));	//CHECK copy notwendigig?

		/* set all varlevelsup to zero for all vars */
		node = replaceParamsMutator (node, context);

		/* scan target list for expression */
		found = 0;
		foreach(innerLc, query->targetList)
		{
			te = (TargetEntry *) lfirst(innerLc);

			if (equal(node, te->expr))
			{

				/*
				 * expression is aready present in target list. Just store the
				 * mapping, but don't add a new target entry.
				 */
				found = te->resno;
				result = lappend_int(result, found);
			}
		}

		/*
		 * Expression is not in target list. Add a new target entry for it.
		 */
		if (!found)
		{
			/* TODO scan if this expression was used by an old target entry. If this is the case
			 * copy the */

			te = makeTargetEntry((Expr *) node, resno,
					appendIdToString("newAggAttr", &curUniqueAttrNum) , false);

			/* is group by expr, then create groupClause */
			if (IsA(node, Var))
			{
				var = (Var *) node;

				te->ressortgroupref = sortGroup;

				groupBy = makeNode(GroupClause);
				groupBy->nulls_first = false;
				groupBy->tleSortGroupRef = sortGroup;
				groupBy->sortop = ordering_oper_opid(var->vartype);

				query->groupClause = lappend(query->groupClause, groupBy);

				sortGroup++;
			}

			query->targetList = lappend(query->targetList, te);
			result = lappend_int(result, resno);

			resno++;
		}
	}

	return result;
}

/*
 * searches for references to aggregate functions or group by vars in target
 * list and where clause of the new top node and replaces them by references
 * to the target list entries of the adapted aggregation query.
 */

static void
adaptNewTopExprs (Query *query, List *exprs, List *mapAggsNewTargets)
{
	ListCell *lc;
	TargetEntry *te;
	ReplaceSubExprTopMutatorContext *context;

	context = (ReplaceSubExprTopMutatorContext *)
			palloc(sizeof(ReplaceSubExprTopMutatorContext));
	context->mapExprTarget = mapAggsNewTargets;
	context->replaceExprs = exprs;
	context->nestDepth = 0;
	context->inSublink = false;
	context->justSeenSublink = false;

	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);

		context->nestDepth = 0;
		context->inSublink = false;
		context->justSeenSublink = false;

		te->expr = (Expr *) replaceSubExprTopMutator((Node *) te->expr,
				context);
	}

	context->nestDepth = 0;
	context->inSublink = false;
	context->justSeenSublink = false;
	query->jointree->quals = replaceSubExprTopMutator(query->jointree->quals,
			context);
}

/*
 * Walks an expression tree and replaces expressions from list context with
 * Var nodes.
 */

static Node *
replaceSubExprTopMutator (Node *node, ReplaceSubExprTopMutatorContext *context)
{
	Index position;
	Var *newVar;
	Node *expr;
	ReplaceSubExprTopMutatorContext *newContext;

	if (node == NULL)
		return false;

	/* if we are inside a sublink check for special case of an agg * expression
	 *  that can never be a correlated var */
	if (context->nestDepth > 0 && IsA(node, Aggref))
	{
		Aggref *agg;

		agg = (Aggref *) node;

		if(agg->aggstar)
			return expression_tree_mutator(node, replaceSubExprTopMutator,
					(void *) context);
	}

	if (nodeInList(context->replaceExprs, node))
	{
		if (context->nestDepth == 0
				|| checkVarLevelsUp(node, context->nestDepth))
		{
			position = nodePositionInList(context->replaceExprs, node);
			expr = (Node *) list_nth(context->replaceExprs, position);

			position = list_nth_int(context->mapExprTarget, position);
			newVar = makeVar(1, position, exprType(expr), exprTypmod(expr),
					context->nestDepth);	//OPTIMIZE compute types and typemods beforehand

			return (Node *) newVar;
		}
	}

	else if (IsA(node, Query))
	{
		/* only recurse into subqueries of sublinks */
		if (context->inSublink)
		{
			/* if we just saw a sublink we have to increase the nesting depth */
			if (context->justSeenSublink)
			{
				context->justSeenSublink = false;
				(context->nestDepth)++;
			}
			return (Node *) query_tree_mutator((Query *) node,
					replaceSubExprTopMutator, (void *) context,
					QTW_IGNORE_JOINALIASES);
		}

		return node;
	}

	// check for sublinks (we have to keep track of nesting depth
	else if (IsA(node, SubLink))
	{
		SubLink* sublink;

		sublink = (SubLink *) node;

		newContext = (ReplaceSubExprTopMutatorContext *)
				palloc(sizeof(ReplaceSubExprTopMutatorContext));
		newContext->nestDepth = context->nestDepth;
		newContext->mapExprTarget = context->mapExprTarget;
		newContext->replaceExprs = context->replaceExprs;
		newContext->justSeenSublink = true;
		newContext->inSublink = true;

		return expression_tree_mutator((Node *) sublink,
				replaceSubExprTopMutator, (void *) newContext);
	}

	// recurse
	return expression_tree_mutator(node, replaceSubExprTopMutator,
			(void *) context);
}

/*
 * Checks if all Var nodes contained in an expression have varlevelsup set
 * to nestDepth. This check is needed to deceide if we should replace an
 * expression in an sublink.
 */

static bool
checkVarLevelsUp(Node *node, int nestDepth)
{
	CheckVarLevelsWalkerContext *context;

	context = (CheckVarLevelsWalkerContext *)
			palloc(sizeof(CheckVarLevelsWalkerContext));
	context->result = true;
	context->nestDepth = nestDepth;

	checkVarLevelsUpWalker(node, context);

	return context->result;
}

/*
 *
 */

static bool
checkVarLevelsUpWalker (Node *node, CheckVarLevelsWalkerContext *context)
{
	if (node == NULL)
		return false;

	else if (IsA(node, Var))
	{
		Var *var;

		var = (Var *) node;

		if (var->varlevelsup != context->nestDepth)
			context->result = false;

		return false;
	}

	return expression_tree_walker(node, checkVarLevelsUpWalker,
			(void *) context);
}

/*
 * Returns all aggregation expressions and vars used in the target list and
 * HAVING clause of a query.
 */

static List *
getAggsAndVars (Query *query)
{
	List *result;
	ListCell *lc;
	TargetEntry *te;
	AggOutputAndGroupByWalkerContext *context;

	result = NIL;

	/* create Context */
	context = (AggOutputAndGroupByWalkerContext *)
			palloc(sizeof(AggOutputAndGroupByWalkerContext));
	context->inAgg = false;
	context->nestDepth = 0;
	context->result = &result;

	/* get aggs and vars in having */
	aggOutputAndGroupByWalker(query->havingQual, context);

	/* get aggs and vars in target list */
	foreach(lc, query->targetList)
	{
		context->inAgg = false;
		context->nestDepth = 0;
		context->result = &result;
		context->justSeenSublink = false;
		context->inSublink = false;

		te = (TargetEntry *) lfirst(lc);

		aggOutputAndGroupByWalker((Node *) te->expr, context);
	}

	//CHECK be carefull we only want to replace aggs that refer outer query

	return result;
}

/*
 * Returns a list of Aggrefs and Vars contained in an expression. Helper for
 * getAggsAndVars.
 */

static bool
aggOutputAndGroupByWalker (Node *node,
		AggOutputAndGroupByWalkerContext *context)
{
	AggOutputAndGroupByWalkerContext *newContext;

	if (node == NULL)
		return false;

	// check for Var nodes
	if (IsA(node, Var))
	{
		Var *var;

		var = (Var *) node;

		/* local vars in sublink cannot belong to the outer queries group by */
		if (var->varlevelsup >= context->nestDepth)
		{
			*(context->result) = lappend(*(context->result), var);
		}

		return false;
	}
	// check for aggrref nodes
	else if (IsA(node, Aggref))
	{
		Aggref *aggref;

		aggref = (Aggref *) node;

		if (context->nestDepth == 0)
		{
			*(context->result) = lappend(*(context->result), aggref);
		}
		/* for aggref inside a sublink they refer to the outer query is they
		 * contain sublevelsup vars */
		else
		{
			CorrelatedVarsWalkerContext *varContext;

			//OPTIMIZE aggvarlevelsup benutzen
			varContext = (CorrelatedVarsWalkerContext *)
					palloc(sizeof(CorrelatedVarsWalkerContext));
			varContext->result = false;
			varContext->varlevelsUp = context->nestDepth;

			hasCorrelatedVars (node, varContext);

			if (varContext->result)
			{
				*(context->result) = lappend(*(context->result), aggref);
			}
		}

		return false;
	}
	// check for query nodes (means we are recursing into a sublink)
	else if (IsA(node, Query))
	{
		/* only recurse into subqueries of sublinks */
		if (context->inSublink)
		{
			/* if we just saw a sublink we have to increase the nesting depth */
			if (context->justSeenSublink)
			{
				context->justSeenSublink = false;
				(context->nestDepth)++;
			}
			return query_tree_walker((Query *) node, aggOutputAndGroupByWalker,
					(void *) context, QTW_IGNORE_JOINALIASES);
		}

		return false;
	}
	// check for sublinks (we have to keep track of nesting depth
	else if (IsA(node, SubLink))
	{
		SubLink* sublink;

		sublink = (SubLink *) node;

		//expression_tree_walker(sublink->testexpr, aggOutputAndGroupByWalker, (void *) context);

		/* add new Context, but do not increase nestDepth jet, because we have
		 * to walk the testexpr first */
		newContext = (AggOutputAndGroupByWalkerContext *)
				palloc(sizeof(AggOutputAndGroupByWalkerContext));
		newContext->inAgg = context->inAgg;
		newContext->nestDepth = context->nestDepth;
		newContext->justSeenSublink = true;
		newContext->inSublink = true;
		newContext->result = context->result;

		return expression_tree_walker((Node *) sublink,
				aggOutputAndGroupByWalker, (void *) newContext);
	}

	// recurse
	return expression_tree_walker(node, aggOutputAndGroupByWalker,
			(void *) context);
}

/*
 * Replaces the Var and Aggref nodes in a former having qual with
 * var nodes of the new top query
 */

static Node *
replaceAggsAndGroupbyMutator (Node *node, List **context)
{
	if (node == NULL)
		return false;

	// replace Var nodes with new Var
	if (IsA(node, Var))
	{
		Var *newVar;

		newVar = (Var *) linitial(*context);
		*context = list_delete_first(*context);

		return (Node *) newVar;
	}

	// replace Aggref node with Var
	if (IsA(node, Aggref))
	{
		Var *newVar;

		newVar = (Var *) linitial(*context);
		*context = list_delete_first(*context);

		return (Node *) newVar;
	}

	// recurse
	return expression_tree_mutator(node, replaceAggsAndGroupbyMutator,
			(void *) context);
}

/*
 * Returns a list of the inputs for the aggregate expressions used in the
 * target list and HAVING clause of a Query node.
 */

static List *
getAggFuncInputs (Query *query)
{
	List *result;
	ListCell *lc;
	TargetEntry *te;

	result = NIL;

	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);

		aggInputWalker((Node *) te->expr, &result);
	}

	aggInputWalker(query->havingQual, &result);

	return result;
}

/*
 * Adds Aggrefs contained in an expression to the context list.
 */

static bool
aggInputWalker (Node *node, List **context)
{
	if (node == NULL)
		return false;

	// check for aggrref nodes
	if (IsA(node, Aggref))
	{
		Aggref *aggref;

		aggref = (Aggref *) node;
		*context = list_concat(*context, copyObject(aggref->args)); //CHECK if we should use lappend instead

		return false;
	}

	//CHECK sublinks can contain aggrefs we dont want to replace (no problem because no agg or group by refs in below sublinks

	// recurse
	return expression_tree_walker(node, aggInputWalker, (void *) context);
}
