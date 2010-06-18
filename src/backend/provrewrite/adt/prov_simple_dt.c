/*-------------------------------------------------------------------------
 *
 * prov_simple_dt.c
 *	  POSTGRES C 	Naive implementation of a provenance data type that represents provenance as a list of list of tuples.
 *	  				This data type can be used to store provenance in a compressed format in a relation and still be able to
 *	  				query this relation as usual, because no result duplication is necessary. The internal representation of
 *	  				the data type is chosen such that it can be casted to a node structure. Thus, the node output facilities of
 *	  				postgres are used to convert this data type into a string.
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/adt/prov_simple_dt.c,v 1.542 16.06.2009 14:38:03 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "provrewrite/prov_datatype.h"


