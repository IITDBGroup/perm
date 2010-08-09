/*-------------------------------------------------------------------------
 *
 * prov_sublink_util_search.c
 *	  PERM C 
 *
 * Portions Copyright (c) 2010 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $Perm: pgsql//perm/src/backend/provrewrite/sublink/prov_sublink_util_search.c,v 1.542 Mar 31, 2010 bglav Exp $
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
#include "nodes/print.h"                                // pretty print node (trees)
#include "optimizer/clauses.h"
#include "parser/parse_relation.h"
#include "parser/parse_expr.h"
#include "parser/parse_oper.h"
#include "parser/parsetree.h"
#include "utils/guc.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/provstack.h"
#include "provrewrite/provlog.h"
#include "provrewrite/prov_sublink_unnest.h"
#include "provrewrite/prov_sublink_unn.h"
#include "provrewrite/prov_sublink_util_search.h"

/* Global variables */

static SublinkLocation defaultLoc;

/* Static methods */
static List *extractJoinExprSublinks(JoinExpr *joinExpr);
static bool findSublevelsUpVarWalker(Node *node,
		GetCorrelatedVarsWalkerContext *context);
static bool findCorrelatedVarsQueryTreeWalker(Query *query,
		GetCorrelatedVarsWalkerContext *context);
static bool findExprVarWalker(Node *node, List **context);
static SublinkInfo * makeSublinkInfo(SubLink *sublink, Node *rootExpr,
		Node **sublinkExprPointer, Node **grandParentRef, Node *parent);
static bool findLinkToExprWalker(Node *node,
		FindLinkToExprWalkerContext *context);
static List *getChildSublinksWalker(Query *query);

/*
 *
 */

List *
getSublinkBaseRelations(Query *query)
{
	List *result;

	result = findBaseRelationsForProvenanceQuery(query);

	return result;
}

/*
 * expression walker used to find sublinks. All sublinks that are found are
 * appended to the context list.
 */

bool
findExprSublinkWalker(Node *node, List **context)
{
	if (node == NULL)
		return false;

	// check for Sublink nodes
	if (IsA(node, SubLink))
	{
		SubLink *sublink;

		sublink = (SubLink *) node;
		*context = lappend(*context, sublink);

		/* check if the sublink test expression contains a sublink */
		if (sublink->testexpr != NULL)
			return expression_tree_walker((Node *) sublink->testexpr,
					findExprSublinkWalker, (void *) context);

		return false;
	}

	// recurse
	return expression_tree_walker(node, findExprSublinkWalker, (void *) context);
}

/*
 * Searches a query node for sublinks (in targetList, joinExpr, groupClause) and
 * returns a list of sublinkinfos for these sublinks. The flags define which
 * possible locations should be considered. (See prov_sublink_util.h)
 */

List *
findSublinkLocations(Query *query, int flags)
{
	ListCell *lc;
	Node *node;
	TargetEntry *te;
	List *groupByTes;
	List *result;

	result = NIL;
	/* scan SELECT clause for sublinks */
	if (flags & PROV_SUBLINK_SEARCH_SELECT)
	{
		defaultLoc = SUBLOC_SELECT;
		foreach(lc, query->targetList)
		{
			te = (TargetEntry *) lfirst(lc);

			/* check that target entry is not used in group by, because group
			 * by is handled below */
			if (!isUsedInGroupBy(query, te))
			{
				node = (Node *) te->expr;
				result = list_concat(result, findSublinksForExpr(node));
			}
		}
	}

	/* scan WHERE clause */
	if (flags & PROV_SUBLINK_SEARCH_WHERE)
	{
		defaultLoc = SUBLOC_WHERE;
		node = query->jointree->quals;
		result = list_concat(result, findSublinksForExpr(node));

		foreach(lc, query->jointree->fromlist)
		{
			node = (Node *) lfirst(lc);
			if (IsA(node, JoinExpr))
			{
				result = list_concat(result, extractJoinExprSublinks(
						((JoinExpr *) node)));
			}
		}
	}

	/* scan GROUP BY clause */
	if (flags & PROV_SUBLINK_SEARCH_GROUPBY)
	{
		defaultLoc = SUBLOC_GROUPBY;
		groupByTes = getGroupByTLEs(query);

		foreach(lc, groupByTes)
		{
			node = (Node *) ((TargetEntry *) lfirst(lc))->expr;

			result = list_concat(result, findSublinksForExpr(node));
		}
	}

	/* scan HAVING clause */
	if (flags & PROV_SUBLINK_SEARCH_HAVING)
	{
		defaultLoc = SUBLOC_HAVING;
		result = list_concat(result, findSublinksForExpr(query->havingQual));
	}

	/* scan ORDER BY clause */
	if (flags & PROV_SUBLINK_SEARCH_ORDER)
	{
		defaultLoc = SUBLOC_ORDER;
		//TODO result = list_concat(result, findSublinksForExpr(query->sortClause));
	}

	return result;
}

