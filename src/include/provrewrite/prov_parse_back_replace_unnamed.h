/*-------------------------------------------------------------------------
 *
 * prov_parse_back_replace_unnamed.h
 *		External interface to 
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql//SF_Perm/src/include/provrewrite/prov_parse_back_replace_unnamed.h ,v 1.29 2012-01-17 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_PARSE_BACK_REPLACE_UNNAMED_H_
#define PROV_PARSE_BACK_REPLACE_UNNAMED_H_

#include "nodes/parsenodes.h"

/* indicator for aggproject columns in target lists */
#define AGGPROJ_INDICATOR -666

extern bool replaceUnnamedColumnsWalker (Node *node, void *context);
//extern bool removeAggProjTargetEntriesWalker (Node *node, void *context);

#endif /* PROV_PARSE_BACK_REPLACE_UNNAMED_H_ */
