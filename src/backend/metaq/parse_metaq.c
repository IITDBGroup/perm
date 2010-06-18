/*-------------------------------------------------------------------------
 *
 * parse_metaq.c
 *	  POSTGRES C	- parser support for meta querying language constructs. These are:
 *	  		XsltFuncExpr: call to an XSLT function. This is translated into a call to the
 *	  					generic xslt_process function of postgres. The definition of the
 *	  					XSLT script implementing the function is fetched from relation xslt_funcs.
 *	  		ThisExpr	: is transformed into an XML constant that is the XML representation of
 *	  					the current query tree.
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/metaq/parse_metaq.c,v 1.542 29.09.2009 19:29:04 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "parser/parse_node.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_coerce.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "nodes/makefuncs.h"
#include "nodes/primnodes.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "optimizer/clauses.h"
#include "utils/xml.h"

#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_util.h"
#include "metaq/parse_metaq.h"
#include "metaq/xmlqtree.h"

/* types */
typedef struct ThisWalkerContext
{
	Query *topQuery;
	Query *curQuery;
} ThisWalkerContext;




/* macros */
#define MAKE_THISCONTEXT(context, top, cur) \
	do { \
		context = (ThisWalkerContext *) palloc(sizeof(ThisWalkerContext)); \
		context->topQuery = (top); \
		context->curQuery = (cur); \
	} while (0)

/* functions */
static Node *generateXsltLookup (ParseState *pstate, XsltFuncExpr *xslt);
static Node *handleThisExprMutator (Node *node, void *context);
static Node *transformThisExpr (ThisExpr *this, ThisWalkerContext *context);

/*
 *
 */

Node *
transformXsltFuncCall (ParseState *pstate, XsltFuncExpr *xslt)
{
	FuncExpr *result;
	Oid xsltOid;
	Oid paramOids[2] = {TEXTOID, TEXTOID};
	List *args;
	Node *param;
	Oid paramType;

	/* inform the analyzer of the new sublink */
	pstate->p_hasSubLinks = true;

	/* get xsltOid */
	xsltOid = LookupFuncName(list_make1(makeString("xslt_process")), 2, paramOids, true); //TODO cache

	/* analyze the parameter and if necessary cast it to text */
	param = transformExpr(pstate, xslt->param);
	paramType = exprType(param);

	if (paramType != TEXTOID)
	{
		param = coerce_to_target_type(pstate,
									param,
									paramType,
									TEXTOID,
									-1,
									COERCION_EXPLICIT,
									COERCE_IMPLICIT_CAST);

		if (!param)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_PARAMETER),
					errmsg("cannot coerce type %s to text", format_type_be(paramType))));
	}
	/* generate args */
	args = list_make2(
				param,
				generateXsltLookup(pstate, xslt)
				);

	result = makeFuncExpr(xsltOid, TEXTOID, args, COERCE_EXPLICIT_CALL);

	return (Node *) result;
}

/*
 *
 */

static Node *
generateXsltLookup (ParseState *pstate, XsltFuncExpr *xslt)
{
	RelabelType *caster;
	SubLink *result;
	Query *query;
	RangeTblRef *rtRef;
	RangeTblEntry *rte;
	RangeVar *rangeVar;
	Const *funcNameConst;
	Datum constVal;

	result = makeNode(SubLink);
	query = makeQuery ();

	result->operName = NIL;
	result->subLinkType = EXPR_SUBLINK;
	result->testexpr = NULL;
	result->subselect = (Node *) query;

	/* create target entry (the XSLT-script) */
	query->targetList = list_make1(makeTargetEntry(
							(Expr *) makeVar(1,2,XMLOID,-1 ,0), 1, "func", false));

//	rte = makeRte(RTE_RELATION);
//	rte->relid = RelnameGetRelid("xslt_funcs");

	/* create range table entry for xslt_funcs */
	rangeVar = makeNode(RangeVar);

	rangeVar->relname = "xslt_funcs";
	rangeVar->catalogname = NULL;
	rangeVar->schemaname = NULL;
	rangeVar->isProvBase = false;
	rangeVar->istemp = false;
	rangeVar->provAttrs = NIL;
	rangeVar->inhOpt = INH_NO;
	rangeVar->alias = NULL;

	rte = addRangeTableEntry(pstate, rangeVar, NULL, false, true);
	query->rtable = list_make1(rte);

	/* create jointree and qual that checks for the correct function name */
	MAKE_RTREF(rtRef, 1);
	query->jointree->fromlist = list_make1(rtRef);

	constVal = DirectFunctionCall1(textin, CStringGetDatum(xslt->funcName));
	funcNameConst = makeConst(TEXTOID, -1, -1, constVal, false, false);
	query->jointree->quals = createEqualityConditionForNodes(
									(Node *) makeVar(1,1,TEXTOID,-1,0),
									(Node *) funcNameConst);

	caster = makeNode(RelabelType);
	caster->arg = (Expr *) result;
	caster->resulttype = TEXTOID;
	caster->resulttypmod = -1;
	caster->relabelformat = COERCE_EXPLICIT_CAST;

	return (Node *) caster;
}

/*
 *
 */

Query *
handleThisExprs (Query *query)
{
	ThisWalkerContext *context;

	MAKE_THISCONTEXT(context, query, query);

	query = (Query *) handleThisExprMutator((Node *) query, context);

	pfree(context);

	return query;
}

/*
 *
 */

static Node *
handleThisExprMutator (Node *node, void *context)
{
	ThisWalkerContext *newContext;
	Node *result;

	if (node == NULL)
		return false;

	if (IsA(node, Query))
	{
		MAKE_THISCONTEXT(newContext, ((ThisWalkerContext *) context)->topQuery, (Query *) node);
		result = (Node *) query_tree_mutator((Query *) node, handleThisExprMutator, (void *) newContext, QTW_DONT_COPY_QUERY);
		pfree(newContext);

		return result;
	}
	else if (IsA(node,ThisExpr))
		return transformThisExpr((ThisExpr *) node, (ThisWalkerContext *) context);

	return expression_tree_mutator(node, handleThisExprMutator, context);
}

/*
 *
 */

static Node *
transformThisExpr (ThisExpr *this, ThisWalkerContext *context)
{
	Const *cons;
	Datum xmlDatum;
	Query *queryPart;
	char *typePart;
	int childNum;
	RangeTblEntry *rte;

	if(strcmp(this->thisType, "local") == 0)
		queryPart = context->curQuery;
	else if (strcmp(this->thisType, "top") == 0)
		queryPart = context->topQuery;
	else if (strncmp(this->thisType, "child", 5) == 0)
	{
		typePart = this->thisType + 4;
		queryPart = context->curQuery;

		while(*(++typePart))
		{
			if (!isdigit(*typePart))
				;//TODO ERROR

			childNum = *typePart - '0';

			if (childNum < 1 || childNum > list_length(queryPart->rtable))
				;//TODO error

			rte = rt_fetch(childNum, queryPart->rtable);

			if (!rte->rtekind == RTE_SUBQUERY)
				;//TODO error

			queryPart = rte->subquery;
		}
	}
	else
	{
		//TODO ERROR unknown this type
	}

	xmlDatum = XmlPGetDatum(queryTreeToXml(queryPart));

	cons = makeConst(XMLOID, -1, -1, xmlDatum, false, false);

	return (Node *) cons;
}
