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


/*
 * remove dummy provenance attribute target entries added during parse analysis.
 */

void removeDummyProvenanceTEs (Query *query)
{
	ListCell *lc;
	TargetEntry *te;
	int pos;
	List *newTl = NIL;
	pos = 0;

	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);
		if (!isProvAttr(te))
		{
			te->resno = ++pos;
			newTl = lappend(newTl, te);
		}
	}

	query->targetList = newTl;
}
