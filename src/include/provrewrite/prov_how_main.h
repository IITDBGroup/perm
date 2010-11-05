/*-------------------------------------------------------------------------
 *
 * prov_how_main.h
 *		External interface to 
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql//SF_Perm/src/include/provrewrite/prov_how_main.h ,v 1.29 Oct 4, 2010 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_HOW_MAIN_H_
#define PROV_HOW_MAIN_H_

#include "nodes/parsenodes.h"

extern Query *rewriteQueryHow (Query *query);

#endif /* PROV_HOW_MAIN_H_ */
