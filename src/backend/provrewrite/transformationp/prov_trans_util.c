/*-------------------------------------------------------------------------
 *
 * prov_trans_util.c
 *	  POSTGRES C -
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/transformationp/prov_trans_util.c,v 1.542 03.09.2009 10:24:04 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "nodes/makefuncs.h"
#include "catalog/pg_type.h"
#include "utils/datum.h"
#include "parser/parse_expr.h"
#include "parser/parsetree.h"

#include "provrewrite/provstack.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_trans_util.h"



/*
 *
 */

TransSubInfo *
getRootSubForNode (TransProvInfo *info)
{
	while (IsA(info->root, TransProvInfo))
		info = (TransProvInfo *) info->root;

	return ((TransSubInfo *) info->root);
}

/*
 *
 */

void
addStaticTransProvAttr (Query *query)
{
	TransProvInfo *info;
	Datum provSet;
	TargetEntry *newTarget;

	info = GET_TRANS_INFO(query);
	provSet = getRootSubForNode(info)->setForNode;
	newTarget = MAKE_TRANS_PROV_ATTR(query, MAKE_VARBIT_CONST(provSet));

	query->targetList = lappend(query->targetList, newTarget);
	info->transProvAttrNum = list_length(query->targetList);
}

/*
 *
 */

Node **
getParentPointer (TransProvInfo *parent, TransProvInfo *sub, Node **current)
{
	TransSubInfo *node;
	TransProvInfo *curInfo;
	Node *child;
	Node **result;
	ListCell *lc;

	result = NULL;

	if (IsA(*current, TransProvInfo))
	{
		curInfo = (TransProvInfo *) *current;

		/* have found parent pointer */
		if(curInfo->rtIndex == sub->rtIndex)
			return current;
	}
	else
	{
		node = (TransSubInfo *) *current;

		foreach(lc, node->children)
		{
			child = (Node *) lfirst(lc);

			result = getParentPointer(parent, sub, (Node **) &(lc->data.ptr_value));

			if(result)
				return result;
		}
	}

	return NULL;
}

/*
 *
 */

Node *
getRealSubqueryTransProvAttr (Query *query, Index rtIndex)
{
	RangeTblEntry *rte;
	TransProvInfo *info;
	TargetEntry *te;

	rte = rt_fetch(rtIndex, query->rtable);

	info = GET_TRANS_INFO(rte->subquery);
	te = (TargetEntry *) list_nth(rte->subquery->targetList, info->transProvAttrNum - 1);

	return (Node *) makeVar(rtIndex, info->transProvAttrNum, exprType((Node *) te->expr), exprTypmod((Node *) te->expr), 0);
}

/*
 *
 */

Node *
getSubqueryTransProvAttr (Query *query, Index rtIndex)
{
	RangeTblEntry *rte;
	TransProvInfo *info;
	TransSubInfo *subInfo;
	TargetEntry *te;

	rte = rt_fetch(rtIndex, query->rtable);

	info = GET_TRANS_INFO(rte->subquery);
	subInfo = getRootSubForNode(info);

	if (info->isStatic)
		return (Node *) MAKE_VARBIT_CONST(subInfo->setForNode);

	te = (TargetEntry *) list_nth(rte->subquery->targetList, info->transProvAttrNum - 1);

	return (Node *) makeVar(rtIndex, info->transProvAttrNum, exprType((Node *) te->expr), exprTypmod((Node *) te->expr), 0);
}

/*
 *
 */

Node *
getSubqueryTransProvUnionSet (Query *query, Index rtIndex, Datum bitset)	//TODO include bitset into sub if possible
{
	Var *var;
	Node *result;

	var = (Var *) getSubqueryTransProvAttr (query, rtIndex);
	result = (Node *) MAKE_SETOR_FUNC(list_make2(MAKE_VARBIT_CONST(bitset), var));

	return result;
}

/*
 *
 */

