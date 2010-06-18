/*-------------------------------------------------------------------------
 *
 * simple_outxml.c
 *	  POSTGRES C
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/metaq/simple_outxml.c,v 1.542 29.09.2009 18:19:10 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "lib/stringinfo.h"

#include "metaq/simple_outxml.h"

/*
 *
 */

char *nodeToXmlSimple (void *obj)
{
	return NULL;
}

/*
 *
 */

StringInfo nodeToXmlSimpleStringInfo(void *obj)
{
	StringInfo result;

	return result;
}
