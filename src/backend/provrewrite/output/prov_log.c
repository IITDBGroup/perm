/*-------------------------------------------------------------------------
 *
 * provlog.c
 *	  POSTGRES C provenance logging utils
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/provlog.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"					// needed for all stuff

#include "utils/guc.h"
#include "nodes/print.h"
#include "utils/lsyscache.h"
#include "utils/builtins.h"
#include "utils/varbit.h"

#include "metaq/outxmlfuncs.h"
#include "provrewrite/provlog.h"
#include "provrewrite/parse_back_db2.h"

/*
 * Logs a node struct (deep print).
 */

void
logNode (void *node, char *message)
{
	if (Debug_print_rewritten && log_min_messages <= DEBUG1) {
		elog_node_display(DEBUG1, message, node, true);
	}
}

/*
 * Logs a node as XML.
 */

void
logNodeXml (void *node)
{
	ereport (DEBUG1,
					(errcode(ERRCODE_DIVISION_BY_ZERO),
					errmsg("%s",nodeToXml(node))));
}

/*
 * log message with loglevel DEBUG1
 */

void
logDebug (char *message)
{
	ereport (DEBUG1,
				(errcode(ERRCODE_DIVISION_BY_ZERO),
				errmsg("%s",message)));
}

/*
 * log message with loglevel NOTICE
 */

void
logNotice (char *message)
{
	ereport (NOTICE,
				(errcode(ERRCODE_DIVISION_BY_ZERO),
				errmsg("%s",message)));
}


void
logPList (List *pList)
{
	ListCell *lc;
	TargetEntry *te;
	int pos;

	ereport (DEBUG1,
					(errcode(ERRCODE_DIVISION_BY_ZERO),
					errmsg("current pList length is: %i", list_length(pList))));

	pos = 0;
	foreach(lc, pList)
	{
		te = (TargetEntry *) lfirst(lc);

		if(te)
			ereport (DEBUG1,
						(errcode(ERRCODE_DIVISION_BY_ZERO),
						errmsg("%i: <%s>", pos, te->resname)));
		pos++;
	}

}

/*
 * logs a list of T_Value string nodes
 */

void
logValueStringList (List *list)
{
	char *curString;
	ListCell *lc;
	int pos;

	ereport (DEBUG1,
					(errcode(ERRCODE_DIVISION_BY_ZERO),
					errmsg("print T_String list with length: %i",
							list_length(list))));

	pos = 0;
	foreach(lc, list)
	{
		Assert(((Value *) lfirst(lc))->type == T_String);

		curString = (char *) ((Value *) lfirst(lc))->val.str;
		ereport (DEBUG1,
						(errcode(ERRCODE_DIVISION_BY_ZERO),
						errmsg("%i: <%s>", pos, curString)));
		pos++;
	}
}

void
logListLength (List *list)
{
	ereport (DEBUG1,
					(errcode(ERRCODE_DIVISION_BY_ZERO),
					errmsg("list length is: %i", list_length(list))));
}

void
logCharList (List *list)
{
	char *curString;
	ListCell *lc;
	int pos;

	ereport (DEBUG1,
					(errcode(ERRCODE_DIVISION_BY_ZERO),
					errmsg("print char list with length: %i",
							list_length(list))));

	pos = 0;
	foreach(lc, list)
	{
		curString = (char *) lfirst(lc);
		ereport (DEBUG1,
						(errcode(ERRCODE_DIVISION_BY_ZERO),
						errmsg("%i: <%s>", pos, curString)));
		pos++;
	}
}

void
logIntList (List *list)
{
	int curInt;
	ListCell *lc;
	int pos;

	ereport (DEBUG1,
					(errcode(ERRCODE_DIVISION_BY_ZERO),
					errmsg("print int list with length: %i",
							list_length(list))));

	pos = 0;
	foreach(lc, list)
	{
		curInt =  lfirst_int(lc);
		ereport (DEBUG1,
						(errcode(ERRCODE_DIVISION_BY_ZERO),
						errmsg("%i: <%i>", pos, curInt)));
		pos++;
	}
}

/*
 * Given a parse tree this method reconstructs the SQL statement for this parse tree and logs it.
 */

void
logQuerySql (Query *query)
{
	StringInfo buf;

	buf = parseBack(query, true);

	ereport (DEBUG1,
					(errcode(ERRCODE_DIVISION_BY_ZERO),
					errmsg("%s", buf->data)));

	pfree(buf->data);
	pfree(buf);
}

/*
 *
 */

void
logQuerySqlDb2 (Query *query)
{
	StringInfo buf;

	buf = parseBackDB2(query,true);

	ereport (DEBUG1,
					(errcode(ERRCODE_DIVISION_BY_ZERO),
					errmsg("%s", buf->data)));

	pfree(buf->data);
	pfree(buf);
}

/*
 *
 */

char *
bitsetToString (Datum set)
{
	VarBit	   *s = DatumGetVarBitP(set);
	char	   *result,
			   *r;
	bits8	   *sp;
	bits8		x;
	int			i,
				k,
				len;

	len = VARBITLEN(s);
	result = (char *) palloc(len + 1);
	sp = VARBITS(s);
	r = result;
	for (i = 0; i <= len - BITS_PER_BYTE; i += BITS_PER_BYTE, sp++)
	{
		/* print full bytes */
		x = *sp;
		for (k = 0; k < BITS_PER_BYTE; k++)
		{
			*r++ = IS_HIGHBIT_SET(x) ? '1' : '0';
			x <<= 1;
		}
	}
	if (i < len)
	{
		/* print the last partial byte */
		x = *sp;
		for (k = i; k < len; k++)
		{
			*r++ = IS_HIGHBIT_SET(x) ? '1' : '0';
			x <<= 1;
		}
	}
	*r = '\0';

	return result;
}
