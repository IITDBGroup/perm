/*-------------------------------------------------------------------------
 *
 * prov_sublink_totarget.c
 *	  	Moves sublinks from WHERE clause and other locations to the target list of a query.
 * 		This operation is provenance preserving. Some of the sublink rewrite methods can
 * 		produce more efficient results if sublinks are in the target list. (for example
 * 		the left join method). If the same sublink is used in different locations of a
 * 		query we can replace this sublink by a single target list sublink with
 * 		more than one reference. (We still have to rewrite this sublink more than once
 * 		to produce the correct provenance!)
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
#include "parser/parse_expr.h"
#include "parser/parse_oper.h"
#include "parser/parse_relation.h"
#include "nodes/nodes.h"
#include "nodes/makefuncs.h"
#include "nodes/primnodes.h"
#include "optimizer/clauses.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/provlog.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_sublink_leftjoin.h"
#include "provrewrite/prov_sublink.h"
#include "provrewrite/prov_spj.h"
#include "provrewrite/prov_sublink_totarget.h"

/* prototypes */
static void filterOutSublinkForRewrite (Query *query, List *sublinks, List **condTrue, List **condFalse);
static void isBelowLeftJoinWalker (Node *node, BelowLeftJoinContext *context, bool inOuterJoin);

static Query *moveSublinksToTargetList (Query *query, List *infos, List *correlated);
static void copyTargetListToNewTop (Query *newTop, Query *query, List *corrTarget, List *uncorrTarget, List **sublinksAndVars, List **map);
static TargetEntry *moveExpressionToTopTarget (Query *query, TargetEntry *te, Index curTopResno, List *corrTarget, List *uncorrTarget, List **sublinksAndVars, List **map);
static Node *createTopQueryQual (List *infos, Node *origQual);
static Node *getNormalConditions (Node *qual);
static List *getAndClauses (Node *qual);
static void addTargetEntriesForVarQualsAndSublinks (Query *query, List *uncorrWhere, List *uncorrTarget, List **sublinksAndVars, List **map);
static void addExpressionsToTargetList (List **expressions, List **map, Query *query);
static bool varAndSublinkWalker (Node *node, VarAndSublinkWalkerContext *context);

static void adaptReferences (Query *query, List *sublinksAndAggs, List *map);
static Node *adaptReferencesMutator (Node *node, AdaptReferencesMutatorContext *context);
static void adaptTestExpressions (List *sublinksAndAggs, List *map, List *sublinks);
static Node *adaptTestExpressionMutator (Node *node, AdaptTestExpressionMutatorContext *context);


static void rewriteTargetSublinks (Query *newTop, Query *query, List *uncorrelated, List *correlated, List *sublinks, Index subList[], List **rewritePos);
static List *rewriteNormalSubqueries (Query *query, List **subList, Index maxRTindex);
static void addProvenanceAttrsToNewTop (Query *newTop, List *sublinks, List *corrPstack, List *uncorrPstack, Index subList[]);
static void addProvenanceAttr (Query *query, TargetEntry *te, List **pList, Index rtindex, Index *resno);

/*
 *
 */

Query *
rewriteSublinkQueryWithMoveToTarget (Query *query, List *sublinks, List *uncorrelated, Index subList[], List **rewritePos)
{
	Query *newTop;
	List *correlatedSublinks;
	List *uncorrCondTrue;
	List *uncorrCondFalse;
	List *notUnnested;

	/* get all sublinks that have not been unnested */
	notUnnested = findSublinksUnnested (sublinks, PROV_SUBLINK_SEARCH_NOUNNEST);

	/* get correlated sublinks and check which uncorrelated sublinks can be savely moved to target list */
	correlatedSublinks = findSublinkByCats (notUnnested, PROV_SUBLINK_SEARCH_CORR | PROV_SUBLINK_SEARCH_CORR_IN);

	uncorrCondTrue = NIL;
	uncorrCondFalse = NIL;
	filterOutSublinkForRewrite (query, uncorrelated, &uncorrCondTrue, &uncorrCondFalse);

	/* create new top query node and move uncorrelated sublinks to target list of query */
	newTop = moveSublinksToTargetList (query, uncorrCondTrue, correlatedSublinks);

	/* rewriteSublinks */
	rewriteTargetSublinks(newTop, query, uncorrelated, correlatedSublinks, sublinks, subList, rewritePos);

	return newTop;
}

