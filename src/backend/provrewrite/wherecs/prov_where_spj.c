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
#include "parser/parse_expr.h"


#include "provrewrite/provattrname.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_where_util.h"
#include "provrewrite/prov_where_spj.h"

/*
 *
 */

Query *
rewriteWhereSPJQuery (Query *query)
{
	ListCell *lc;
	TargetEntry *te;
	Var *var, *outVar;
	WhereProvInfo *whereInfo;
	WhereAttrInfo *newAttrInfo;

	/* process normal SPJ query */
	whereInfo = GET_WHERE_PROVINFO(query);

	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);
		var = getVarFromTeIfSimple((Node *) te->expr);
		var = resolveToRteVar(var, query);
		Assert(var != NULL); //TODO const ok?

		outVar = makeVar(-1, te->resno, exprType((Node *) te->expr),
				exprTypmod((Node *) te->expr), false);

		newAttrInfo = (WhereAttrInfo *) makeNode(WhereAttrInfo);
		newAttrInfo->outVar = outVar;
		newAttrInfo->inVars = list_make1(copyObject(var));
		newAttrInfo->annotVars = NIL;

		whereInfo->attrInfos = lappend(whereInfo->attrInfos, newAttrInfo);
	}

	generateAnnotBaseQueries(query);

	return query;
}