/*
 *
 */

static List *
extractJoinExprSublinks(JoinExpr *joinExpr)
{
	List *result;

	result = NIL;
	if (joinExpr == NULL)
	{
		return NIL;
	}
	result = findSublinksForExpr(joinExpr->quals);
	if (IsA(joinExpr->larg, JoinExpr))
	{
		result = list_concat(result, extractJoinExprSublinks(
				(JoinExpr *) joinExpr->larg));
	}
	if (IsA(joinExpr->rarg, JoinExpr))
	{
		result = list_concat(result, extractJoinExprSublinks(
				(JoinExpr *) joinExpr->rarg));
	}
	return result;
}

/*
 * Returns a trimmed list of sublink infos that contains only sublinks with the
 * category indicated by the flags.
 */

List *
findSublinkByCats(List *sublinks, int flags)
{
	ListCell *lc;
	List *result;
	SublinkInfo *info;

	result = NIL;

	foreach(lc, sublinks)
	{
		info = (SublinkInfo *) lfirst(lc);

		switch (info->category)
		{
		case SUBCAT_UNCORRELATED:
			if (flags & PROV_SUBLINK_SEARCH_UNCORR)
				result = lappend(result, info);
			break;
		case SUBCAT_CORRELATED:
			if (flags & PROV_SUBLINK_SEARCH_CORR)
				result = lappend(result, info);
			break;
		case SUBCAT_CORRELATED_INSIDE:
			if (flags & PROV_SUBLINK_SEARCH_CORR_IN)
				result = lappend(result, info);
			break;
		default:
			//TODO error
			break;
		}
	}

	return result;
}

/*
 * Searches for SublinkInfo in a list according to their "unnest" field value.
 */

List *
findSublinksUnnested(List *infos, int flags)
{
	ListCell *lc;
	List *result;
	SublinkInfo *info;

	result = NIL;

	foreach(lc, infos)
	{
		info = (SublinkInfo *) lfirst(lc);

		if (info->unnested && (flags & PROV_SUBLINK_SEARCH_UNNEST))
			result = lappend(result, info);
		else if (!info->unnested && (flags & PROV_SUBLINK_SEARCH_NOUNNEST))
			result = lappend(result, info);
	}

	return result;
}

/*
 * Scans a sublink query for expressions using correlated attributes
 */

void
findVarsSublevelsUp(Query *query, SublinkInfo *info)
{
	GetCorrelatedVarsWalkerContext *context;

	context = (GetCorrelatedVarsWalkerContext *)
			palloc(sizeof(GetCorrelatedVarsWalkerContext));
	context->correlated = &(info->corrVarInfos);
	context->parent = info->sublink->subselect;
	context->varlevelsUp = 0;
	context->realLevelsUp = list_make1_int(0);
	context->belowAgg = query->hasAggs;
	context->belowSet = !(query->setOperations == NULL);

	findCorrelatedVarsQueryTreeWalker(query, context);
}

