/*-------------------------------------------------------------------------
 *
 * prov_trans_main.c
 *	  POSTGRES C - Transformation provenance rewrite main methods.
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/transformationp/prov_trans_main.c,v 1.542 24.08.2009 14:40:21 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "nodes/makefuncs.h"
#include "catalog/pg_type.h"
#include "utils/fmgroids.h"
#include "utils/datum.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "nodes/makefuncs.h"

#include "provrewrite/provattrname.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_trans_main.h"
#include "provrewrite/prov_trans_staticana.h"
#include "provrewrite/prov_trans_set.h"
#include "provrewrite/prov_trans_aggr.h"
#include "provrewrite/prov_trans_util.h"
#include "provrewrite/prov_trans_bitset.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/provstack.h"


/* methods */
static Query *rewriteTransSPJ (Query *query);

static void rewriteStaticQueryNode (Query *query);

static Query *addResultConstruction (Query *query, int queryId, ContributionType type);
static void wrapChildComps (TransSubInfo *parent, Query *query, List *childArgs);
static void wrapElementComp (int elem, Query *query, List *list, Node *child);
static Node *buildBitsetComputationForQueryNode (Query *query, TransProvInfo *info, TransSubInfo *node);


/*
 *
 */

Query *
rewriteQueryTransProv (Query *query, char *cursorName)
{
	int queryId;
	ContributionType type;

	type = ContributionType(query);
	/* create ids for parts of the query and determine if parts
	 * of the query produce static transformation provenance.
	 */
	analyseStaticTransProv(query);

	if(type == CONTR_TRANS_SQL)
		queryId = generateTransProvQueryIndex(query, cursorName);
	if(type == CONTR_TRANS_XML || type == CONTR_TRANS_XML_SIMPLE)
		queryId = generateTransXmlQueryIndex(query, cursorName);
	if(type == CONTR_MAP)
		queryId = generateMapQueryIndex(query, cursorName);

	/* rewrite query for transformation provenance computation */
	query = rewriteQueryNodeTrans (query, NULL, NULL);

	/* rewrite for transformation provenance propagation and result construction */
	addResultConstruction (query, queryId, type);

	return query;
}

/*
 *
 */

static Query *
addResultConstruction (Query *query, int queryId, ContributionType type)
{
	TargetEntry *provAttr;

	/* rename top prov attr to "prov_trans" and change it as a call to the
	 * transformation provenance representation construction function.
	 */
	provAttr = (TargetEntry *) list_nth(query->targetList, GET_TRANS_INFO(query)->transProvAttrNum - 1);
	provAttr->resname = "trans_prov";

	switch(type)
	{
	case CONTR_TRANS_XML: //CHECK ok for xml
	case CONTR_TRANS_SQL:
		provAttr->expr = (Expr *) makeFuncExpr(F_RECONSTRUCTTRANSTOSQL,
											TEXTOID,
											list_make2(provAttr->expr,
													makeConst(INT4OID, -1, 4, Int32GetDatum(queryId), false, true)),
											COERCE_EXPLICIT_CALL);
	break;
	case CONTR_MAP:
		provAttr->expr = (Expr *) makeFuncExpr(F_RECONSTRUCTMAP,
											TEXTOID,
											list_make2(provAttr->expr,
													makeConst(INT4OID, -1, 4, Int32GetDatum(queryId), false, true)),
											COERCE_EXPLICIT_CALL);
	break;
	default:
	break;
	}

	return query;
}

/*
 *
 */

extern Query *
rewriteQueryNodeTrans (Query *query, RangeTblEntry *parent, Node **parentInfo)
{
	TransProvInfo *trans;

	trans = GET_TRANS_INFO(query);

	/* is static just add set as const to target list. Only for set operations we need
	 * to add a new top query node
	 */
	if (trans->isStatic)
	{
		if (query->setOperations)
			query = rewriteStaticSetOp (query, parentInfo);
		else
			rewriteStaticQueryNode (query);

		/* create correct eref for rewritten query */
		if (parent)
			correctRTEAlias(parent);

		return query;
	}

	/* choose rewrite method for query type */
	if (query->hasAggs)
		query = rewriteTransAgg (query, parentInfo);
	else if (query->setOperations)
		query = rewriteTransSet (query, parentInfo, parent);
	else
		query = rewriteTransSPJ (query);

	/* create correct eref for rewritten query */
	if (parent)
		correctRTEAlias(parent);

	return query;
}


/*
 *
 */

