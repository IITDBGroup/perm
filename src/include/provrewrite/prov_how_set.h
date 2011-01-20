/*-------------------------------------------------------------------------
 *
 * prov_how_set.h
 *		External interface to 
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql//SF_Perm/src/include/provrewrite/prov_how_set.h ,v 1.29 Nov 19, 2010 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_HOW_SET_H_
#define PROV_HOW_SET_H_

#include "nodes/parsenodes.h"

extern Query *rewriteHowSet (Query *query);

#endif /* PROV_HOW_SET_H_ */