/*
 * Walks an expression tree and builds a list of the correlated var nodes used in
 * the expression.
 */

static bool
findSublevelsUpVarWalker(Node *node, GetCorrelatedVarsWalkerContext *context)
{
	CorrVarInfo *corrVarInfo;
	GetCorrelatedVarsWalkerContext *newContext;

	if (node == NULL)
		return false;

	newContext = (GetCorrelatedVarsWalkerContext *)
			palloc(sizeof(GetCorrelatedVarsWalkerContext));
	newContext->correlated = context->correlated;
	newContext->varlevelsUp = context->varlevelsUp;
	newContext->exprRoot = context->exprRoot;
	newContext->location = context->location;
	newContext->parent = node;
	newContext->realLevelsUp = list_copy(context->realLevelsUp);

	// check for var nodes with sublevelsup
	if (IsA(node, Var))
	{
		Var *var;
		List *vars;
		int realVarLevelsUp;
		ListCell *lc;

		var = (Var *) node;

		if (var->varlevelsup > 0)
		{
			// check how many sublinks we are referencing upwards
			foreachi(lc, realVarLevelsUp, context->realLevelsUp)
			{
				if (context->varlevelsUp - ((signed int) var->varlevelsup)
						>= lfirst_int(lc))
					break;
			}
//			realVarLevelsUp++;

			vars = NIL;//TODO
			corrVarInfo = makeCorrVar(var, vars, context->parent,
					context->exprRoot, context->location,
					var->varlevelsup >= context->varlevelsUp, NULL,
					context->belowAgg, context->belowSet, realVarLevelsUp);
			*(newContext->correlated) = lappend(*(newContext->correlated),
					corrVarInfo);
		}

		return false;
	}
	// we are entering a new sublink, record the current varlevels up
	// to be able to identify which sublink a
	else if (IsA(node, SubLink))
	{
		newContext->realLevelsUp = lcons_int(newContext->varlevelsUp + 1,
				newContext->realLevelsUp);
	}
	// A query node. Increase varlevels up counter and check if
	// we are entering an aggregation or set operation stmt
	else if (IsA(node,Query))
	{
		Query *newQuery = (Query *) node;
		newContext->varlevelsUp++;
		newContext->belowAgg = context->belowAgg || newQuery->hasAggs;
		newContext->belowSet = context->belowSet || newQuery->setOperations;

		return findCorrelatedVarsQueryTreeWalker(newQuery, newContext);
	}

	// recurse
	return expression_tree_walker(node, findSublevelsUpVarWalker,
			(void *) newContext);
}

/*
 *
 */

static bool
findCorrelatedVarsQueryTreeWalker(Query *query,
		GetCorrelatedVarsWalkerContext *context)
{
	ListCell *lc;
	Node *node;
	List *groupByTes;

	/* walk range table */
	if (range_table_walker(query->rtable, findSublevelsUpVarWalker,
			(void *) context, QTW_IGNORE_JOINALIASES))
		return true;

	/* walk target list */
	context->location = SUBLOC_SELECT;
	foreach(lc, query->targetList)
	{
		node = (Node *) lfirst(lc);
		context->exprRoot = node;

		if (!isUsedInGroupBy(query, (TargetEntry *) node))
		{
			if (findSublevelsUpVarWalker(node, context))
				return true;
		}
	}

	/* walk group by */
	context->location = SUBLOC_GROUPBY;
	groupByTes = getGroupByTLEs(query);
	foreach(lc, groupByTes)
	{
		node = (Node *) lfirst(lc);
		context->exprRoot = node;

		if (findSublevelsUpVarWalker(node, context))
			return true;
	}

	/* walk WHERE */
	context->location = SUBLOC_WHERE;
	context->exprRoot = (Node *) query->jointree;
	if (findSublevelsUpVarWalker((Node *) query->jointree, context))
		return true;

	/* walk HAVING */
	context->location = SUBLOC_HAVING;
	context->exprRoot = query->havingQual;
	if (findSublevelsUpVarWalker(query->havingQual, context))
		return true;

	//      context->location = SUBLOC_ORDER;       //TODO siehe group by
	//      foreach(lc, query->sortClause)
	//      {
	//              node = (Node *) lfirst(lc);
	//              context->exprRoot = node;
	//
	//              if (findSublevelsUpVarWalker(node, context))
	//                      return true;
	//      }

	return false;
}

