/*-------------------------------------------------------------------------
 *
 * prov_datatype.h
 *		 :	Functions for the provenance data types that are used to interally represent provenance information.
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_datatype.h,v 1.29 16.06.2009 14:45:26 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_DATATYPE_H_
#define PROV_DATATYPE_H_

extern Datum prov_simple_in(PG_FUNCTION_ARGS);
extern Datum prov_simple_out(PG_FUNCTION_ARGS);

#endif /* PROV_DATATYPE_H_ */
