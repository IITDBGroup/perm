/*-------------------------------------------------------------------------
 *
 * prov_copy_equi.c
 *	  POSTGRES C
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/prov_copy_equi.c,v 1.542 14.11.2008 09:02:08 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "provrewrite/provlog.h"
#include "provrewrite/prov_sublink_util_analyze.h"
#include "provrewrite/prov_copy_equi.h"
#include "optimizer/clauses.h"

#define GET_LEFT_VAR_NUM (equi) \
	(((Var *) linitial(((OpExpr *) (equi))->args))->varattno)

#define GET_RIGHT_VAR_NUM (equi) \
	(((Var *) list_nth(((OpExpr *) (equi))->args,2))->varattno)

#define GET_LEFT_VAR (equi) \
	((Var *) linitial(((OpExpr *) (equi))->args))

#define GET_RIGHT_VAR (equi) \
	((Var *) list_nth(((OpExpr *) (equi))->args,2))


/* static functions */
static bool getEquivFromExprWalker (Node *node, List **context);
static EqGraph *makeEqGraph (int numNodes);
static int getNumberOfAttrs (List *equivs);

/*
 *
 */

List *
getEquivFromExpr (Node *expr)
{
	List *result;

	getEquivFromExprWalker (expr, &result);

	return result;
}

/*
 *
 */
static bool
getEquivFromExprWalker (Node *node, List **context)
{
	if (node == NULL)
		return false;

	if (IsA(node, OpExpr))
	{
		OpExpr *op;

		op = (OpExpr *) node;

		if (!isEqualityOper(op))
			return false;

		if (!op->args->length == 2)
			return false;

		if (!IsA(linitial(op->args), Var))
			return false;

		if (!IsA(list_nth(op->args,1),Var))
			return false;

		*context = list_append_unique(*context, op);
		//TODO sublevels checken

		return false;
	}

	return expression_tree_walker(node, getEquivFromExprWalker, (void *) context);
}


EqGraph *
computeEqGraph (Query *query)
{
	List *equis;
	EqGraph *eq;

	eq = makeEqGraph(1);
	equis = getEquivFromExpr(query->jointree->quals);

	return eq;
}

static int
getNumberOfAttrs (List *equivs)
{
	List *varSeen;
	Var *newVar;
	OpExpr *equi;
	ListCell *lc;

	varSeen = NIL;

	foreach(lc, equivs)
	{
		equi = (OpExpr *) lfirst(lc);
		//newVar = GET_VAR
	}

	return -1;
}

List *
getComponentAttrs (EqGraph *graph, Var *attr)
{
	return NULL;
}

static EqGraph *
makeEqGraph (int numNodes)
{
	EqGraph *result;


	result = (EqGraph *) palloc(sizeof(EqGraph));

	result->components = NIL;


	return result;
}
