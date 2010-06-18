/*-------------------------------------------------------------------------
 *
 * parse_metaq.h
 *		 :
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/metaq/parse_metaq.h,v 1.29 29.09.2009 19:30:44 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PARSE_METAQ_H_
#define PARSE_METAQ_H_

#include "parser/parse_node.h"
#include "provrewrite/prov_nodes.h"

extern Node *transformXsltFuncCall (ParseState *pstate, XsltFuncExpr *xslt);
extern Query *handleThisExprs (Query *query);

#endif /* PARSE_METAQ_H_ */