/*
 * find Var nodes used in an expression and return them as a List.
 */

List *
findExprVars(Node *node)
{
	List *result;

	result = NIL;
	findExprVarWalker(node, &result);
	return result;
}

/*
 * expression walker used by findExprVars
 */

static bool
findExprVarWalker(Node *node, List **context)
{
	if (node == NULL)
		return false;

	// check for Var nodes
	if (IsA(node, Var))
	{
		Var *var;

		var = (Var *) node;
		*context = lappend(*context, var);

		return false;
	}

	// recurse
	return expression_tree_walker(node, findExprVarWalker, (void *) context);
}

/*
 * returns a List of sublink infos for all sublinks used in expr
 */

List *
findSublinksForExpr(Node *expr)
{
	ListCell *lc;
	List *sublinks;
	List *result;
	Node *copyExpr;
	Node *copyExpr2;
	SublinkInfo *newInfo;
	SublinkInfo *newInfo2;
	SubLink *sublink;

	result = NIL;
	sublinks = NIL;

	/* find sublinks used in epxr */
	findExprSublinkWalker(expr, &sublinks);

	/* for each sublink copy the expr and find pointer to sublink and parent node */
	foreach(lc, sublinks)
	{
		sublink = (SubLink *) lfirst(lc);
		copyExpr = copyObject(expr);
		copyExpr2 = copyObject(expr);

		newInfo = extractSublinks(copyExpr, copyExpr, &copyExpr2, NULL,
				sublink, NULL);
		newInfo2 = extractSublinks(copyExpr2, copyExpr2, &copyExpr2, NULL,
				sublink, NULL);
		newInfo->rootCopy = newInfo2->exprRoot;

		result = lappend(result, newInfo);
	}

	return result;
}

/*
 * Searches for Sublinks inside an expression and returns a list of SublinkInfos
 * for all Sublinks found.
 */

