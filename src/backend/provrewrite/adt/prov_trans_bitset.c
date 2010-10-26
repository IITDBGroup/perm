/*-------------------------------------------------------------------------
 *
 * prov_trans_bitset.c
 *	  PERM C - bitset support functions for transformation provenance.
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/adt/prov_trans_bitset.c,v 1.542 31.08.2009 16:23:59 bglav Exp $
 *
 * NOTES
 *		Transformation provenance is represented as bitsets during computation of
 *		this information. The bitset describes which parts of a query belong to
 *		transformation provenance and which parts do not. So we need several
 *		support functions that implement operations on bitsets (e.g., bit-wise or).
 *		Transformation provenance is presented to a user as either SQL text with
 *		<NOT> and </NOT> surrounding the part of the original query that does not belong to
 *		the transformation provenance or as XML. Both representations are constructed
 *		by a UDF (generateTransProcRep) that takes as input a bitset and returns the
 *		desired representation. To avoid repeated generation of the representation from
 *		a query tree we generate a auxiliary data structure (TransRepQueryInfo) once,
 *		before executing the query and drop it after execution. This structure basically
 *		stores the string representation we want to generate and mapping between indexes
 *		into this string and parts of the query. Thus, the representation function just
 *		has to traverse this structure and copy parts to the output and add the <NOT>-
 *		annotations if necessary.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "fmgr.h"
#include "utils/varbit.h"
#include "utils/memutils.h"

#include "provrewrite/prov_util.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_trans_bitset.h"
#include "provrewrite/prov_trans_util.h"
#include "provrewrite/prov_trans_parse_back.h"
#include "provrewrite/prov_trans_parse_back_xml.h"
#include "provrewrite/prov_trans_map.h"





/* macro to return one of the bitor_with_null function arguments */
#define BITARG_RETURN(arg, result) \
	do { \
		result = (VarBit *) palloc(VARSIZE(arg)); \
		SET_VARSIZE(result, VARSIZE(arg)); \
		VARBITLEN(result) = VARBITLEN(arg); \
		p1 = VARBITS(arg); \
		r = VARBITS(result); \
		for (; p1 < VARBITEND(arg); p1++) \
			*r++ = *p1; \
		mask = BITMASK << VARBITPAD(result); \
		if (mask) \
		{ \
			r--; \
			*r &= mask; \
		} \
		PG_RETURN_VARBIT_P(result); \
	} while (0)

/* global data and consts */
static const char *NOT_STRING = "<NOT>";
static const char *CLOSE_NOT_STRING = "</NOT>";

MemoryContext funcPrivateContext = NULL;
TransRepQueryIndex *transProvQueryIndex = NULL;
int queryId = 0;

/* Functions declarations */
static char *generateTransProvRep (VarBit *bitset, Index queryId);
static bool bitsetContains (VarBit *left, VarBit *right);
static bool bitsetEqual (VarBit *left, VarBit* right);
static char *generateMapRep (VarBit *bitset, Index queryId);
static void listRemoveQueryInfoCell (List **list, ListCell *cell, ListCell *before);
static void freeQueryRepInfo (TransRepQueryInfo *info);

/*
 * Perform a logical OR on two bit strings. If only one argument is NULL than
 * the other one is returned unmodified. This behavior is needed by transformation
 * provenance rewrites for e.g. LEFT JOIN to omit adding additional joins.
 *
 * 		Arguments: varbit, varbit
 */