Node *
createBitorExpr (List *args)
{
	FuncExpr *orFunc;
	FuncExpr *resultFunc;
	ListCell *lc;

	orFunc = NULL;

	/* create function expression of format bitor_with_null(arg1, bitor_wth_null(arg2, bitor_with_null( ... */
	foreach(lc, args)
	{
		/* first element create or function */
		if (orFunc == NULL)
		{
			orFunc = MAKE_SETOR_FUNC(list_make1(lfirst(lc)));
			resultFunc = orFunc;
		}
		/* add new nested function call */
		else if (lc->next != NULL)
		{
			orFunc->args = lappend(orFunc->args, MAKE_SETOR_FUNC(list_make1(lfirst(lc))));
			orFunc = (FuncExpr *) llast(orFunc->args);
		}
		/* last element add it as second argument to function call */
		else
			orFunc->args = lappend(orFunc->args, lfirst(lc));
	}

	return (Node *) resultFunc;
}


/*
 *
 */


TransSubInfo *
getTopJoinInfo (Query *query, bool hasCross)
{
	TransProvInfo *info;
	Node *result;
	Node *child;
	TransSubInfo *sub;
	ListCell *lc;

	info = GET_TRANS_INFO(query);
	result = info->root;

	/* no cross product */
	if (!hasCross) //TODO
	{
		while(IsA(result, TransSubInfo) &&
				(((TransSubInfo *) result)->opType == SUBOP_Aggregation ||
				((TransSubInfo *) result)->opType == SUBOP_Projection ||
				((TransSubInfo *) result)->opType == SUBOP_Having ||
				((TransSubInfo *) result)->opType == SUBOP_Selection))
			result = (Node *) TSET_LARG(result);

		if(IsA(result, TransProvInfo))
			return NULL;
	}
	/* has a cross product. If this is not represented as a TransSubInfo,
	 * we have to be smarter in recognizing the top join node.
	 */
	else
	{
		while (true)
		{
			if(IsA(result, TransProvInfo))
				return NULL;

			/* easy check if a node has more than 2
			 * children its definitly the top join node */
			if (list_length(((TransSubInfo *) result)->children) > 1)
				return (TransSubInfo *) result;

			/* check its children, if they are TransProvInfos, Joins, or BaseRel accesses then
			 * the current node it the top join node.
			 */
			foreach(lc, ((TransSubInfo *) result)->children)
			{
				child = (Node *) lfirst(lc);

				if(IsA(child, TransProvInfo))
					return (TransSubInfo *) result;

				sub = (TransSubInfo *) child;

				switch(sub->opType)
				{
				case SUBOP_Join_Inner:
				case SUBOP_Join_Left:
				case SUBOP_Join_Right:
				case SUBOP_Join_Full:
				case SUBOP_Join_Cross:
				case SUBOP_BaseRel:
					return (TransSubInfo *) result;
				default:
					break;
				}
			}
			result = child;
		}
	}

	return (TransSubInfo *) result;
}

/*
 *
 */

TransSubInfo *
getSpecificInfo (TransSubInfo *cur, SubOperationType type)
{
	TransSubInfo *childInfo;
	ListCell *lc;
	Node *child;

	if (cur->opType == type)
		return cur;

	foreach(lc, cur->children)
	{
		child = lfirst(lc);

		if (IsA(child, TransSubInfo))
			childInfo = getSpecificInfo((TransSubInfo *) child, type);

		if (childInfo)
			return childInfo;
	}

	return NULL;
}

/*
 *
 */

TransProjType
getProjectionType (Query *query, bool *underHaving)
{
	TransSubInfo *curSub;

	curSub = (TransSubInfo *) GET_TRANS_INFO(query)->root;
	*underHaving = false;

	if (query->hasAggs)
	{
		if (curSub->opType == SUBOP_Having)
		{
			*underHaving = true;
			curSub = TSET_LARG(curSub);
		}
		if (curSub->opType == SUBOP_Projection)
		{
			curSub = TSET_LARG(curSub);
			if (list_length(curSub->children) > 0 &&
					TSET_LARG(curSub)->opType == SUBOP_Projection)
				return ProjBothAgg;

			return ProjOverAgg;
		}
		else
		{
			if(list_length(curSub->children) > 0 &&
					TSET_LARG(curSub)->opType == SUBOP_Projection)
				return ProjUnderAgg;

			return SingleOp;
		}
	}
	else if (curSub->opType != SUBOP_Projection)
		return belongToTop;

	return SingleOp;
}