/*
 * Partitions the sublinks list into sublinks that can be moved to target list and those which can not. A
 * sublink cannot be moved to the target list if it used in the qualification of outer join or a join that
 * is below a outer join in the jointree.
 */

static void
filterOutSublinkForRewrite (Query *query, List *sublinks, List **condTrue, List **condFalse)
{
	ListCell *lc;
	SublinkInfo *info;
	BelowLeftJoinContext *context;

	/* for each sublink get the position of the join in the join tree where the sublink is used */
	foreach(lc, sublinks)
	{
		info = (SublinkInfo *) lfirst(lc);

		context = (BelowLeftJoinContext *) palloc(sizeof(BelowLeftJoinContext));
		context->sublink = info;
		context->result = false;

		isBelowLeftJoinWalker((Node *) query->jointree, context, false);

		if (context->result)
			*condFalse = lappend(*condFalse, info);
		else
			*condTrue = lappend(*condTrue, info);

		pfree(context);
	}
}

/*
 * 	checks if
 */

static void
isBelowLeftJoinWalker (Node *node, BelowLeftJoinContext *context, bool inOuterJoin)
{
	ListCell *lc;
	Node *sub;

	if (node == NULL)
		return;

	if (IsA(node, FromExpr))
	{
		FromExpr *from;

		from = (FromExpr *) node;

		if (equal(from->quals, context->sublink->exprRoot))
			context->result = false;
		else
		{
			foreach(lc, from->fromlist)
			{
				sub = (Node *) lfirst(lc);
				isBelowLeftJoinWalker (sub, context, false);
			}
		}
	}
	if (IsA(node, JoinExpr))
	{
		JoinExpr *joinExpr;
		bool outer;

		joinExpr = (JoinExpr *) node;
		outer = (joinExpr->jointype != JOIN_INNER) || inOuterJoin;

		/* is sublink in join qual? */
		if (equal(joinExpr->quals, context->sublink->exprRoot))
			context->result = outer;
		else
			isBelowLeftJoinWalker (joinExpr->larg, context, outer);
	}
}

/*
 * Creates a new top query node and moves the qualifications using sublinks to this query node.
 */

static Query *
moveSublinksToTargetList (Query *query, List *infos, List *correlated)//TODO adapt varlevelsup if we are rewritting a query in a sublink
{
	Query *newTop;
	List *sublinksAndVars;
	List *map;
	List *corrTarget;
	List *uncorrTarget;
	List *uncorrWhere;
	RangeTblRef *rtRef;

	sublinksAndVars = NIL;
	map = NIL;

	newTop = makeQuery();
	newTop->hasSubLinks = false;
	SetSublinkRewritten(newTop,true);

	corrTarget = getSublinkTypes(correlated, PROV_SUBLINK_SEARCH_SELECT);
	uncorrTarget = getSublinkTypes(infos, PROV_SUBLINK_SEARCH_SELECT);
	uncorrWhere = getSublinkTypes(list_union(infos,correlated), PROV_SUBLINK_SEARCH_WHERE);

	/* add query to RT of newTop */
	addSubqueryToRT (newTop, query, "origWithSublinks");

	MAKE_RTREF(rtRef, 1);
	newTop->jointree->fromlist = list_make1(rtRef);

	/* copy target list to new Top */
	copyTargetListToNewTop (newTop, query, corrTarget, uncorrTarget, &sublinksAndVars, &map);

	/* adapt target list of query (add references to attributes used in qual */
	addTargetEntriesForVarQualsAndSublinks(query, uncorrWhere, uncorrTarget, &sublinksAndVars, &map);

	/* copy qual to newTop and adapt vars and replace sublinks by references */
	newTop->jointree->quals = createTopQueryQual(infos, query->jointree->quals);
	adaptReferences(newTop, sublinksAndVars, map);

	/* adapt var references in the test expressions of the sublinks */
	adaptTestExpressions(sublinksAndVars, map, infos);

	/* correct the eref for the RTE of query */
	correctRTEAlias ((RangeTblEntry *) linitial(newTop->rtable));

	/* remove query quals */
	query->jointree->quals = NULL;

	return newTop;
}

/*
 * Creates the initial target list of the new top query node by adding references to the target list
 * entries of the original query.
 */