Datum
bitor_with_null(PG_FUNCTION_ARGS)
{
	VarBit	   *arg1;
	VarBit	   *arg2;
	VarBit	   *result;
	int			len,
				bitlen1,
				bitlen2,
				i;
	bits8	   *p1,
			   *p2,
			   *r;
	bits8		mask;

	/* check if one or both arguments are NULL */
	if (PG_ARGISNULL(0))
	{
		if (PG_ARGISNULL(1))
			PG_RETURN_NULL();

		arg2 = PG_GETARG_VARBIT_P(1);
		BITARG_RETURN(arg2, result);
	}
	else if (PG_ARGISNULL(1))
	{
		arg1 = PG_GETARG_VARBIT_P(0);
		BITARG_RETURN(arg1, result);
	}

	/* both args are not OR them */
	arg1 = PG_GETARG_VARBIT_P(0);
	arg2 = PG_GETARG_VARBIT_P(1);
	bitlen1 = VARBITLEN(arg1);
	bitlen2 = VARBITLEN(arg2);
	if (bitlen1 != bitlen2)
		ereport(ERROR,
				(errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH),
				 errmsg("cannot OR bit strings of different sizes")));
	len = VARSIZE(arg1);
	result = (VarBit *) palloc(len);
	SET_VARSIZE(result, len);
	VARBITLEN(result) = bitlen1;

	p1 = VARBITS(arg1);
	p2 = VARBITS(arg2);
	r = VARBITS(result);
	for (i = 0; i < VARBITBYTES(arg1); i++)
		*r++ = *p1++ | *p2++;

	/* Pad the result */
	mask = BITMASK << VARBITPAD(result);
	if (mask)
	{
		r--;
		*r &= mask;
	}

	PG_RETURN_VARBIT_P(result);
}

/*
 * Generate a annotated SQL or XML (text data type) representation of the transformation
 * provenance of a query result tuple. Transformation provenance is given as a bitset.
 * This function uses the global datastructure transProvQueryIndex. Make sure this is
 * initialized before first query run-time call to function and that it is released
 * after processing the current query.
 * 		Arguments: varbit
 */

Datum
reconstructTransToSQL (PG_FUNCTION_ARGS)
{
	VarBit *transSet;
	char *resultC;
	text *result;
	int len;

	if (PG_ARGISNULL(0))
		transSet = NULL;
	else
		transSet = PG_GETARG_VARBIT_P(0);

	/* generate the string representation */
	resultC = generateTransProvRep(transSet, PG_GETARG_INT32(1));

	if (!resultC)
		PG_RETURN_NULL();

	len = strlen(resultC);
	result = (text *) palloc(len + VARHDRSZ);
	SET_VARSIZE(result, len + VARHDRSZ);

	memcpy(VARDATA(result), resultC, len);

	PG_RETURN_TEXT_P(result);
}

/*
 * Generate a representation of the mapping
 * provenance of a query result tuple. The mapping provenance is given as a bitset.
 * This function uses the global datastructure transProvQueryIndex. Make sure this is
 * initialized before first query run-time call to function and that it is released
 * after processing the current query.
 *
 * 		Arguments: varbit
 */

Datum
reconstructMap (PG_FUNCTION_ARGS)
{
	VarBit *transSet;
	char *resultC;
	text *result;
	int len;

	if (PG_ARGISNULL(0))
		transSet = NULL;
	else
		transSet = PG_GETARG_VARBIT_P(0);

	/* generate the string representation */
	resultC = generateMapRep(transSet, PG_GETARG_INT32(1));

	if (!resultC)
		PG_RETURN_NULL();

	len = strlen(resultC);
	result = (text *) palloc(len + VARHDRSZ);
	SET_VARSIZE(result, len + VARHDRSZ);

	memcpy(VARDATA(result), resultC, len);

	PG_RETURN_TEXT_P(result);
}

/*
 * For two bitsets (Varbit Datums) determine if the right set is included in
 * the left one.
 */

Datum
bitset_contains(PG_FUNCTION_ARGS)
{
	VarBit	   *arg1;
	VarBit	   *arg2;

	if (PG_ARGISNULL(0) ||  PG_ARGISNULL(1))
		PG_RETURN_BOOL(false);

	arg1 = PG_GETARG_VARBIT_P(0);
	arg2 = PG_GETARG_VARBIT_P(1);

	PG_RETURN_BOOL(bitsetContains(arg1,arg2));
}

/*
 *
 */