SublinkInfo *
extractSublinks(Node *node, Node *exprRoot, Node **parentRef,
		Node **grandParentRef, SubLink* sublink, Node *parent)
{
	ListCell *lc;
	SublinkInfo *info;

	if (node == NULL)
	{
		return NULL;
	}
	switch (node->type)
	{
	case T_SubLink:
	{
		SubLink *foundSublink;

		foundSublink = (SubLink *) node;

		if (equal(sublink, foundSublink))
		{
			info = makeSublinkInfo(foundSublink, exprRoot, parentRef,
					grandParentRef, parent);
			return info;
		}

		info = extractSublinks(foundSublink->testexpr, exprRoot,
				&(foundSublink->testexpr), parentRef, sublink, node);

		if (info)
			return info;
	}
		break;
	case T_A_Expr:
	{
		A_Expr *aExpr;

		aExpr = (A_Expr *) node;

		info = extractSublinks(aExpr->lexpr, exprRoot, &(aExpr->lexpr),
				parentRef, sublink, node);

		if (info)
			return info;

		info = extractSublinks(aExpr->rexpr, exprRoot, &(aExpr->rexpr),
				parentRef, sublink, node);

		if (info)
			return info;

		return NULL;
	}
		break;
	case T_TypeCast:
	{
		TypeCast *typeCast;

		typeCast = (TypeCast *) node;

		return extractSublinks(typeCast->arg, exprRoot, &(typeCast->arg),
				parentRef, sublink, node);
	}
		break;
	case T_A_Indirection:
	{
		A_Indirection *aInd;

		aInd = (A_Indirection *) node;

		return extractSublinks(aInd->arg, exprRoot, &(aInd->arg), parentRef,
				sublink, node);
	}
		break;
	case T_RowExpr:
	{
		RowExpr *rowExpr;

		rowExpr = (RowExpr *) node;

		foreach(lc, rowExpr->args)
		{
			info = extractSublinks((Node *) lfirst(lc), exprRoot,
					(Node **) &(lc->data.ptr_value), parentRef, sublink, node);
			if (info)
				return info;
		}
	}
		break;
	case T_CoalesceExpr:
	{
		CoalesceExpr *colExpr;

		colExpr = (CoalesceExpr *) node;

		foreach(lc, colExpr->args)
		{
			info = extractSublinks((Node *) lfirst(lc), exprRoot,
					(Node **) &(lc->data.ptr_value), parentRef, sublink, node);

			if (info)
				return info;
		}
	}
		break;
	case T_MinMaxExpr:
	{
		MinMaxExpr *mmExpr;

		mmExpr = (MinMaxExpr *) node;

		foreach(lc, mmExpr->args)
		{
			info = extractSublinks((Node *) lfirst(lc), exprRoot,
					(Node **) &(lc->data.ptr_value), parentRef, sublink, node);

			if (info)
				return info;
		}
	}
		break;
	case T_XmlExpr:
	{
		XmlExpr *xmlExpr;

		xmlExpr = (XmlExpr *) node;
		foreach(lc, xmlExpr->args)
		{
			info = extractSublinks((Node *) lfirst(lc), exprRoot,
					(Node **) &(lc->data.ptr_value), parentRef, sublink, node);

			if (info)
				return info;
		}
		foreach(lc, xmlExpr->named_args)
		{
			info = extractSublinks((Node *) lfirst(lc), exprRoot,
					(Node **) &(lc->data.ptr_value), parentRef, sublink, node);

			if (info)
				return info;
		}
	}
		break;
	case T_XmlSerialize:
	{
		XmlSerialize *xmlSer;

		xmlSer = (XmlSerialize *) node;

		return extractSublinks(xmlSer->expr, exprRoot, &(xmlSer->expr),
				parentRef, sublink, node);
	}
		break;
	case T_NullTest:
	{
		NullTest *nullTest;

		nullTest = (NullTest *) node;

		return extractSublinks((Node *) nullTest->arg, exprRoot,
				(Node **) &(nullTest->arg), parentRef, sublink, node);
	}
		break;
	case T_BooleanTest:
	{
		BooleanTest *boolTest;

		boolTest = (BooleanTest *) node;

		return extractSublinks((Node *) boolTest->arg, exprRoot,
				(Node **) &(boolTest->arg), parentRef, sublink, node);
	}
		break;
	case T_Aggref:
	{
		Aggref *aggref;

		aggref = (Aggref *) node;
		foreach(lc, aggref->args)
		{
			info = extractSublinks((Node *) lfirst(lc), exprRoot,
					(Node **) &(lc->data.ptr_value), parentRef, sublink, node);
			if (info)
				return info;
		}
	}
		break;
	case T_CaseExpr:
	{
		CaseExpr *caseExpr;

		caseExpr = (CaseExpr *) node;

		foreach(lc, caseExpr->args)
		{
			info = extractSublinks((Node *) lfirst(lc), exprRoot,
					(Node **) &(lc->data.ptr_value), parentRef, sublink, node);

			if (info)
				return info;
		}
	}
		break;
	case T_CaseWhen:
	{
		CaseWhen *caseWhen;

		caseWhen = (CaseWhen *) node;

		info = extractSublinks((Node *) caseWhen->expr, exprRoot,
				(Node **) &(caseWhen->expr), parentRef, sublink, node);

		if (info)
			return info;

		info = extractSublinks((Node *) caseWhen->result, exprRoot,
				(Node **) &(caseWhen->result), parentRef, sublink, node);

		if (info)
			return info;
	}
		break;
	case T_FuncExpr:
	{
		FuncExpr *funcExpr;

		funcExpr = (FuncExpr *) node;

		foreach(lc, funcExpr->args)
		{
			info = extractSublinks((Node *) lfirst(lc), exprRoot,
					(Node **) &(lc->data.ptr_value), parentRef, sublink, node);

			if (info)
				return info;
		}
	}
		break;
	case T_OpExpr:
	case T_DistinctExpr:
	case T_NullIfExpr:
	{
		OpExpr *opExpr;

		opExpr = (OpExpr *) node;

		foreach(lc, opExpr->args)
		{
			info = extractSublinks((Node *) lfirst(lc), exprRoot,
					(Node **) &(lc->data.ptr_value), parentRef, sublink, node);

			if (info)
				return info;
		}
	}
		break;
	case T_BoolExpr:
	{
		BoolExpr *boolExpr;

		boolExpr = (BoolExpr *) node;

		foreach(lc, boolExpr->args)
		{
			info = extractSublinks((Node *) lfirst(lc), exprRoot,
					(Node **) &(lc->data.ptr_value), parentRef, sublink, node);

			if (info)
				return info;
		}
	}
		break;
	case T_FieldSelect:
	{
		FieldSelect *fieldSel;

		fieldSel = (FieldSelect *) node;

		return extractSublinks((Node *) fieldSel->arg, exprRoot,
				(Node **) &(fieldSel->arg), parentRef, sublink, node);
	}
		break;
	case T_ScalarArrayOpExpr:
	{
		ScalarArrayOpExpr * saOp;

		saOp = (ScalarArrayOpExpr *) node;

		foreach(lc, saOp->args)
		{
			info = extractSublinks((Node *) lfirst(lc), exprRoot,
					(Node **) &(lc->data.ptr_value), parentRef, sublink, node);

			if (info)
				return info;
		}
	}
		break;
	case T_RelabelType:
	{
		RelabelType *relab;

		relab = (RelabelType *) node;

		return extractSublinks((Node *) relab->arg, exprRoot,
				(Node **) &(relab->arg), parentRef, sublink, node);
	}
		break;
	case T_CoerceViaIO:
	{
		CoerceViaIO *co;

		co = (CoerceViaIO *) node;

		return extractSublinks((Node *) co->arg, exprRoot,
				(Node **) &(co->arg), parentRef, sublink, node);
	}
		break;
	case T_ArrayCoerceExpr:
	{
		ArrayCoerceExpr *acExpr;

		acExpr = (ArrayCoerceExpr *) node;
		return extractSublinks((Node *) acExpr->arg, exprRoot,
				(Node **) &(acExpr->arg), parentRef, sublink, node);
	}
		break;
	case T_ConvertRowtypeExpr:
	{
		ConvertRowtypeExpr *conRowExpr;

		conRowExpr = (ConvertRowtypeExpr *) node;

		return extractSublinks((Node *) conRowExpr->arg, exprRoot,
				(Node **) &(conRowExpr->arg), parentRef, sublink, node);
	}
		break;
	case T_CoerceToDomain:
	{
		CoerceToDomain *coerceDom;

		coerceDom = (CoerceToDomain *) node;

		return extractSublinks((Node *) coerceDom->arg, exprRoot,
				(Node **) &(coerceDom->arg), parentRef, sublink, node);
	}
		break;
	case T_ArrayExpr:
	{
		ArrayExpr *arrayExpr;

		arrayExpr = (ArrayExpr *) node;

		foreach(lc, arrayExpr->elements)
		{
			info = extractSublinks((Node *) lfirst(lc), exprRoot,
					(Node **) &(lc->data.ptr_value), parentRef, sublink, node);

			if (info)
				return info;
		}
	}
		break;
		// nodes that can not contain sublinks
	case T_CurrentOfExpr:
	case T_Var:
	case T_Const:
	case T_Param:
	case T_FieldStore:
	case T_CaseTestExpr:
	case T_CoerceToDomainValue:
	default:
		// do nothing
		break;
	}
	return NULL;
}

