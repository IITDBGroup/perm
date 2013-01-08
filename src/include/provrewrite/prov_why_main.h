/*-------------------------------------------------------------------------
 *
 * prov_why_main.h
 *		External interface to 
 *
 *
 * Portions Copyright (c) 2012 Zhen Wang
 * $PostgreSQL: pgsql//SF_Perm/src/include/provrewrite/prov_why_main.h ,v 0.01 Dec 12, 2012 zwang80 Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_WHY_MAIN_H_
#define PROV_WHY_MAIN_H_

#include "nodes/parsenodes.h"
#include "fmgr.h"


extern Query *rewriteQueryWhy (Query *query);
extern Query *rewriteWhyHowProv (Query *query);

extern static void addHWhy ( Query *query);



#endif /* PROV_WHY_MAIN_H_ */
