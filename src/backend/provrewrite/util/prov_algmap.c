/*-------------------------------------------------------------------------
 *
 * prov_algmap.c
 *	  POSTGRES C - Helper methods to map a query tree to equivalent algebra expressions. This is used to generate dot scripts
 *	  			that illustrate the algebra expression of a query and in transformation provenance computation to identify the
 *	  			conceptual parts of a query node.
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/util/prov_algmap.c,v 1.542 26.08.2009 11:12:30 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "parser/parsetree.h"
#include "parser/parse_relation.h"

#include "provrewrite/prov_util.h"
#include "provrewrite/prov_algmap.h"

/*
 * Checks if a query node represents a alegbra expression containing a projection.
 */

bool
isProjection (Query *query)
{
	ListCell *lc;
	ListCell *innerLc;
	TargetEntry *te;
	RangeTblEntry *rte;
	List *vars;
	List *rteVars;
	Var *var;
	Var *rteVar;
	int i;

	vars = NIL;

	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);
		var = getVarFromTeIfSimple((Node *) te->expr);

		/* if te is not a simple var (possibly surrounded by casts then this is a projection */
		if (!var)
			return true;

		/* check that this var is not duplicated or renamed */
		if (list_member(vars, var))
			return true;

		if(isVarRenamed(query, te, var))
			return true;

		var = resolveToRteVar(var, query);

		vars = lappend(vars, var);
	}

	/* now check that nothing is projected out */
	foreachi(lc, i, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		rteVars = NIL;

		/* skip join RTE's. They do not contain new attrs */ //TODO more complex have to consider that it can be either join var or subquery/baserel var!!!
		if (rte->rtekind == RTE_JOIN)
			continue;

		expandRTEWithParam(rte, (i + 1), 0, false, false, NULL, &rteVars);

		foreach(innerLc, rteVars)
		{
			rteVar = (Var *) lfirst(innerLc);

			if(!list_member(vars, rteVar))
				return true;
		}
	}

	return false;
}

/*
 * Checks is a query node is an aggregation and contains a projection under the aggregation operations. E.g.
 * 		SELECT sum(r.i) FROM ...	 -> return false
 * 		SELECT sum(r.i * 2) FROM ... -> return true
 */

bool
isProjectionUnderAgg (Query *query)
{
	ListCell *lc, *innerLc;
	List *aggs;
	TargetEntry *te;
	Aggref *agg;

	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);

		aggs = getAggrExprs((Node *) te->expr);

		foreach(innerLc, aggs)
		{
			agg = (Aggref *) lfirst(innerLc);

			/* no count (*) stuff? */
			if (!agg->aggstar)
			{
				if(!IsA(linitial(agg->args), Var))
					return true;
			}
		}
	}

	return false;
}

/*
 * Checks if a query node is a projection over an aggregation. E.g.:
 * 		SELECT sum(r.i) FROM ...		-> return false
 * 		SELECT 2 * sum(r.i) FROM ...	-> return true
 */

bool
isProjectionOverAgg (Query *query)
{
	ListCell *lc;
	TargetEntry *te;

	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);

		if (!(IsA(te->expr, Aggref) || IsA(te->expr, Var)))
			return true;
	}

	return false;
}

/*
 * Checks if a Target Entry that is a single var is an attribute renaming.
 */

bool
isVarRenamed (Query *query, TargetEntry *te, Var *var)
{
	char *resultname;
	char *origname;
	List *names;
	RangeTblEntry *rte;

	names = NIL;
	resultname = te->resname;
	rte = rt_fetch(var->varno, query->rtable);

	expandRTEWithParam(rte, 0, 0, false, false, &names, NULL);

	origname = strVal(list_nth(names, var->varattno - 1));

	return (strcmp(resultname, origname) != 0);
}