/*
 * Creates a SublinkInfo struct
 */

static SublinkInfo *
makeSublinkInfo(SubLink *sublink, Node *rootExpr, Node **sublinkExprPointer,
		Node **grandParentRef, Node *parent)
{
	SublinkInfo *info;
	List *outsideVars;

	info = makeNode(SublinkInfo);
	info->sublink = sublink;

	/* find correlated vars */
	info->condRTEs = NIL;
	info->corrRTEs = NIL;
	info->corrVarInfos = NIL;
	info->targetVar = NULL;
	info->unnested = false;
	findVarsSublevelsUp((Query *) sublink->subselect, info);

	if (info->corrVarInfos != NIL)
	{
		outsideVars
				= findCorrVarTypes(info->corrVarInfos, PROV_CORRVAR_OUTSIDE);

		if (list_length(outsideVars) > 0)
			info->category = SUBCAT_CORRELATED;
		else
			info->category = SUBCAT_CORRELATED_INSIDE;
	}
	else
	{
		info->category = SUBCAT_UNCORRELATED;
	}

	/* set expression sublink is used in and various related pointers */
	info->exprRoot = rootExpr;
	info->location = defaultLoc;
	info->subLinkExprPointer = sublinkExprPointer;
	info->grandParentExprPointer = grandParentRef;
	info->parent = parent;

	return info;
}

