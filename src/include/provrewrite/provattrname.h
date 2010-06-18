/*-------------------------------------------------------------------------
 *
 * provattrname.h
 *		External interface to the provenance attribute name generator
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql/src/include/provrewrite/provrewrite.h,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROVATTRNAME_H_
#define PROVATTRNAME_H_

#include "utils/rel.h"
#include "nodes/parsenodes.h"

extern const char ProvPraefix[];
extern const char TransProvName[];

typedef struct OidRef {
	Oid relid;
	int refCounter;
} OidRef;

typedef struct RTEref {
	char *name;
	int refCounter;
} RTEref;

extern char *createProvAttrName (RangeTblEntry *rte, char *name);
extern char *getRelationName (Oid relid);
extern char *getRelationNameUnqualified (Oid relid);
extern char *getQueryName (RangeTblEntry *rte);
extern int getRelationRefNum (Oid relid, bool increment);
extern int getQueryRefNum (RangeTblEntry *rte, bool increment);
extern void resetRelReferences (void);
extern bool isProvAttr (TargetEntry *te);
extern char *createExternalProvAttrName (char *name);

#endif /*PROVATTRNAME_H_*/