static void
copyTargetListToNewTop (Query *newTop, Query *query, List *corrTarget,
		List *uncorrTarget, List **sublinksAndVars, List **map)
{
	ListCell *lc;
	ListCell *innerLc;
	SublinkInfo *info;
	TargetEntry *te;
	TargetEntry *newTe;
	Index curResno;
	int i, numTEs;
	bool hasSublink;

	newTop->targetList = NIL;

	curResno = 1;
	numTEs = list_length(query->targetList);

	/* check if the query has any target list entries */
	if (!numTEs)
		return;

	for(lc = query->targetList->head, i = 0; i < numTEs; lc = lc->next, i++)
	{
		te = (TargetEntry *) lfirst(lc);
		hasSublink = false;

		/* do not copy resjunc entries */
		if (!te->resjunk)
		{
			/*
			 * if a target sublink is included in the expression of this target entry move the
			 * expression to the newTop target list and keep just the sublink in query target list
			 */
			foreach(innerLc, uncorrTarget)
			{
				info = (SublinkInfo *) lfirst(innerLc);

				/* is this sublink used in the target entry expression? */
				if (!hasSublink && equal(te->expr, info->exprRoot))
				{
					/* If expression is only the sublink then change nothing */
					if (!isTopLevelSublink(info))
					{
						newTe = moveExpressionToTopTarget(query, te, curResno, corrTarget, uncorrTarget, sublinksAndVars, map);
						newTop->targetList = lappend(newTop->targetList, newTe);
						hasSublink  = true;
					}
					else
						info->targetVar = makeVar(1, te->resno, exprType((Node *) te->expr), exprTypmod((Node *) te->expr), 0);
				}
			}

			if (!hasSublink)
			{
				newTe = copyObject(te);
				newTe->resno = curResno;
				newTe->expr = (Expr *) makeVar (1, curResno, exprType((Node *) te->expr), exprTypmod((Node *) te->expr), 0);

				newTop->targetList = lappend(newTop->targetList, newTe);
			}
			curResno++;
		}
	}
}


/*
 * For a target entry that is an expression over one or more sublinks move the expression to the top level target list
 * and add all uncorrelated sublinks to the target list of the original query (if they are not already available as
 * target list entries.
 */

static TargetEntry *
moveExpressionToTopTarget (Query *query, TargetEntry *te, Index curTopResno, List *corrTarget, List *uncorrTarget, List **sublinksAndVars, List **map)
{
	ListCell *lc;
	ListCell *infoLc;
	TargetEntry *newTe;
	Node *newExpr;
	List *replaceSublinks;
	List *replaceSublinkInfos;
	List *replaceVars;
	List *newVars;
	SublinkInfo *info;
	Var *newVar;
	Index curResno;
	SubLink *sublink;
	VarAndSublinkWalkerContext *varContext;
	AdaptTestExpressionMutatorContext *adaptContext;

	newVars = NIL;	//TODO check if we got the same problem with varlevelsup

	/*
	 * Get all uncorrelated sublinks used in te expression.
	 */
	replaceSublinks = NIL;
	replaceSublinkInfos = NIL;

	foreach(lc, uncorrTarget)
	{
		info = (SublinkInfo *) lfirst(lc);

		if (equal(te->expr, info->exprRoot))
		{
			replaceSublinks = lappend(replaceSublinks, info->sublink);
			replaceSublinkInfos = lappend(replaceSublinkInfos, info);
			*sublinksAndVars = lappend(*sublinksAndVars, info->sublink);
		}
	}

	/*
	 *  check if the expression contains a correlated sublink too, which means we have to get their correlated
	 *	Vars and add them to query target list and set varno correctly
	 */


	foreach(lc, corrTarget)
	{
		info = (SublinkInfo *) lfirst(lc);

		if (equal(te->expr, info->exprRoot))
		{
			replaceSublinks = lappend(replaceSublinks, info->sublink);
			replaceSublinkInfos = lappend(replaceSublinkInfos, info);
			*sublinksAndVars = lappend(*sublinksAndVars, info->sublink);
		}
	}

	/*
	 * get vars used in the expression
	 */
	replaceVars = NIL;

	varContext = (VarAndSublinkWalkerContext *) palloc(sizeof(VarAndSublinkWalkerContext));
	varContext->result = &replaceVars;
	varContext->maxVarlevelsUp = -1;

	varAndSublinkWalker ((Node *) te->expr, varContext);

	*sublinksAndVars = list_concat(*sublinksAndVars, replaceVars);

	curResno = list_length(query->targetList) + 1;

	/*
	 * if only a single uncorrelated sublink is used just adapt the te expression. Else add new
	 * target entries for the remainder of the sublinks too.
	 */
	newExpr = copyObject(te->expr);

	sublink = (SubLink *) linitial(replaceSublinks);
	info = (SublinkInfo *) linitial(replaceSublinkInfos);
	te->expr = (Expr *) copyObject(sublink);

	newVar = makeVar(1, te->resno, exprType((Node *) te->expr), exprTypmod((Node *) te->expr), 0);
	info->targetVar = newVar;
	newVars = lappend(newVars, newVar);

	curResno = list_length(query->targetList) + 1;

	for(lc = replaceSublinks->head->next, infoLc = replaceSublinkInfos->head->next; lc != NULL; lc = lc->next, infoLc = infoLc->next)
	{
		sublink = (SubLink *) lfirst(lc);
		info = (SublinkInfo *) lfirst(infoLc);

		newTe = makeTargetEntry((Expr *) sublink, curResno, "newExtractedTargetListSublink", false);
		newVar = makeVar(1, curResno, exprType((Node *) newTe->expr), exprTypmod((Node *) newTe->expr), 0);

		info->targetVar = newVar;
		newVars = lappend(newVars, newVar);

		query->targetList = lappend(query->targetList, newTe);

		curResno++;
	}

	addExpressionsToTargetList (sublinksAndVars, map, query);

	//TODO two replacements can lead to errors if a var replaced in the first run is falsely replaced in second run.

	/* replace vars in new expression by newVars */
	adaptContext = (AdaptTestExpressionMutatorContext *) palloc(sizeof(AdaptTestExpressionMutatorContext));
	adaptContext->sublinksAndVars = *sublinksAndVars;
	adaptContext->map = *map;

	newExpr = adaptTestExpressionMutator (newExpr, adaptContext);

	newTe = makeTargetEntry((Expr *) newExpr, curTopResno, te->resname, te->resjunk);

	return newTe;
}