/*
 * find range table entries referenced in an expression
 */

void
findRefRTEinCondition(SublinkInfo *info)
{
	ListCell *lc;
	ListCell *innerLc;
	Var *var;
	SubLink *sublink;
	List *vars;
	List *referencedRTEs;
	Index rteNo;
	bool found;

	sublink = info->sublink;
	referencedRTEs = NIL;

	/* find vars used in sublink testexpr */
	vars = findExprVars(info->exprRoot);
	//vars = findExprVars(sublink->testexpr);       //CHECK ok so

	/* create List with RTEs referenced by vars from list */
	foreach(lc, vars)
	{
		var = (Var *) lfirst(lc);

		found = false;
		foreach(innerLc, referencedRTEs)
		{
			rteNo = lfirst_int(innerLc);
			if (var->varno == rteNo)
			{
				found = true;
			}
		}

		if (!found)
		{
			referencedRTEs = lappend_int(referencedRTEs, var->varno);
		}
	}

	info->condRTEs = referencedRTEs;
}

/*
 * returned true if RTE with Index "rtIndex" is referenced in "fromItem".
 */

bool
findRTRefInFromExpr(Node *fromItem, Index rtIndex)
{
	RangeTblRef *ref;
	JoinExpr *joinExpr;

	Assert(fromItem != NULL);

	if (IsA(fromItem, RangeTblRef))
	{
		ref = (RangeTblRef *) fromItem;
		return (ref->rtindex == rtIndex);
	}
	else
	{
		joinExpr = (JoinExpr *) fromItem;
		return (findRTRefInFromExpr(joinExpr->larg, rtIndex)
				|| findRTRefInFromExpr(joinExpr->rarg, rtIndex));
	}
}

/*
 * returns a list of all base relation RTEs for a query
 */

List *
findAccessedBaseRelations(Query *query)
{
	ListCell *lc;
	List *result;
	RangeTblEntry *rte;

	result = NIL;

	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		if (rte->rtekind == RTE_RELATION)
		{
			/* skip view/rules dummy entries */
			if (strcmp("*NEW*", rte->eref->aliasname) != 0 && strcmp("*OLD*",
					rte->eref->aliasname) != 0)
			{
				result = lappend(result, rte);
			}
		}
		else if (rte->rtekind == RTE_SUBQUERY)
		{
			result = list_concat(result, findAccessedBaseRelations(
					rte->subquery));
		}
	}

	return result;
}

/*
 * Searches through a list of sublink infos and returns all sublinks that are
 * at one of the locations (SELECT, WHERE, ...) specified by the flags.
 */

