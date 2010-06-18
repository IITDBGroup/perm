/*-------------------------------------------------------------------------
 *
 * prov_dotnode.h
 *		 :
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_dotnode.h,v 1.29 09.10.2008 15:21:38 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_DOTNODE_H_
#define PROV_DOTNODE_H_

#include "nodes/parsenodes.h"
#include "lib/stringinfo.h"

/* prototypes */
extern StringInfo dotQuery (Query *query);
extern void showDotQuery (Query *node);

#endif /*PROV_DOTNODE_H_*/

