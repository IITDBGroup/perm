/*-------------------------------------------------------------------------
 *
 * prov_how_main.c
 *	  PERM C 
 *
 * Portions Copyright (c) 2010 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $Perm: pgsql//SF_Perm/src/backend/provrewrite/howsem/prov_how_main.c,v 1.542 Oct 4, 2010 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "nodes/parsenodes.h"

#include "provrewrite/prov_how_main.h"

Query *
rewriteQueryHow (Query *query)
{
	return query;
}
