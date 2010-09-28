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
#include "provrewrite/prov_nodes.h"

/* data structures */
typedef struct CaseInfo {
	List *vars;
	List *conditions;
} CaseInfo;

typedef struct VarEqualitiesContext {
	List **result;
	Node *root;
	bool outerJoin;
} VarEqualitiesContext;

/* create a CaseInfo struct */
#define CREATE_CASEINFO(result) \
	do { \
		result = ((CaseInfo *) palloc(sizeof(CaseInfo))); \
		result->vars = NIL; \
		result->conditions = NIL; \
	} while (0)

/* prototypes */
extern void generateCopyMaps (Query *query);
extern bool shouldRewriteQuery (Query *query);
extern bool shouldRewriteRTEforMap (CopyMap *map, Index rtindex);
extern List *getAllEntriesForRTE (CopyMap *map, Index rtindex);
extern CopyMapRelEntry *getEntryForBaseRel (CopyMap *map, Index rtindex);

//extern bool inclusionCondWalker (AttrInclusions *incl,
//		bool (*condWalker) (InclusionCond *cond, void *context),
//		void *context);
//extern void copyMapWalker (List *entries, void *context, void *attrContext,
//		void *inclContext,
//		bool (*relWalker) (CopyMapRelEntry *entry, void *context),
//		bool (*attrWalker) (CopyMapRelEntry *entry, CopyMapEntry *attr,
//				void *context),
//		bool (*inclWalker) (CopyMapRelEntry *entry, CopyMapEntry *attr,
//				AttrInclusions *incl, void *context));
//extern bool dummyAttrWalker (CopyMapRelEntry *entry, CopyMapEntry *attr,
//		void *context);
#endif /*PROV_COPY_MAP_H_*/
