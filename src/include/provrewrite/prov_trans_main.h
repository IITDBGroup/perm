/*-------------------------------------------------------------------------
 *
 * prov_trans_main.h
 *		 : Interface to main methods of transformation provenance rewrites.
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_trans_main.h,v 1.29 24.08.2009 14:40:53 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_TRANS_MAIN_H_
#define PROV_TRANS_MAIN_H_

#include "nodes/parsenodes.h"
#include "provrewrite/prov_nodes.h"

/* functions */
extern Query *rewriteQueryTransProv (Query *query, char *cursorName);
extern Query *rewriteQueryNodeTrans (Query *query, RangeTblEntry *parent, Node **parentInfo);

#endif /* PROV_TRANS_MAIN_H_ */