Datum
bitset_nonzero_repeat(PG_FUNCTION_ARGS)
{
	VarBit *arg1;
	int32 arg2;
	bits8 *p1;
	bits8 temp;
	bool foundOne = false;
	int i;

	if (PG_ARGISNULL(0) ||  PG_ARGISNULL(1))
		PG_RETURN_BOOL(false);

	arg1 = PG_GETARG_VARBIT_P(0);
	arg2 = PG_GETARG_INT32(1);

	p1 = VARBITS(arg1);
	temp = *p1;

	/* for each byte of the varbit set check for each bit if it is set by
	 * shifting and comparing with the high bit. For each sub-bitstring of
	 * length arg2 check that we have found at least one 1 bit */
	for(i = 1; i <= VARBITLEN(arg1); i++)
	{
		foundOne = foundOne || IS_HIGHBIT_SET(temp);

		if (!(i % arg2))
		{
			if (!foundOne)
				PG_RETURN_BOOL(false);
			foundOne = false;
		}

		if (!(i % BITS_PER_BYTE))
		{
			p1++;
			temp = *p1;
		}
		else
			temp <<= 1;
	}

	PG_RETURN_BOOL(true);
}

/*
 * Given a bitset representing the mapping provenance, generate a string representation.
 * The global structure transProvQuerIndex is accessed that stores string representations
 * for the complete mapping provenance (list of all mappings corresponding to the query)
 * and pointers to each part corresponding to bit in the bitset. Thus, this method only
 * traverses the bitset one bit at a time to determine if a certain part should be
 * copied from the global structure to the result or not.
 */

static char *
generateMapRep (VarBit *bitset, Index queryId)
{
	TransRepQueryInfo *queryInfo;
	char *result;
	char *partPointer;
	int resultLength = 0;
	int partLength;
	int numRanges;
	ListCell *lc;
	int i;

	/* get the TransRepQueryInfo for the current query */
	foreach(lc, transProvQueryIndex->queryInfos)
	{
		queryInfo = (TransRepQueryInfo *) lfirst(lc);
		if (queryInfo->queryId  == queryId)
			break;
	}

	if (!bitset)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				errmsg("a NULL value was passed to the transformation"
						"provenance representation generation!")));

	if(!(queryInfo->queryId == queryId))
		ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					errmsg("did not find TransRepQueryInfo needed for map"
							"ping provenance generation!")));

	numRanges = queryInfo->numRanges;

	/* compute length of result */
	for(i = 0; i < numRanges; i++)
	{
		if (bitsetEqual(queryInfo->sets[i], bitset))
			resultLength += queryInfo->ends[i] - queryInfo->begins[i];
	}

	partPointer = result = (char *) palloc(resultLength + 1);

	/* copy string parts to the result */
	for(i = 0; i < numRanges; i++)
	{
		/* a part is only copied if it belongs to the provenance
		 * according to the given bitset.
		 */
		if (bitsetEqual(queryInfo->sets[i], bitset))
		{
			partLength = queryInfo->ends[i] - queryInfo->begins[i];
			memcpy(partPointer, queryInfo->stringPointers[i], partLength);
			partPointer += partLength;
		}
	}

	*partPointer = '\0';

	return result;
}

/*
 * Given a bitset representing the transformation provenance, generate a string
 * representation. The global structure transProvQuerIndex is accessed that
 * stores string representations for the complete transformation provenance and
 * pointers to each part corresponding to bit in the bitset. Thus, this method
 * only traverses the bitset one bit at a time to determine if a certain part
 * should be copied from the global structure to the result or not.
 */

