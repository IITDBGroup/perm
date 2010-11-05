/*-------------------------------------------------------------------------
 *
 * prov_where_spj.c
 *	  PERM C 
 *
 * Portions Copyright (c) 2010 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $Perm: pgsql//SF_Perm/src/backend/provrewrite/wherecs/prov_where_spj.c,v 1.542 Oct 4, 2010 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "parser/parse_coerce.h"
#include "parser/parse_relation.h"
#include "parser/parse_expr.h"
#include "parser/parsetree.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"

#include "provrewrite/provattrname.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_where_spj.h"

/* prototypes */
static Query *addBaseRelationAnnotationAttrs (RangeTblEntry *rte, List *attrs);
static Node *getAnnotValueExpr (Oid reloid, char *attrName);

/*
 *
 */

#define GET_ANNOTREL_CURATT(index) \
	do { \
		RangeTblEntry *rte = rt_fetch(rtindex, query->rtable); \
		curSubResno = list_length(baseVars[rtindex - 1]); \
		curSubResno += list_length(rte->eref->colnames); \
		curSubResno++; \
	} while (0)

Query *
rewriteWhereSPJQuery (Query *query)
{
	ListCell *lc;
	TargetEntry *te;
	Var *var, *outVar, *annotVar;
	List **baseVars;
	WhereProvInfo *whereInfo;
	WhereAttrInfo *newAttrInfo;
	RangeTblEntry *rte;
	int rtindex, numBaseRels, i, curSubResno;

	/* process normal SPJ query */
	whereInfo = GET_WHERE_PROVINFO(query);

	numBaseRels = 0;
	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		if (rte->rtekind == RTE_RELATION)
			numBaseRels++;
	}

	baseVars = (List **) palloc(sizeof(List *) * numBaseRels);
	for(i = 0; i < numBaseRels; i++)
		baseVars[i] = NIL;

	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);
		var = getVarFromTeIfSimple((Node *) te->expr);
		Assert(var != NULL); //TODO const ok?
		rtindex = var->varno;

		outVar = makeVar(-1, te->resno, exprType((Node *) te->expr),
				exprTypmod((Node *) te->expr), false);

		GET_ANNOTREL_CURATT(var->varno);
		annotVar = makeVar(var->varno, curSubResno, TEXTOID, -1, false);

		newAttrInfo = (WhereAttrInfo *) makeNode(WhereAttrInfo);
		newAttrInfo->outVar = outVar;
		newAttrInfo->inVars = list_make1(copyObject(var));
		newAttrInfo->annotVars = list_make1(annotVar);

		whereInfo->attrInfos = lappend(whereInfo->attrInfos, newAttrInfo);

		baseVars[rtindex - 1] = lappend_int(baseVars[rtindex - 1],
				var->varattno);
	}

	for(i = 0; i < numBaseRels; i++)
		addBaseRelationAnnotationAttrs(rt_fetch(i + 1, query->rtable),
				baseVars[i]);

	for(i = 0; i < numBaseRels; i++)
		list_free(baseVars[i]);
	pfree(baseVars);

	return query;
}

/*
 *
 */

static Query *
addBaseRelationAnnotationAttrs (RangeTblEntry *rte, List *attrs)
{
	Query *result;
	List *newTes = NIL;
	TargetEntry *origAttr;
	TargetEntry *annotAttr;
	ListCell *lc;
	Expr *annotExpr;
	int curResno, i;
	char *annotName;

	/* transform into a trivial query */
	result = generateQueryFromBaseRelation(copyObject(rte));
	curResno = list_length(result->targetList);

	/* for each attribute generate an annotation attribute */
	foreachi(lc, i, result->targetList)
	{
		origAttr = (TargetEntry *) lfirst(lc);

		if (listPositionInt(attrs, i + 1) != -1)
		{
			annotName = getWhereAnnotName(origAttr->resname);
			annotExpr = (Expr *) getAnnotValueExpr(rte->relid,
					origAttr->resname);
			annotAttr = makeTargetEntry ((Expr *) annotExpr, ++curResno,
					annotName, false);
			newTes = lappend(newTes, annotAttr);
		}
	}

	result->targetList = list_concat(result->targetList, newTes);

	/* change rte type to subquery */
	rte->rtekind = RTE_SUBQUERY;
	rte->subquery = result;
	correctRTEAlias(rte);

	return result;
}

/*
 * Create the expression that computes the value of an WHERE-CS annotation
 * attribute. This expression concatenates the constant prefix string
 * with the oid attribute value of a tuple. For instance, for the annotation
 * attribute for relation R and its attribute A the following prefix is
 * generated:
 * 		"R#A#"
 */

static Node *
getAnnotValueExpr (Oid reloid, char *attrName)
{
	Const *annotConst;
	Node *result;
	Var *oidVar;
	Node *oidCast;
	char *annotValue;
	char *relName;
	int constLength;

	/* create a var for the oid column */
	oidVar = makeVar(1, -2, OIDOID ,-1, 0);
	oidCast = coerce_to_target_type(NULL, (Node *) oidVar, OIDOID, TEXTOID, -1,
			COERCION_EXPLICIT, COERCE_EXPLICIT_CAST);

	/* create a constant for the annotation prefix
	 * ("Relname#Attrname#" ) */
	relName = getRelationNameUnqualified(reloid);
	constLength = strlen(relName) + strlen(attrName) + 3;
	annotValue = (char *) palloc(constLength);

	annotValue = strcpy(annotValue, relName);
	annotValue = strcat(annotValue, "#");
	annotValue = strcat(annotValue, attrName);
	annotValue = strcat(annotValue, "#");

	pfree(relName);

	annotConst = makeConst(TEXTOID, -1, -1,
			DirectFunctionCall1(textin,CStringGetDatum(annotValue)),
			false, false);

	/* apply text concat to both values */
	result = (Node *) makeFuncExpr(F_TEXTCAT, TEXTOID,
			list_make2(annotConst, oidCast), COERCE_IMPLICIT_CAST);

	return result;
}
