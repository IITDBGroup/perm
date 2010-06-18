/*-------------------------------------------------------------------------
 *
 *	  EUC_KR <--> UTF8
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/utils/mb/conversion_procs/utf8_and_euc_kr/utf8_and_euc_kr.c,v 1.19 2008/01/01 19:45:54 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "mb/pg_wchar.h"
#include "../../Unicode/euc_kr_to_utf8.map"
#include "../../Unicode/utf8_to_euc_kr.map"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(euc_kr_to_utf8);
PG_FUNCTION_INFO_V1(utf8_to_euc_kr);

extern Datum euc_kr_to_utf8(PG_FUNCTION_ARGS);
extern Datum utf8_to_euc_kr(PG_FUNCTION_ARGS);

/* ----------
 * conv_proc(
 *		INTEGER,	-- source encoding id
 *		INTEGER,	-- destination encoding id
 *		CSTRING,	-- source string (null terminated C string)
 *		CSTRING,	-- destination string (null terminated C string)
 *		INTEGER		-- source string length
 * ) returns VOID;
 * ----------
 */
Datum
euc_kr_to_utf8(PG_FUNCTION_ARGS)
{
	unsigned char *src = (unsigned char *) PG_GETARG_CSTRING(2);
	unsigned char *dest = (unsigned char *) PG_GETARG_CSTRING(3);
	int			len = PG_GETARG_INT32(4);

	Assert(PG_GETARG_INT32(0) == PG_EUC_KR);
	Assert(PG_GETARG_INT32(1) == PG_UTF8);
	Assert(len >= 0);

	LocalToUtf(src, dest, LUmapEUC_KR, NULL,
		   sizeof(LUmapEUC_KR) / sizeof(pg_local_to_utf), 0, PG_EUC_KR, len);

	PG_RETURN_VOID();
}

Datum
utf8_to_euc_kr(PG_FUNCTION_ARGS)
{
	unsigned char *src = (unsigned char *) PG_GETARG_CSTRING(2);
	unsigned char *dest = (unsigned char *) PG_GETARG_CSTRING(3);
	int			len = PG_GETARG_INT32(4);

	Assert(PG_GETARG_INT32(0) == PG_UTF8);
	Assert(PG_GETARG_INT32(1) == PG_EUC_KR);
	Assert(len >= 0);

	UtfToLocal(src, dest, ULmapEUC_KR, NULL,
		   sizeof(ULmapEUC_KR) / sizeof(pg_utf_to_local), 0, PG_EUC_KR, len);

	PG_RETURN_VOID();
}