static char *
generateTransProvRep (VarBit *bitset, Index queryId)
{
	TransRepQueryInfo *queryInfo;
	ListCell *lc;
	char *result;
	char *partPointer;
	int resultLength;
	int partLength;
	int numRanges;
	int i, curRange, before;

	foreach(lc, transProvQueryIndex->queryInfos)
	{
		queryInfo = (TransRepQueryInfo *) lfirst(lc);
		if (queryInfo->queryId  == queryId)
			break;
	}

	if (!bitset)
		ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					errmsg("a NULL value was passed to the transformation"
							"provenance representation generation!")));

	if(!(queryInfo->queryId == queryId))
		ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					errmsg("did not find TransRepQueryInfo needed for "
							"transformation provenance generation!")));

	numRanges =  queryInfo->numRanges;
	resultLength = strlen(queryInfo->string);

	/* compute length */
	for(i = curRange = 0; i < numRanges; curRange = ++i)
	{
		/* If not contained in the provenance (bitset) then the result string
		 * is prolonged to account for the <NOT> and </NOT> parts that surround
		 * parts of the query do not belong to the transformation provenance.
		 * In this case all bit elements that represent parts of the query
		 * contained in the current part are skipped, because according to the
		 * definition of transformation provenance that can not belong to the
		 * transformation provenance. */
		if (!bitsetContains(bitset, queryInfo->sets[i]))
		{
			resultLength += strlen(NOT_STRING) + strlen(CLOSE_NOT_STRING);
			while(i < numRanges - 1
					&& queryInfo->ends[i + 1] <= queryInfo->ends[curRange])
				i++;
		}
	}

	partPointer = result = palloc(resultLength + 1);

	/* generate result string */
	for(i = curRange = before = 0; i < numRanges; curRange = ++i)
	{
		/* Part belongs to the transformation provenance. Copy string until
		 * begin of the next part
		 */
		if(bitsetContains(bitset, queryInfo->sets[i]))
		{
			if (i != numRanges - 1)
				partLength = queryInfo->begins[i + 1] - queryInfo->begins[i];
			else
				partLength = strlen(queryInfo->string) - queryInfo->begins[i];

			memcpy(partPointer, queryInfo->stringPointers[i], partLength);
			partPointer += partLength;

			before = i;
		}
		/*
		 * Part does not belong to the transformation provenance. Copy complete
		 * part and skip parts that are subsumed by this part.
		 */
		else
		{
			memcpy(partPointer, NOT_STRING, strlen(NOT_STRING));
			partPointer += strlen(NOT_STRING);

			partLength = queryInfo->ends[i] - queryInfo->begins[i];
			memcpy(partPointer, queryInfo->stringPointers[i], partLength);
			partPointer += partLength;

			memcpy(partPointer, CLOSE_NOT_STRING, strlen(CLOSE_NOT_STRING));
			partPointer += strlen(CLOSE_NOT_STRING);

			before = i;

			while(i < numRanges - 1
					&& queryInfo->ends[i + 1] <= queryInfo->ends[curRange])
				i++;

			if (i < numRanges - 1
					&& queryInfo->ends[before] < queryInfo->begins[i + 1])
			{
				partLength =
						queryInfo->begins[i + 1] - queryInfo->ends[before];
				memcpy(partPointer,
						queryInfo->string + queryInfo->ends[before],
						partLength);
				partPointer += partLength;
			}
		}
	}

	/* If last range does not end at the end of the string add last bit of the
	 * string. */
	if (partPointer - result < resultLength)
	{
		partLength = strlen(queryInfo->string) - queryInfo->ends[before];
		memcpy(partPointer, queryInfo->string + queryInfo->ends[before],
				partLength);
		partPointer += partLength;
	}

	*partPointer = '\0';

	return result;
}

/*
 * Drop TransRepQueryInfo structures that is not needed anymore, because the
 * queries represented by this structures have finshed execution.
 */

