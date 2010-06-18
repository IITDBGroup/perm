/*-------------------------------------------------------------------------
 *
 * prov_copy_map.h
 *		 : Routines for creation of a CopyMap data structure for a query and to retrieve information from such a data structure.
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_copy_map.h,v 1.29 14.11.2008 08:47:37 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_COPY_MAP_H_
#define PROV_COPY_MAP_H_

#include "nodes/parsenodes.h"
#include "provrewrite/prov_copy_equi.h"
#include "provrewrite/prov_nodes.h"

/* macro to create an empty CopyMap */
#define CREATE_COPYMAP \
	((CopyMap *) palloc(sizeof(CopyMap)))


extern void generateCopyMaps (Query *query);
extern bool isPropagating (CopyMapRelEntry *entry);
extern bool shouldRewriteQuery (Query *query);
extern bool shouldRewriteRTEforMap (CopyMap *map, Index rtindex);
extern List *getAllEntriesForRTE (CopyMap *map, Index rtindex);
extern CopyMapRelEntry *getEntryForBaseRel (CopyMap *map, Index rtindex);

extern void addCopyMapEntry (CopyMap *map, Oid relation);
extern CopyMapEntry *getCopyMapEntry (CopyMap *map, Oid relation);
extern void removeConditionsForAttrs (CopyMap *map, Oid relation);
extern List *getQueryOutAttrs (Query *query);
extern CopyMap *addTransitiveClosure (CopyMap *mapIn, EqGraph *eqGraph);
extern void copyMapWalker (CopyMap *map, void *context, void *attrContext,
							bool (*relWalker) (CopyMapRelEntry *entry, void *context),
							bool (*attrWalker) (CopyMapRelEntry *entry, CopyMapEntry *attr, void *context));

#endif /*PROV_COPY_MAP_H_*/
