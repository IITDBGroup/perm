/*-------------------------------------------------------------------------
 *
 * prov_adaptsuper.c
 *	  PERM C - provenance module non-provenance super query handling
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/prov_adaptsuper.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "parser/parse_expr.h"
#include "provrewrite/prov_adaptsuper.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/provlog.h"
#include "provrewrite/provattrname.h"
#include "provrewrite/prov_nodes.h"

///* prototypes */
//static bool searchForAttr (Query *query, Var *var, char *attrname);
//static bool searchForRTEAttr (RangeTblEntry * rte, Var *var, char *attrname);
//
///*
// * check super queries of a provenance query for provenance attribute TEs
// * that have to be checked. This is nessecary because the analyser is not
// * aware of provenance attributes and has to ignore them.
// */
//
//void
//adaptSuperQueriesTE (Query *query)
//{
//	RangeTblEntry *rte;
//	TargetEntry *te;
//	ListCell *lc;
//	Var *var;
//
//	logDebug ("------------	adapt super query TE STARTED -----------------");
//
//	/* query is a provenance query, so we are finished */
//	if (IsProvRewrite(query))
//		return;
//
//	/*
//	 * walk through target list and search for dummy entries representing provenance attributes
//	 * or star expressions
//	 */
//	foreach(lc, query->targetList)
//	{
//		te = (TargetEntry *) lfirst(lc);
//
//		/* check if te is a provenance attr */
//		if (strncmp(te->resname, "prov_", 5) == 0)
//		{
//			logDebug("found attribute to adapt");
//
//			var = (Var *) te->expr;
//
//			logDebug("have to find varnatto and maybe varno too");
//
//			if (var->varno != 0)
//			{
//				if (!searchForRTEAttr (list_nth(query->rtable, var->varno - 1), var, te->resname))
//					ereport(ERROR,
//							(errcode(ERRCODE_AMBIGUOUS_COLUMN),
//							errmsg("provenance column \"%s\" does not exist", te->resname)));
//			}
//			else
//			{
//				searchForAttr (query, var, te->resname);
//			}
//		}
//	}
//
//	/* adapt sub queries TEs not referenced in super query */
//	logDebug("adapted TEs for super query");
//
//	foreach(lc, query->rtable)
//	{
//		rte = (RangeTblEntry *) lfirst(lc);
//
//		if (rte->rtekind == RTE_SUBQUERY)
//			adaptSuperQueriesTE (rte->subquery);
//	}
//
//	logDebug("------------	adapt super query TE FINISHED -----------------");
//}
//
//static bool
//searchForRTEAttr (RangeTblEntry * rte, Var *var, char *attrname)
//{
//	Query *query;
//	TargetEntry *te;
//	Var *teVar;
//	ListCell *lc;
//	int attrno;
//	bool subFound;
//
//	logDebug("sfRAttr    -- start");
//
//	if (rte->rtekind != RTE_SUBQUERY)
//	{
//		ereport(ERROR,
//				(errcode(ERRCODE_AMBIGUOUS_COLUMN),
//				errmsg("rte is not a subquery")));
//	}
//	query = rte->subquery;
//
//	/* subquery is a rewritten provenance query */
//	if (IsProvRewrite(query))
//	{
//		logDebug("sfRAttr    -- is provenance query");
//		attrno = 0;
//		foreach(lc, query->targetList)
//		{
//			attrno++;
//			te = (TargetEntry *) lfirst(lc);
//
//			Assert(IsA(te->expr, Var));
//
//			/* we found attribute with same name */
//			if (strcmp(te->resname, attrname) == 0)
//			{
//				var->varattno = attrno;
//				var->vartype = exprType((Node *) te->expr);
//				return true;
//			}
//		}
//
//		/* attribute not found */
//		return false;
//	}
//	/* subquery is a normal subquery */
//	else {
//		logDebug("sfRAttr    -- is normal subquery");
//
//		attrno = 0;
//		foreach(lc, query->targetList)
//		{
//			attrno++;
//			te = (TargetEntry *) lfirst(lc);
//
//			/* we found attribute with same name */
//			if (strcmp(te->resname, attrname) == 0)
//			{
//				var->varattno = attrno;
//				teVar = (Var *) te->expr;
//
//				if (teVar->varno == 0)
//					subFound = searchForAttr(query, teVar, attrname);
//				else
//					subFound = searchForRTEAttr(list_nth(query->rtable,teVar->varno - 1), teVar, attrname);
//
//				var->vartype = teVar->vartype;
//				return subFound;
//			}
//		}
//	}
//
//	/* not found */
//	return false;
//}
//
//static bool
//searchForAttr (Query *query, Var *var, char *attrname)
//{
//	ListCell *lc;
//	RangeTblEntry *rte;
//	bool found;
//	Index varno;
//
//	found = false;
//	varno = 0;
//
//	foreach(lc, query->rtable)
//	{
//		varno++;
//		rte = (RangeTblEntry *) lfirst(lc);
//
//		if (searchForRTEAttr(rte, var, attrname))
//		{
//			if (found)
//				ereport(ERROR,
//						(errcode(ERRCODE_AMBIGUOUS_COLUMN),
//						errmsg("column reference \"%s\" is ambiguous",
//								attrname)));
//			found = true;
//			var->varno = varno;
//		}
//	}
//	return found;
//}

/*
 * remove dummy provenance attribute target entries added during parse analysis.
 */

void removeDummyProvenanceTEs (Query *query)
{
	ListCell *lc;
	TargetEntry *te;
	int pos;

	pos = 0;
	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);
		if (isProvAttr(te))
			query->targetList = list_truncate(query->targetList, pos);

		pos++;
	}
}