void
dropTransProvQueryIndex (void)
{
	ListCell *lc, *before;
	TransRepQueryInfo *queryInfo;

	/* if memory context does not exist create it */
	if (!funcPrivateContext)
	{
		funcPrivateContext = AllocSetContextCreate(TopMemoryContext,
					"TransProv private context",
					ALLOCSET_SMALL_MINSIZE,
					ALLOCSET_SMALL_INITSIZE,
					ALLOCSET_SMALL_MAXSIZE);
	}

	/* create trans prov query index */
	if (transProvQueryIndex == NULL)
	{
		queryId = 0;
		transProvQueryIndex = (TransRepQueryIndex *)
				MemoryContextAlloc(funcPrivateContext,
						sizeof(TransRepQueryIndex));
		transProvQueryIndex->queryInfos = NIL;
	}

	/* drop query infos if they are not needed anymore */
	foreachwithbefore(lc, before, transProvQueryIndex->queryInfos)
	{
		queryInfo = (TransRepQueryInfo *) lfirst(lc);

		if (!queryInfo->hold)
		{
			listRemoveQueryInfoCell(&transProvQueryIndex->queryInfos, lc,
					before);
			lc = before;

			if (transProvQueryIndex->queryInfos == NIL)
				break;
		}
	}
}

/*
 * The hold field of the TransRepQueryInfo structure indicates if it is safe to
 * free this structure. Set the hold field for all queryInfos to true,
 * indicating that they all can be purged.
 */

void
releaseAllHolds (void)
{
	ListCell *lc;
	TransRepQueryInfo *info;

	if (!transProvQueryIndex)
		return;

	foreach(lc, transProvQueryIndex->queryInfos)
	{
		info = (TransRepQueryInfo *) lfirst(lc);
		info->hold = false;
	}
}

/*
 * Release the hold from a TransRepQueryInfo structure representing the query
 * for cursor named "cursorName". This indicates that we safely free the data
 * structure.
 */

void
releaseHold (char *cursorName)
{
	ListCell *lc;
	TransRepQueryInfo *info;

	foreach(lc, transProvQueryIndex->queryInfos)
	{
		info = (TransRepQueryInfo *) lfirst(lc);

		if(strcmp(cursorName, info->cusorName) == 0)
			info->hold = false;
	}
}

/*
 * Removes a given cell from a list pointing to a TransQueryInfo structure and
 * free this structure.
 */

static void
listRemoveQueryInfoCell (List **list, ListCell *cell, ListCell *before)
{
	/* if its the only element, free the whole list structure */
	if (list_length(*list) == 1)
	{
		freeQueryRepInfo((TransRepQueryInfo *) (*list)->head->data.ptr_value);
		pfree(*list);
		*list = NIL;
		return;
	}

	if (cell == (*list)->head)
		(*list)->head = lnext(cell);
	else if (cell != (*list)->tail)
	{
		before->next = NULL;
		(*list)->tail = before;
	}
	else
		before->next = cell->next;

	(*list)->length--;
	freeQueryRepInfo((TransRepQueryInfo *) cell->data.ptr_value);
	pfree(cell);
}

/*
 * Free a TransRepQueryInfo structure.
 */

static void
freeQueryRepInfo (TransRepQueryInfo *info)
{
	int i;
	VarBit *set;

	pfree(info->begins);
	pfree(info->ends);

	if (info->cusorName)
		pfree(info->cusorName);

	/* check if rep info was used for map prov or trans prov because
	 * data structure is used differently for these cases. */
	if (!(info->string))
	{
		for (i = 0; i < info->numRanges; i++)
			pfree(info->stringPointers[i]);
	}
	else
	{
		pfree(info->string);
		//OK because they point into string freed before.
		pfree(info->stringPointers);
	}

	for (i = 0, set = info->sets[0]; i < info->numRanges;
			i++, set = info->sets[i])
		pfree(set);
	pfree(info->sets);

	pfree(info);
}

/*
 * Generate the auxilariy datastructures needed to generate the SQL
 * transformation provenance representation of a query from a bitset. The
 * datastructure is created once per query and then used for each call to the
 * representation function (generateTransProvRep). The index is auxiliary data
 * structures are droped after execution of the query.
 */

