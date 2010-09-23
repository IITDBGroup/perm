/*-------------------------------------------------------------------------
 *
 * prov_copy_inclattr.h
 *		External interface to 
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql//SF_Perm/src/include/provrewrite/prov_copy_inclattr.h ,v 1.29 Aug 27, 2010 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_COPY_INCLATTR_H_
#define PROV_COPY_INCLATTR_H_

#include "nodes/parsenodes.h"

extern void addTopCopyInclExpr (Query *query, int numQAttrs);
extern void generateCopyMapAttributs (Query *query, int numQAttrs);

#endif /* PROV_COPY_INCLATTR_H_ */