/*
 * Creates the qualification of the newTopNode by ANDing all qual expr of the sublinks
 * that should be moved to target list.
 */

static Node *
createTopQueryQual (List *infos, Node *origQual)
{
	ListCell *lc;
	List *noTarget;
	List *exprSeen;
	SublinkInfo *info;
	Node *result;

	result = NULL;
	exprSeen = NIL;
	noTarget = getSublinkTypes(infos, PROV_SUBLINK_SEARCH_WHERE);

	result = getNormalConditions (origQual);

	/* AND all the rootexpr of all WHERE sublinks, but do not add a expression more than once. */
	foreach(lc, noTarget)
	{
		info = (SublinkInfo *) lfirst(lc);

		if (result != NULL && !list_member(exprSeen, info->exprRoot))
		{
			result = (Node *)  makeBoolExpr(AND_EXPR, list_make2(result, copyObject(info->exprRoot)));//CHECK have to copy?
			exprSeen = lappend(exprSeen, info->exprRoot);
		}
		else
		{
			result = copyObject(info->exprRoot);
			exprSeen = lappend(exprSeen, info->exprRoot);
		}
	}

	return result;
}

/*
 * Extracts conditions from the original query qual that do not contain sublinks. We know that
 * the qual is an and structure because otherwise we would not use the MOVE-strategy. Therefore
 * each subtree under a leaf AND can be handled separately.
 */

static Node *
getNormalConditions (Node *qual)
{
	List *clauses;
	ListCell *lc;
	Node *clause;
	List *result;
	List *sublinks;

	result = NIL;
	clauses = getAndClauses(qual);

	/* append each clause to result if it does not contain sublinks */
	foreach(lc, clauses)
	{
		clause = (Node *) lfirst(lc);
		sublinks = NIL;

		findExprSublinkWalker(clause, &sublinks);

		if(list_length(sublinks) == 0)
			result = lappend(result, copyObject(clause));
	}

	return createAndFromList(result);
}

/*
 * For a qual that is an ANDed condition return the list of subexpressions ANDed together by the qual
 */

