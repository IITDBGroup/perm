/*-------------------------------------------------------------------------
 *
 * simple_outxml.h
 *		 :
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/metaq/simple_outxml.h,v 1.29 29.09.2009 18:19:34 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef SIMPLE_OUTXML_H_
#define SIMPLE_OUTXML_H_

#include "lib/stringinfo.h"

extern char *nodeToXmlSimple (void *obj);
extern StringInfo nodeToXmlSimpleStringInfo(void *obj);


#endif /* SIMPLE_OUTXML_H_ */
