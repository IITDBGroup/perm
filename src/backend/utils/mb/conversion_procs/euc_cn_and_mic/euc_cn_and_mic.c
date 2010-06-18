/*-------------------------------------------------------------------------
 *
 *	  EUC_CN and MULE_INTERNAL
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/utils/mb/conversion_procs/euc_cn_and_mic/euc_cn_and_mic.c,v 1.17 2008/01/01 19:45:53 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "mb/pg_wchar.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(euc_cn_to_mic);
PG_FUNCTION_INFO_V1(mic_to_euc_cn);

extern Datum euc_cn_to_mic(PG_FUNCTION_ARGS);
extern Datum mic_to_euc_cn(PG_FUNCTION_ARGS);

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

static void euc_cn2mic(const unsigned char *euc, unsigned char *p, int len);
static void mic2euc_cn(const unsigned char *mic, unsigned char *p, int len);

Datum
euc_cn_to_mic(PG_FUNCTION_ARGS)
{
	unsigned char *src = (unsigned char *) PG_GETARG_CSTRING(2);
	unsigned char *dest = (unsigned char *) PG_GETARG_CSTRING(3);
	int			len = PG_GETARG_INT32(4);

	Assert(PG_GETARG_INT32(0) == PG_EUC_CN);
	Assert(PG_GETARG_INT32(1) == PG_MULE_INTERNAL);
	Assert(len >= 0);

	euc_cn2mic(src, dest, len);

	PG_RETURN_VOID();
}

Datum
mic_to_euc_cn(PG_FUNCTION_ARGS)
{
	unsigned char *src = (unsigned char *) PG_GETARG_CSTRING(2);
	unsigned char *dest = (unsigned char *) PG_GETARG_CSTRING(3);
	int			len = PG_GETARG_INT32(4);

	Assert(PG_GETARG_INT32(0) == PG_MULE_INTERNAL);
	Assert(PG_GETARG_INT32(1) == PG_EUC_CN);
	Assert(len >= 0);

	mic2euc_cn(src, dest, len);

	PG_RETURN_VOID();
}

/*
 * EUC_CN ---> MIC
 */
static void
euc_cn2mic(const unsigned char *euc, unsigned char *p, int len)
{
	int			c1;

	while (len > 0)
	{
		c1 = *euc;
		if (IS_HIGHBIT_SET(c1))
		{
			if (len < 2 || !IS_HIGHBIT_SET(euc[1]))
				report_invalid_encoding(PG_EUC_CN, (const char *) euc, len);
			*p++ = LC_GB2312_80;
			*p++ = c1;
			*p++ = euc[1];
			euc += 2;
			len -= 2;
		}
		else
		{						/* should be ASCII */
			if (c1 == 0)
				report_invalid_encoding(PG_EUC_CN, (const char *) euc, len);
			*p++ = c1;
			euc++;
			len--;
		}
	}
	*p = '\0';
}

/*
 * MIC ---> EUC_CN
 */
static void
mic2euc_cn(const unsigned char *mic, unsigned char *p, int len)
{
	int			c1;

	while (len > 0)
	{
		c1 = *mic;
		if (IS_HIGHBIT_SET(c1))
		{
			if (c1 != LC_GB2312_80)
				report_untranslatable_char(PG_MULE_INTERNAL, PG_EUC_CN,
										   (const char *) mic, len);
			if (len < 3 || !IS_HIGHBIT_SET(mic[1]) || !IS_HIGHBIT_SET(mic[2]))
				report_invalid_encoding(PG_MULE_INTERNAL,
										(const char *) mic, len);
			mic++;
			*p++ = *mic++;
			*p++ = *mic++;
			len -= 3;
		}
		else
		{						/* should be ASCII */
			if (c1 == 0)
				report_invalid_encoding(PG_MULE_INTERNAL,
										(const char *) mic, len);
			*p++ = c1;
			mic++;
			len--;
		}
	}
	*p = '\0';
}