int
generateTransProvQueryIndex (Query *query, char *cursorName)
{
	TransRepQueryInfo *newInfo;
	MemoryContext oldCtx;

	newInfo = (TransRepQueryInfo *) MemoryContextAlloc(funcPrivateContext,
			sizeof(TransRepQueryInfo));
	newInfo->queryId = queryId++;
	if (cursorName)
	{
		newInfo->cusorName =
				MemoryContextStrdup(funcPrivateContext, cursorName);
		newInfo->hold = true;
	}
	else
	{
		newInfo->cusorName = NULL;
		newInfo->hold = false;
	}

	oldCtx = MemoryContextSwitchTo(funcPrivateContext);
	transProvQueryIndex->queryInfos =
			lappend(transProvQueryIndex->queryInfos, newInfo);
	MemoryContextSwitchTo(oldCtx);

	parseBackTransToSQL (query, newInfo, funcPrivateContext);

	return newInfo->queryId;
}

/*
 * Generate the auxilariy datastructures needed to generate the xml
 * transformation provenance representation of a query from a bitset. The
 * datastructure is created once per query and then used for each call to the
 * representation function (generateTransProvRep). The index is auxiliary data
 * structures are droped after execution of the query.
 */

int
generateTransXmlQueryIndex (Query *query, char *cursorName)//TODO merge with SQL method if no big changes
{
	TransRepQueryInfo *newInfo;
	MemoryContext oldCtx;

	newInfo = (TransRepQueryInfo *) MemoryContextAlloc(funcPrivateContext,
			sizeof(TransRepQueryInfo));
	newInfo->queryId = queryId++;

	/* Is the query used inside a cursor? */
	if (cursorName)
	{
		newInfo->cusorName = MemoryContextStrdup(funcPrivateContext,
				cursorName);
		newInfo->hold = true;
	}
	else
	{
		newInfo->cusorName = NULL;
		newInfo->hold = false;
	}

	/* generate the data structure inside the private memory context of this
	 * module (It cannot be droped after execution of a query, if we are
	 * fetching from a cursor). */
	oldCtx = MemoryContextSwitchTo(funcPrivateContext);
	transProvQueryIndex->queryInfos =
			lappend(transProvQueryIndex->queryInfos, newInfo);
	MemoryContextSwitchTo(oldCtx);

	//if (Provinfo(query)->contribution == CONTR_TRANS_XML_SIMPLE)
		parseBackTransToXML (query, newInfo, true, funcPrivateContext);

	return newInfo->queryId;
}

/*
 * Generate the auxilariy datastructures needed to generate the mapping
 * provenance representation of a query from a bitset. The datastructure is
 * created once per query and then used for each call to the representation
 * function (generateMapRep). The index is auxiliary data structures are droped
 * after execution of the query.
 */

int
generateMapQueryIndex (Query *query, char *cursorName)
{
	TransRepQueryInfo *newInfo;
	MemoryContext oldCtx;

	/* generate a new TransRepQueryInfo */
	newInfo = (TransRepQueryInfo *) MemoryContextAlloc(funcPrivateContext,
			sizeof(TransRepQueryInfo));
	newInfo->queryId = queryId++;

	/* Is the query used inside a cursor? */
	if (cursorName)
	{
		newInfo->cusorName = MemoryContextStrdup(funcPrivateContext,
				cursorName);
		newInfo->hold = true;
	}
	else
	{
		newInfo->cusorName = NULL;
		newInfo->hold = false;
	}

	/* generate the data structure inside the private memory
	 * context of this module (It cannot be droped after execution
	 * of a query, if we are fetching from a cursos).
	 */
	oldCtx = MemoryContextSwitchTo(funcPrivateContext);
	transProvQueryIndex->queryInfos =
			lappend(transProvQueryIndex->queryInfos, newInfo);
	MemoryContextSwitchTo(oldCtx);

	/* generate the string representation of the mapping provenance,
	 * with pointers into that string used by the representation
	 * generation function.
	 */
	generateMapString(query, newInfo, funcPrivateContext);

	return newInfo->queryId;
}

/*
 * Returns true if the left bitset contains the right one.
 */

