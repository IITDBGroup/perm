/*-------------------------------------------------------------------------
 *
 * xmlqtree.h
 *		 : Interface to query tree to Xml converter.
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/metaq/xmlqtree.h,v 1.29 15.07.2009 10:55:50 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef XMLQTREE_H_
#define XMLQTREE_H_

#include "nodes/parsenodes.h"
#include "fmgr.h"
#include "utils/xml.h"

/* functions */
extern StringInfo queryTreeToXmlText (Query *query);
extern xmltype *queryTreeToXml (Query *query);
extern Const *queryTreeToXmlConst (Query *query);
extern StringInfo queryTreeToSimpleXmlText (Query *query);
extern xmltype *queryTreeToSimpleXml (Query *query);
extern Const *queryTreeToSimpleXmlConst (Query *query);

/* SQL functions */
extern Datum query_sql_to_xml (PG_FUNCTION_ARGS);
extern Datum query_sql_to_simple_xml (PG_FUNCTION_ARGS);

#endif /* XMLQTREE_H_ */
