/*-------------------------------------------------------------------------
 *
 * outxmlfuncs.h
 *		 : converts a node into its XML representation and return it as a string.
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/metaq/outxmlfuncs.h,v 1.29 15.07.2009 11:46:11 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef OUTXMLFUNCS_H_
#define OUTXMLFUNCS_H_

#include "lib/stringinfo.h"

extern char *nodeToXml (void *obj);
extern StringInfo nodeToXmlStringInfo(void *obj);

#endif /* OUTXMLFUNCS_H_ */
