/*-------------------------------------------------------------------------
 *
 * prov_trans_staticana.h
 *		 : Interface for static analysis of a query tree for transformation provenance rewrite
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_trans_staticana.h,v 1.29 25.08.2009 17:46:38 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_TRANS_STATICANA_H_
#define PROV_TRANS_STATICANA_H_

#include "nodes/parsenodes.h"

extern void analyseStaticTransProv (Query *query);

#endif /* PROV_TRANS_STATICANA_H_ */
