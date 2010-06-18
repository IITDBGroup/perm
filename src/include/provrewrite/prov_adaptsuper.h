/*-------------------------------------------------------------------------
 *
 * provrewrite.h
 *		- External interface to the provenance module non-provenance super query handling
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql/src/include/provrewrite/provrewrite.h,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_ADAPTSUPER_H_
#define PROV_ADAPTSUPER_H_

#include "nodes/parsenodes.h"

//extern void adaptSuperQueriesTE (Query *query);
extern void removeDummyProvenanceTEs (Query *query);

#endif /*PROV_ADAPTSUPER_H_*/