static List *
getAndClauses (Node *qual)
{
	BoolExpr *and;
	List *result;
	ListCell *lc;
	Node *subQual;

	if (!qual)
		return NIL;

	if (IsA(qual, BoolExpr))
	{
		and = (BoolExpr *) qual;
		result = NIL;

		Assert(and->boolop == AND_EXPR);

		foreach(lc, and->args)
		{
			subQual = (Node *) lfirst(lc);

			result = list_concat(result, getAndClauses(subQual));
		}

		return result;
	}

	return list_make1(qual);
}



/*
 * Adds target entries for vars used in qual and for sublinks from WHERE clause.
 */

static void
addTargetEntriesForVarQualsAndSublinks (Query *query, List *uncorrWhere, List *uncorrTarget, List **sublinksAndVars, List **map)
{
	ListCell *lc;
	VarAndSublinkWalkerContext *context;
	Index curResno;
	SublinkInfo *info;
	TargetEntry *te;
	Var *var;

	context = (VarAndSublinkWalkerContext *) palloc(sizeof(VarAndSublinkWalkerContext));
	context->infos = uncorrWhere;
	context->result = sublinksAndVars;
	context->maxVarlevelsUp = 0; 		// don't add target entries for correlated Vars

	varAndSublinkWalker ((Node *) query->jointree, context);
	varAndSublinkWalker ((Node *) uncorrTarget, context);

	addExpressionsToTargetList (sublinksAndVars, map, query);

	curResno = list_length(query->targetList) + 1;

	/* add new target list entries for sublinks */
	foreach(lc, uncorrWhere)
	{
		info = (SublinkInfo *) lfirst(lc);

		te = makeTargetEntry((Expr *) copyObject(info->sublink), curResno, "newSublinkAttr",false);
		var = makeVar(1,curResno, exprType((Node *) info->sublink), exprTypmod((Node *) info->sublink), 0);

		info->targetVar = var;
		*sublinksAndVars = lappend(*sublinksAndVars, info->sublink);
		*map = lappend_int(*map, curResno);

		query->targetList = lappend(query->targetList, te);
		curResno++;
	}

}

/*
 * adds all expressions from "expressions" to the target list of query and stores their target list position in "map".
 * No target entry is added for expressions that are already in the target list, but their target list position is stored in "map".
 */

static void
addExpressionsToTargetList (List **expressions, List **map, Query *query)
{
	ListCell *lc;
	ListCell *innerLc;
	TargetEntry *te;
	Var *var;
	bool found;
	Index curResno;
	int listPos;

	curResno = list_length(query->targetList) + 1;

	/* for each var used in qual add a new target entry if none for this var exists */
	listPos = 0;

	foreach(lc, *expressions)
	{
		listPos++;
		var = (Var *) lfirst(lc);

		found = false;

		foreach(innerLc, query->targetList)
		{
			te = (TargetEntry *) lfirst(innerLc);

			/* if expression is already a target entry just add an entry in map for the expression */
			if (equal(te->expr, var))
			{
				found = true;

				/*
				 * check if we have set this map entry before (this is the case if length of the map list is >= te->resno)
				 * If not than append the target list position for the current expression to the map list.
				 */
				if (list_length(*map) < listPos)
				{
					*map = lappend_int(*map, te->resno);
				}
			}
		}

		/*
		 * expression is not a target entry. add a new target entry for the expression
		 * and add an entry to map for the expression
		 */
		if (!found)
		{
			te = makeTargetEntry (copyObject(var), curResno, "newSublinkVar", false);

			query->targetList = lappend(query->targetList, te);
			*map = lappend_int(*map, curResno);

			curResno++;
		}
	}
}


/*
 *  Finds all vars used in an expression. This walker does not recurse into subqueries or
 *  the queries of sublinks.
 */

static bool
varAndSublinkWalker (Node *node, VarAndSublinkWalkerContext *context)
{
	if (node == NULL)
		return false;

	// check for Var nodes
	if (IsA(node, Var))
	{
		Var *var;

		var = (Var *) node;

		if (context->maxVarlevelsUp == -1 || var->varlevelsup <= context->maxVarlevelsUp)
			*(context->result) = lappend(*(context->result), var);

		return false;
	}
	else if (IsA(node, SubLink))
	{
		SubLink *sublink;

		sublink = (SubLink *) node;

		return expression_tree_walker(sublink->testexpr, varAndSublinkWalker, (void *) context);
	}

	// recurse
	return expression_tree_walker(node, varAndSublinkWalker, (void *) context);
}

