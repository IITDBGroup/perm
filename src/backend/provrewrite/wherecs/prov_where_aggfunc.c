/*-------------------------------------------------------------------------
 *
 * prov_where_aggfunc.c
 *	  PERM C 
 *
 * Portions Copyright (c) 2010 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $Perm: pgsql//SF_Perm/src/backend/provrewrite/wherecs/prov_where_aggfunc.c,v 1.542 Oct 6, 2010 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "fmgr.h"
#include "access/tupmacs.h"
#include "catalog/pg_type.h"
#include "utils/array.h"

#include "provrewrite/prov_where_aggfunc.h"

/*
 * Append a text datum to a text array if it is not already present in the
 * array.
 */

Datum
textarray_unique_concat(PG_FUNCTION_ARGS)
{
	ArrayType *inArray;
	ArrayType *result;
	text *item;
	char *dataPtr;
	int arrLength, i, bitmask;
	bits8 *bitmap;

	// handle NULL inputs
	if (PG_ARGISNULL(1))
	{
		if (PG_ARGISNULL(0))
			PG_RETURN_NULL();
		PG_RETURN_DATUM(PG_GETARG_DATUM(0));
	}
	else
		item = PG_GETARG_TEXT_PP(1);

	if (PG_ARGISNULL(0)){
		Datum data[1];
		int dim[1];

		dim[0] = 1;
		data[0] = PG_GETARG_DATUM(1);

		result = construct_md_array(data, NULL, 1, dim, dim, TEXTOID, -1,
				false, 'i');
		PG_RETURN_ARRAYTYPE_P(result);
	}
	else
		inArray = PG_GETARG_ARRAYTYPE_P(0);

	// check if element is contained in array
	arrLength = ArrayGetNItems(ARR_NDIM(inArray), ARR_DIMS(inArray));
	dataPtr = ARR_DATA_PTR(inArray);
	bitmap = ARR_NULLBITMAP(inArray);
	bitmask = 1;

	for (i = 0; i < arrLength; i++)
	{
		Datum element;
		text *textElem;

		/* Get element, checking for NULL */
		if (!bitmap || !((*bitmap & bitmask) == 0))
		{
			element = fetch_att(dataPtr, false, -1);
			dataPtr = att_addlength_pointer(dataPtr, -1, dataPtr);
			dataPtr = (char *) att_align_nominal(dataPtr, 'i');
			textElem = DatumGetTextP(element);

			// check for equality
			if (VARSIZE_ANY_EXHDR(item) == VARSIZE_ANY_EXHDR(textElem)
					&& (strncmp(VARDATA_ANY(item), VARDATA_ANY(textElem),
								  VARSIZE_ANY_EXHDR(textElem)) == 0))
			{
				PG_FREE_IF_COPY(inArray, 0);
				PG_RETURN_DATUM(PG_GETARG_DATUM(0));
			}
		}

		/* advance bitmap pointer if any */
		bitmask <<= 1;
		if (bitmask == 0x100)
		{
			if (bitmap)
				bitmap++;
			bitmask = 1;
		}
	}

	// if not contained append it to end of array
	arrLength++;
	result = array_set(inArray, 1, &arrLength, PointerGetDatum(item), false,
			   -1, -1, false, 'i');

	PG_FREE_IF_COPY(inArray, 0);
	PG_FREE_IF_COPY(item, 1);

	PG_RETURN_ARRAYTYPE_P(result);
}

