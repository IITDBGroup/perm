/*-------------------------------------------------------------------------
 *
 * prov_how_adt.h
 *		External interface to datatype for representation of how-provenance.
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql//SF_Perm/src/include/provrewrite/prov_how_adt.h ,v 1.29 Nov 10, 2010 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_HOW_ADT_H_
#define PROV_HOW_ADT_H_

#include "fmgr.h"

/* datatype for how provenance */
typedef struct HowProv
{
	int32 vl_len_; /* varlena header (do not touch directly!) */
	int32 header_size;
	int32 data_size;
	char data[1];
} HowProv;

/* macros for how provenance handling */
#define DatumGetHowProvP(X)		   	((HowProv *) PG_DETOAST_DATUM(X))
#define DatumGetHowProvPCopy(X)	   	((HowProv *) PG_DETOAST_DATUM_COPY(X))
#define HowProvPGetDatum(x)  		PointerGetDatum(x)
#define PG_GETARG_HOWPROV_P(n)	   	DatumGetHowProvP(PG_GETARG_DATUM(n))
#define PG_GETARG_HOWPROV_P_COPY(n) DatumGetHowProvPCopy(PG_GETARG_DATUM(n))
#define PG_RETURN_HOWPROV_P(x) 		return HowProvPGetDatum(x)

/* macros for accessing parts of a how provenance interal value */
#define HOWHDRSIZE (sizeof(int32) * 2)
#define BITSTOBYTELENGTH(x) ((x +  (BITS_PER_BYTE - 1)) / BITS_PER_BYTE)
#define HOWHEADERBYTES(x) BITSTOBYTELENGTH(x->header_size)
#define HOWTOTALSIZE(x) (HOWHDRSIZE + VARHDRSZ + HOWHEADERBYTES(x) + x->data_size)
#define HOWHEADER(x) ((bits8 *) ((HowProv *) x)->data)
#define HOWDATA(x) ((char *) ((x->data) + HOWHEADERBYTES(x)))
#define HOWNUMELEMS(x) (x->header_size)

extern Datum howprov_in (PG_FUNCTION_ARGS);
extern Datum howprov_out (PG_FUNCTION_ARGS);
extern Datum howprov_out_human (PG_FUNCTION_ARGS);
extern Datum howprov_add (PG_FUNCTION_ARGS);
extern Datum howprov_multiply (PG_FUNCTION_ARGS);
extern Datum oid_to_howprov (PG_FUNCTION_ARGS);

#endif /* PROV_HOW_ADT_H_ */