/*
 * Adapt the expressions of the new top level query node's target list. Replace vars and sublinks with references to the
 * target entries of the modified original query.
 */

static void
adaptReferences (Query *query, List *sublinksAndAggs, List *map)
{
	AdaptReferencesMutatorContext *context;

	context = (AdaptReferencesMutatorContext *) palloc(sizeof(AdaptReferencesMutatorContext));
	context->map = map;
	context->sublinksAndVars = sublinksAndAggs;

	query->jointree->quals = adaptReferencesMutator (query->jointree->quals, context);
}

/*
 * Replaces all occurrences from sublinksAndVars by a Var node with varattno defined by map.
 */

static Node *
adaptReferencesMutator (Node *node, AdaptReferencesMutatorContext *context)
{
	ListCell *lc;
	ListCell *mapLc;
	Node *replaceNode;

	if (node == NULL)
		return NULL;

	if (IsA(node, Var) || IsA(node, SubLink))
	{
		forboth(lc, context->sublinksAndVars, mapLc, context->map)
		{
			replaceNode = (Node *) lfirst(lc);

			if (equal(node, replaceNode))
				return  (Node *) makeVar(1, lfirst_int(mapLc), exprType(node), exprTypmod(node), 0);
		}

		return node;
	}

	return expression_tree_mutator(node, adaptReferencesMutator, (void *) context);
}

/*
 * Replace Vars and sublinks in the testexpression of a sublink with the elements from map.
 */

static void
adaptTestExpressions (List *sublinksAndAggs, List *map, List *sublinks)
{
	AdaptTestExpressionMutatorContext *context;
	ListCell *lc;
	SubLink *sublink;

	context = (AdaptTestExpressionMutatorContext *) palloc(sizeof(AdaptTestExpressionMutatorContext));
	context->map = map;
	context->sublinksAndVars = sublinksAndAggs;

	foreach(lc, sublinks)
	{
		sublink = ((SublinkInfo *) lfirst(lc))->sublink;
		sublink->testexpr = adaptTestExpressionMutator (sublink->testexpr, context);
	}

}

/*
 * Mutator that replaces expressions from sublinksAndVars (context) with var entries with varattno set to the values defined in map. map
 * is an integer list that stores at position x the varattno value of the expression from position x in sublinksAndVars.
 */

static Node *
adaptTestExpressionMutator (Node *node, AdaptTestExpressionMutatorContext *context)
{
	ListCell *lc;
	ListCell *mapLc;
	Node *replaceNode;

	if (node == NULL)
		return NULL;

	if (IsA(node, Var) || IsA(node, SubLink))
	{
		forboth(lc, context->sublinksAndVars, mapLc, context->map)
		{
			replaceNode = (Node *) lfirst(lc);

			if (equal(node, replaceNode))
			{
				return  (Node *) makeVar(1, lfirst_int(mapLc), exprType(node), exprTypmod(node), 0);
			}
		}

		return node;
	}

	return expression_tree_mutator(node, adaptTestExpressionMutator, (void *) context);
}

/*
 * Rewrite the sublinks of newTop and query.
 */

static void
rewriteTargetSublinks (Query *newTop, Query *query, List *uncorrelated, List *correlated,
						List *sublinks, Index subPos[], List **rewritePos)
{
	ListCell *lc;
	SublinkInfo *info;
	List *belowPList;
	List *belowPStack;
	List *belowSubList;
	List *bothPstack;
	List *topPstack;
	List *subList;
	Index maxRTindex;

	belowPStack = NIL;
	topPstack = NIL;
	belowPList = NIL;
	subList = NIL;
	belowSubList = NIL;

	maxRTindex = list_length(query->rtable);	//TODO get from subPos

	/* rewrite all sublinks (except unnested ones) */
	foreach(lc, sublinks)
	{
		info = (SublinkInfo *) lfirst(lc);

		if (!info->unnested)
		{
			if (info->category == SUBCAT_UNCORRELATED)
			{
				rewriteTargetSublinkUsingLeftJoin(newTop, query, info, subPos);
				*rewritePos = lappend_int(*rewritePos, info->sublinkPos);
			}
			else
				rewriteSublink(query, info, subPos, rewritePos);
		}
	}

	/* reorder pStack for the sublink's pLists according to the sublinkPos of the sublinks */
	createSubList(subPos, &subList, *rewritePos);
	bothPstack = popListAndReverse(&pStack, list_length(sublinks));
	sortSublinkInfos(&sublinks);

	/* create a separate pStack for the new top level query and the original query below it */
	foreach(lc, sublinks)
	{
		info = (SublinkInfo *) lfirst(lc);
		if (info->category == SUBCAT_UNCORRELATED && !info->unnested)
			topPstack = lappend(topPstack, list_nth(bothPstack, info->sublinkPos));
		else
		{
			belowPStack = lappend(belowPStack, list_nth(bothPstack, info->sublinkPos));
			belowSubList = lappend_int(belowSubList, subPos[info->sublinkPos]);
			push(&pStack, list_nth(bothPstack, info->sublinkPos));
		}
	}

	/* rewrite the modified original query */
	belowPList = rewriteNormalSubqueries(query, &belowSubList, maxRTindex);

	correctRTEAlias((RangeTblEntry *) linitial(newTop->rtable));

	/* correct provenance attributes */
	addProvenanceAttrsToNewTop (newTop, sublinks, belowPStack, topPstack, subPos);
}