List *
getSublinkTypes(List *sublinks, int flags)
{
	ListCell *lc;
	List *result;
	SublinkInfo *info;

	result = NIL;

	foreach(lc, sublinks)
	{
		info = (SublinkInfo *) lfirst(lc);

		switch (info->location)
		{
		case SUBLOC_SELECT:
			if (flags & PROV_SUBLINK_SEARCH_SELECT)
				result = lappend(result, info);
			break;
		case SUBLOC_WHERE:
			if (flags & PROV_SUBLINK_SEARCH_WHERE)
				result = lappend(result, info);
			break;
		case SUBLOC_HAVING:
			if (flags & PROV_SUBLINK_SEARCH_HAVING)
				result = lappend(result, info);
			break;
		case SUBLOC_GROUPBY:
			if (flags & PROV_SUBLINK_SEARCH_GROUPBY)
				result = lappend(result, info);
			break;
		case SUBLOC_ORDER:
			if (flags & PROV_SUBLINK_SEARCH_ORDER)
				result = lappend(result, info);
			break;
		default:
			//TODO error
			break;
		}
	}

	return result;
}

/*
 *
 */

Node **
findLinkToExpr(Node *expr, Node *root)
{
	FindLinkToExprWalkerContext *context;

	context
			= (FindLinkToExprWalkerContext *) palloc(sizeof(FindLinkToExprWalkerContext));
	context->expr = expr;
	context->parent = &root;
	context->result = NULL;

	findLinkToExprWalker(root, context);

	return context->result;
}

/*
 *
 */

static bool
findLinkToExprWalker(Node *node, FindLinkToExprWalkerContext *context)
{
	return false;
}

/*
 *
 */

List *
findCorrVarTypes(List *corrVars, int flags)
{
	ListCell *lc;
	List *result;
	CorrVarInfo *info;

	result = NIL;

	foreach(lc, corrVars)
	{
		info = (CorrVarInfo *) lfirst(lc);

		if (info->outside && (flags & PROV_CORRVAR_OUTSIDE))
			result = lappend(result, info);
		else if (!info->outside && (flags & PROV_CORRVAR_INSIDE))
			result = lappend(result, info);
	}

	return result;
}

/*
 * Checks if a sublink s contains correlated sublinks that reference an relation
 * outside s that will be rewritten using the GEN-strategy. This means that it
 * cannot be unnested because it would be used in the FROM-clause and no
 * correlations with other FROM-clause items are allowed there.
 */

bool
containsGenCorrSublink(SublinkInfo *info)//TODO remove
{
	List *subInfos;
	ListCell *lc;
	SublinkInfo *subInfo;
	List *outCorrVars;

	/* get child sublink infos */
	subInfos = getChildSublinks(info);

	/* if sublink contains no other sublinks it is safe */
	if (list_length(subInfos) == 0)
		return false;

	/* otherwise check is contained sublinks have correlations referencing
	 * attributes outside the sublink and will be rewritten using GEN-strategy.
	 */
	foreach(lc, subInfos)
	{
		subInfo = (SublinkInfo *) lfirst(lc);

		/* we have only to check for correlated sublinks */
		if (subInfo->category != SUBCAT_UNCORRELATED)
		{
			outCorrVars = findCorrVarTypes(subInfo->corrVarInfos,
					PROV_CORRVAR_OUTSIDE);

			/* sublinks that have correlations inside the sublink are ok */
			if (list_length(outCorrVars) > 0 && !IsUnnestable(subInfo))
				return true;
		}
	}

	return false;
}

/*
 *
 */

List *
getChildSublinks(SublinkInfo *info)
{
	return getChildSublinksWalker((Query *) info->sublink->subselect);
}

/*
 *
 */

static List *
getChildSublinksWalker(Query *query)
{
	ListCell *lc;
	List *result;
	RangeTblEntry *rte;

	result = findSublinkLocations(query, PROV_SUBLINK_SEARCH_ALL);

	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		if (rte->rtekind == RTE_SUBQUERY)
			result = list_concat(result, getChildSublinksWalker(rte->subquery));
	}

	return result;
}
