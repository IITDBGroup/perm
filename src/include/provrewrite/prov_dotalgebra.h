/*-------------------------------------------------------------------------
 *
 * prov_dotalgebra.h
 *		 :
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_dotalgebra.h,v 1.29 19.11.2008 08:13:36 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_DOTALGEBRA_H_
#define PROV_DOTALGEBRA_H_

#include "nodes/parsenodes.h"
#include "lib/stringinfo.h"

extern void dotQueryAlgebra (Query *query, StringInfo str);

#endif /* PROV_DOTALGEBRA_H_ */