/*
 * Rewrites the "normal" subqueries of the modified original query.
 */

static List *
rewriteNormalSubqueries (Query *query, List **subList, Index maxRTindex)
{
	rewriteSPJrestrict(query, subList, maxRTindex);

	return (List *) linitial(pStack);
}

/*
 * Adds the provenance attributes of query and the sublinks to the new top query.
 */

static void
addProvenanceAttrsToNewTop (Query *newTop, List *sublinks, List *corrPstack, List *uncorrPstack, Index subList[])
{
	ListCell *lc;
	ListCell *innerLc;
	ListCell *curPos;
	SublinkInfo *info;
	List *pList;
	List *subPlist;
	List *curPlist;
	Index curPListLength;
	Index curResno;
	Index rtindex;
	Index pListPos;
	Index subPos;
	TargetEntry *te;
	int i;

	/* init stuff */
	curResno = list_length(newTop->targetList) + 1;
	pListPos = 0;
	pList = NIL;

	/* the sublist the original query (below) is on the top of Pstack */
	subPlist = (List *) pop(&pStack);
	curPos = (list_length(subPlist) > 0) ? subPlist->head : NULL;

	/* add provenance attributes for subqueries */
	foreach(lc, sublinks)
	{
		info = (SublinkInfo *) lfirst(lc);
		subPos = subList[info->sublinkPos];

		/*
		 * Sublink is an uncorrelated sublink use attrs from uncorrPstack.
		 */
		if (info->category == SUBCAT_UNCORRELATED && !info->unnested)
		{
			curPlist = (List *) pop(&uncorrPstack);
			rtindex = subPos;

			foreach(innerLc, curPlist)
			{
				te = (TargetEntry *) lfirst(innerLc);

				addProvenanceAttr(newTop, te, &pList, rtindex, &curResno);
			}
		}
		/*
		 * if sublink is an correlated sublink get provenance attributes from query. The number
		 * of attributes to copy is determined by the corrPstack entry for the correlated sublink.
		 */
		else
		{
			curPListLength = ((List *) pop(&corrPstack))->length;

			for(i = 0; i < curPListLength; i++, curPos = curPos->next)
			{
				te = (TargetEntry *) lfirst(curPos);

				addProvenanceAttr(newTop, te, &pList, 1, &curResno);
			}
		}
	}

	/* add provenance attributes of normal query */
	for(; curPos != NULL; curPos = curPos->next)
	{
		te = (TargetEntry *) lfirst(curPos);

		addProvenanceAttr(newTop, te, &pList, 1, &curResno);
	}

	/* push pList on pStack */
	push(&pStack, pList);
}

/*
 * adds a provenance attribute to target list of query and pList and increases resno.
 */

static void
addProvenanceAttr (Query *query, TargetEntry *te, List **pList, Index rtindex, Index *resno)
{
	TargetEntry *newTe;
	Expr *expr;


	/* create new TE */
	expr = (Expr *) makeVar (rtindex,
				te->resno,
				exprType ((Node *) te->expr),
				exprTypmod ((Node *) te->expr),
				0);
	newTe = makeTargetEntry(expr, *resno, te->resname, false);

	/* append to targetList and pList */
	query->targetList = lappend (query->targetList, newTe);
	*pList = lappend (*pList, newTe);

	/* increase current resno */
	(*resno)++;
}
