/*-------------------------------------------------------------------------
 *
 * prov_how_spj.h
 *		External interface to 
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql//SF_Perm/src/include/provrewrite/prov_how_spj.h ,v 1.29 Nov 18, 2010 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_HOW_SPJ_H_
#define PROV_HOW_SPJ_H_

#include "provrewrite/prov_how_spj.h"

extern Query *rewriteHowSPJ (Query *query);
extern void createHowAttr (Query *query, Node *howExpr);
extern void addHowAgg (Query *query);

#endif /* PROV_HOW_SPJ_H_ */