static bool
bitsetContains (VarBit *left, VarBit *right)
{
	int bitlen1;
	int bitlen2;
	bits8 *p1;
	bits8 *p2;
	bits8 temp;
	int i;

	bitlen1 = VARBITLEN(left);
	bitlen2 = VARBITLEN(right);
	if (bitlen1 != bitlen2)
		ereport(ERROR,
				(errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH),
				 errmsg("cannot test bit strings of different "
						 "sizes for containment")));

	p1 = VARBITS(left);
	p2 = VARBITS(right);

	/* AND each byte of both bitsets. If the result equals the second bitset
	 * then the first set contains the second. E.g.
	 *		left =  		1100011,1000000
	 *		right = 		0000011,1000000
	 *		left & right = 	0000011,1000000
	 */
	for(i = 0; i < VARBITBYTES(left); i++, p1++, p2++)
	{
		temp = *p1 & *p2;
		if (temp != *p2)
			return FALSE;
	}

	return TRUE;
}

/*
 * Returns true if two bitsets have the same length and contain the same
 * elements.
 */

static bool
bitsetEqual (VarBit *left, VarBit* right)
{
	int bitlen1;
	int bitlen2;
	bits8 *p1;
	bits8 *p2;
	int i;

	bitlen1 = VARBITLEN(left);
	bitlen2 = VARBITLEN(right);
	if (bitlen1 != bitlen2)
		ereport(ERROR,
				(errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH),
				 errmsg("cannot test bit strings of "
						 "different sizes for equality")));

	p1 = VARBITS(left);
	p2 = VARBITS(right);

	for(i = 0; i < VARBITBYTES(left); i++, p1++, p2++)
	{
		if (*p1 != *p2)
			return FALSE;
	}

	return TRUE;
}

/*
 * Generates a Varbit datum that represents a bitset of length n (maximal
 * number of elements) with only the corresponding bit for value set to 1. In
 * other words the singleton bitset containing only value.
 */

Datum
generateVarbitSetElem (int n, int value)
{
	VarBit *result;
	bits8 *cur;
	int realLength;

	Assert(value <= n);

	value--;
	realLength = VARBITTOTALLEN(n);
	result = (VarBit *) palloc0(realLength);
	SET_VARSIZE(result, realLength);
	VARBITLEN(result) = n;

	cur = VARBITS(result);

	/* we do not know size of bit8 definitely so just use this HIGHBIT stuff */
	*cur = HIGHBIT;
	for(;value > 0; value--)
	{
		*cur >>= 1;

		if (*cur == 0)
		{
			cur++;
			*cur = HIGHBIT;
		}
	}

	return VarBitPGetDatum(result);
}

/*
 * Generates an empty bitset (Varbit datum) of length n (the maximal number of
 * elements representable by this bitset).
 */

VarBit *
generateEmptyBitset (int n)
{
	VarBit *result;
	bits8 *cur;
	int realLength;

	realLength = VARBITTOTALLEN(n);
	result = (VarBit *) palloc0(realLength);
	SET_VARSIZE(result, realLength);
	VARBITLEN(result) = n;

	cur = VARBITS(result);
	for(n = VARBITBYTES(result); n > 0; n--, *(cur++) = (bits8) 0)
		;

	return result;
}


/*
 * Bit-wise logical OR for two Varbit datums. The left parameter is
 * is modified and returned as the result.
 */

Datum
varBitOr (Datum left, Datum right)
{
	VarBit *leftBit;
	VarBit *rightBit;
	bits8 *curLeft;
	bits8 *curRight;
	int i;

	/* use as Varbit */
	leftBit = DatumGetVarBitP(left);
	rightBit = DatumGetVarBitP(right);
	curLeft = VARBITS(leftBit);
	curRight = VARBITS(rightBit);

	/* OR-each byte of varbit data */
	for(i = 0; i < VARBITBYTES(leftBit); i++)
	{
		*curLeft = *curLeft | *curRight;
		curLeft++;
		curRight++;
	}

	return left;
}


