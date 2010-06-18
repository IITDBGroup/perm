/*-------------------------------------------------------------------------
 *
 * provrewrite.h
 *		External interface to the provenance rewriter
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql/src/include/provrewrite/provrewrite.h,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_REWRITER_H
#define PROV_REWRITER_H

#include "utils/rel.h"
#include "nodes/parsenodes.h"
//#include "provrewrite/prov_set.h"



/* function prototypes */
extern List *provenanceRewriteQueryList (List *queries);
extern Query *provenanceRewriteQuery (Query *query);
extern Query *rewriteQueryNode (Query * query);
extern Query *rewriteQueryNodeCopy (Query *query);

#endif /* PROV_REWRITER_H */
