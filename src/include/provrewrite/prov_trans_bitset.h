/*-------------------------------------------------------------------------
 *
 * prov_trans_bitset.h
 *		 : bitset (varbit data type) functions needed for transformation provenance computation:
 *		 		- bitor_with_null: a variant of bitor that handles NULL arguments as if they were bitsets containing only 0's.
 *		 		- bitoragg:
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_trans_bitset.h,v 1.29 31.08.2009 16:42:40 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_TRANS_BITSET_H_
#define PROV_TRANS_BITSET_H_

#include "fmgr.h"
#include "utils/varbit.h"
#include "nodes/parsenodes.h"

/* data structures */
typedef struct TransRepQueryInfo
{
	Index queryId;
	char *string;
	VarBit **sets;
	int		*begins;
	int		*ends;
	char 	**stringPointers;
	int numRanges;
	bool hold;
	char *cusorName;
} TransRepQueryInfo;

typedef struct TransRepQueryIndex
{
	List *queryInfos;
} TransRepQueryIndex;

/* macros */
#define VARBITD_LENGTH(datum) \
	(VARBITLEN(DatumGetVarBitP(datum)))

#define VARBITP_OR(left, right) \
	(DatumGetVarBitP(varBitOr(VarBitPGetDatum(left), VarBitPGetDatum(right))))

/* methods */
extern Datum bitor_with_null(PG_FUNCTION_ARGS);
extern Datum reconstructTransToSQL (PG_FUNCTION_ARGS);
extern Datum reconstructMap (PG_FUNCTION_ARGS);
extern Datum bitset_contains(PG_FUNCTION_ARGS);
extern Datum bitset_nonzero_repeat(PG_FUNCTION_ARGS);
extern void dropTransProvQueryIndex (void);
extern int generateTransProvQueryIndex (Query *query, char *cursorName);
extern int generateTransXmlQueryIndex (Query *query, char *cursorName);
extern int generateMapQueryIndex (Query *query, char *cursorName);
extern Datum varBitOr (Datum left, Datum right);
extern VarBit *generateEmptyBitset (int n);
extern Datum generateVarbitSetElem (int n, int value);
extern void releaseAllHolds (void);
extern void releaseHold (char *cursorName);

#endif /* PROV_TRANS_BITSET_H_ */