static Query *
rewriteTransSPJ (Query *query)
{
	Node *bitSetComp;
	TransProvInfo *info;
	TargetEntry *newTarget;
	ListCell *lc;
	RangeTblEntry *rte;

	info = GET_TRANS_INFO(query);

	/* rewrite children first such that we have their trans prov target entries available */
	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		switch(rte->rtekind)
		{
			case RTE_SUBQUERY:
				rte->subquery = rewriteQueryNodeTrans(rte->subquery, rte, getParentPointer(info, GET_TRANS_INFO(rte->subquery), &(info->root)));
			break;
			default:
				/* do nothing ok?*/
			break;
		}

	}

	if (IsA(info->root, TransProvInfo))
		bitSetComp = getSubqueryTransProvAttr (query, ((TransProvInfo *) info->root)->rtIndex);
	else
		bitSetComp = buildBitsetComputationForQueryNode (query, info, (TransSubInfo *) info->root);

	newTarget = MAKE_TRANS_PROV_ATTR(query, bitSetComp);

	query->targetList = lappend(query->targetList, newTarget);
	info->transProvAttrNum = list_length(query->targetList);

	return query;
}

/*
 *
 */

static Node *
buildBitsetComputationForQueryNode (Query *query, TransProvInfo *info, TransSubInfo *node)
{
	List *childArgs;
	ListCell *lc;
	Node *child;
	Node *childComp;
	TransSubInfo *childSub;

	childArgs = NIL;

	/* node is static return a const */
	if (node->isStatic)
		return (Node *) MAKE_VARBIT_CONST(node->setForNode);

	/* get bitset computation or const for each child of current node */
	foreach(lc, node->children)
	{
		child = (Node *) lfirst(lc);

		if (IsA(child, TransProvInfo))
			childComp = getSubqueryTransProvAttr(query, ((TransProvInfo *) child)->rtIndex);
		else
		{
			childSub = (TransSubInfo *) lfirst(lc);

			/* child is static no run-time computation needed */
			if(childSub->isStatic)
				childComp = (Node *) MAKE_VARBIT_CONST(childSub->setForNode);
			else
				childComp = buildBitsetComputationForQueryNode(query, info, childSub);
		}

		childArgs = lappend(childArgs, childComp);
	}
	childArgs = lappend(childArgs, MAKE_VARBIT_CONST(node->setForNode));
	wrapChildComps (node, query, childArgs);

	if (list_length(childArgs) == 1)
		return (Node *) linitial(childArgs);

	return createBitorExpr (childArgs);
}

/*
 *
 */

static void
wrapChildComps (TransSubInfo *parent, Query *query, List *childArgs)
{
	switch(parent->opType)
	{
	case SUBOP_Join_Left:
		wrapElementComp(1, query, childArgs, (Node *) list_nth(parent->children, 1));
	break;
	case SUBOP_Join_Right:
		wrapElementComp(0, query, childArgs, (Node *) list_nth(parent->children, 0));
	break;
	case SUBOP_Join_Full:
		wrapElementComp(0, query, childArgs, (Node *) list_nth(parent->children, 0));
		wrapElementComp(1, query, childArgs, (Node *) list_nth(parent->children, 1));
	break;
	default:
	break;
	}
}

/*
 *
 */

static void
wrapElementComp (int elem, Query *query, List *list, Node *child)
{
	ListCell *lc;
	Node *comp;
	RangeTblEntry *rte;
	List *rteVars;
	Var *childVar;
	CaseExpr *caseExpr;
	CaseWhen *casePart;
	NullTest *isNull;
	Index rtIndex;

	rteVars = NIL;
	rtIndex = IsA(child, TransProvInfo) ? ((TransProvInfo *) child)->rtIndex : ((TransSubInfo *) child)->rtIndex;

	rte = rt_fetch(rtIndex, query->rtable);
	expandRTEWithParam(rte, rtIndex, 0, false, false, NULL, &rteVars);
	childVar = linitial(rteVars);

	for(lc = list_head(list); elem > 0; elem--, lc = lc->next)
		;

	comp = (Node *) lfirst(lc);

	isNull = makeNode(NullTest);
	isNull->arg = copyObject(childVar);
	isNull->nulltesttype = IS_NULL;

	casePart = makeNode(CaseWhen);
	casePart->expr = (Expr *) isNull;
	casePart->result = (Expr *) makeNullConst(VARBITOID, -1);

	caseExpr = makeNode(CaseExpr);
	caseExpr->casetype = VARBITOID;
	caseExpr->defresult = (Expr *) comp;
	caseExpr->args = list_make1(casePart);

	lfirst(lc) = caseExpr;
}


/*
 *
 */

static void
rewriteStaticQueryNode (Query *query)
{
	addStaticTransProvAttr (query);
}




