/*-------------------------------------------------------------------------
 *
 * prov_how_adt.c
 *	  PERM C -
 *
 * Portions Copyright (c) 2010 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $Perm: pgsql//SF_Perm/src/backend/provrewrite/howsem/prov_how_adt.c,v 1.542 Nov 10, 2010 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "fmgr.h"
#include "nodes/parsenodes.h"

#include "provrewrite/prov_how_adt.h"

/* function declarations */
static HowProv *addHowProv(HowProv *left, HowProv *right);
static HowProv *multiplyHowProv(HowProv *left, HowProv *right);
static Oid parseOid (char *input);

/*
 * Input function for how-provenance data type
 */

#define GETC(x) *((char *) x)
#define PEEK(x) *((char *) x + 1)
#define DIGITCASE case '1':case '2':case '3':case '4':case '5':case '6': \
		case '7':case '8':case '9':case '0'
#define ENDORBLANK(x) (x == ' ' || x == '\0')
#define SETBITPOS(ptr,bitpos,value) \
	do { \
		if (value) \
			(*((bits8 *) ptr)) |= bitpos;\
		bitpos >>= 1; \
		if (bitpos == 0) \
		{ \
			ptr++; \
			bitpos = HIGHBIT; \
		} \
	} while (0);

Datum
howprov_in (PG_FUNCTION_ARGS)
{
	char *input_string = PG_GETARG_CSTRING(0);
	HowProv *result;
//	int numElements = 0;
	char *pos = input_string;
	char *token;
	List *tokens = NIL;
//	int inputLength = strlen(input_string);
	uint32 dataLength = 0;
	uint32 headerLength = 0;
	char *dataPtr;
	bits8 *headerPtr;
	ListCell *lc;
	Oid oid;
	bits8 bitPos;

	/* parse input string into list of char * and check if it contains only valid tokens */
	while(GETC(pos) != '\0')
	{
		switch(GETC(pos))
		{
		/* found an numeric token. Append it to list and check that is only
		 * contains digits.
		 */
		DIGITCASE:
		{
			tokens = lappend(tokens, pos);
			dataLength += sizeof(Oid);
			while(isdigit(PEEK(pos++)))
				;
			if(!ENDORBLANK(GETC(pos)))
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("HowProv datatype parsing found number followed by "
						"'%c'",
						GETC(pos))));
		}
		break;
		/* blank is tokens separator skip it */
		case ' ':
			while(PEEK(pos++) == ' ')
				;
			break;
		case '+':
		case '*':
		case '|':
			tokens = lappend(tokens, pos);
			if (!ENDORBLANK(PEEK(pos)))
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("HowProv datatype parsing found a '%c', followed by "
						"'%c' character",
						*pos, PEEK(pos))));
			pos++;
			dataLength++;
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("HowProv datatype parsing found an unexpected '%c' "
					"character, followed by %c character",
					*pos, PEEK(pos))));
			break;
		}
	}

	/* generate the howprov struct and allocate memory for the data */
	headerLength = list_length(tokens);
	result = (HowProv *) palloc0(VARHDRSZ + HOWHDRSIZE + dataLength
			+ BITSTOBYTELENGTH(headerLength));
	result->header_size = headerLength;
	result->data_size = dataLength;
	SET_VARSIZE(result, HOWTOTALSIZE(result));

	/* add the token values to the data section and set the bits in the header
	 * section. Use memcpy to prevent
	 */
	headerPtr = HOWHEADER(result);
	dataPtr = HOWDATA(result);
	bitPos = HIGHBIT;

	//TODO check that the tokes sequence is a valid polish notation of how prov

	foreach(lc, tokens)
	{
		token = (char *) lfirst(lc);

		/* an oid, parse it and copy it */
		if (isdigit(*token))
		{
			oid = parseOid(token);
			memcpy(dataPtr, &oid, sizeof(Oid));
			dataPtr += sizeof(Oid);
			SETBITPOS(headerPtr,bitPos,1);
		}
		/* an operator char. copy it */
		else {
			memcpy(dataPtr, token, 1);
			dataPtr++;
			SETBITPOS(headerPtr,bitPos,0);
		}
	}

	PG_RETURN_HOWPROV_P(result);
}

/*
 *
 */

static Oid
parseOid (char *input)
{
	char *endptr;
	unsigned long cvt;

	errno = 0;
	cvt = strtoul(input, &endptr, 10);

	/*
	 * strtoul() normally only sets ERANGE.  On some systems it also may set
	 * EINVAL, which simply means it couldn't parse the input string. This is
	 * handled by the second "if" consistent across platforms.
	 */
	if (errno && errno != ERANGE && errno != EINVAL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for type oid: \"%s\"",
						input)));

	if (endptr == input && *input != '\0')
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for type oid: \"%s\"",
						input)));

	if (errno == ERANGE)
		ereport(ERROR,
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				 errmsg("value \"%s\" is out of range for type oid", input)));

	return (Oid) cvt;
}

/*
 * Output function for how-provenance data type
 */

#define GETBIT(result,ptr,mask) \
		do { \
			result = (*ptr & mask); \
			if (!mask) \
			{ \
				mask = HIGHBIT; \
				ptr++; \
			} \
			mask >>= 1; \
		} while(0)

Datum
howprov_out (PG_FUNCTION_ARGS)
{
	HowProv *input = PG_GETARG_HOWPROV_P(0);
	StringInfo buf;
	char *result;
	char *dataPtr = HOWDATA(input);
	bits8 *headPtr = HOWHEADER(input);
	bits8 bitMask = HIGHBIT;
	int i;
	bool isOid;
	Oid oid = 0;
	char delim = 0;

	buf = makeStringInfo();

	for(i = 0; i < HOWNUMELEMS(input); i++)
	{
		if (delim)
			appendStringInfoChar(buf, delim);
		else
			delim = ' ';

		GETBIT(isOid,headPtr,bitMask);
		if (isOid)
		{
			memcpy(&oid, dataPtr, sizeof(Oid));
			appendStringInfo(buf, "%u", oid);
			dataPtr += sizeof(Oid);
		}
		else
			appendStringInfoChar(buf, *(dataPtr++));
	}

	/* generate result string and free StringInfo */
	result = palloc(buf->len + 1);
	memcpy(result, buf->data, buf->len);
	result[buf->len] = '\0';

	pfree(buf->data);
	pfree(buf);

	PG_RETURN_CSTRING(result);
}

#define GETBITAT(result,ptr,at) \
		do { \
			bits8 *hByte = ptr + (at / BITS_PER_BYTE); \
			mask = HIGHBIT; \
			mask >>= (at % BITS_PER_BYTE); \
			result = (*hByte & mask); \
		} while(0)

/*
 * The input and internal format of the how provenance data type uses polish
 * notation to represent the How-CS formula. This function transforms the
 * internal representation into a string that is human readable:
 *
 * 	+ 123 * 1 43 55 | | is turned into 123 + (1 * 43 * 55)
 */

Datum
howprov_out_human (PG_FUNCTION_ARGS)
{
	HowProv *input;
	StringInfo buf;
	text *result;
	char *dataPtr;
	bits8 *headPtr;
	bits8 mask;
	char *opStack;
	int *argLengthStack;
	int opStackPos = -1;
	int argStackPos = -1;
	int i;
	Oid oid = 0;
	char op;
	bool bitVal;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	buf = makeStringInfo();

	input = PG_GETARG_HOWPROV_P(0);
	dataPtr = HOWDATA(input);
	headPtr = HOWHEADER(input);
	opStack = (char *) palloc(HOWNUMELEMS(input));
	argLengthStack = (int *) palloc(HOWNUMELEMS(input) * sizeof(int));

	for(i = 0; i < HOWNUMELEMS(input); i++)
	{
		GETBITAT(bitVal, headPtr, i);
		/* a number -> output it */
		if (bitVal)
		{
			if (opStackPos != -1)
			{
				if (argLengthStack[argStackPos] > 0)
					appendStringInfo(buf, " %c ", opStack[opStackPos]);
				argLengthStack[argStackPos]++;
			}
			memcpy(&oid, dataPtr, sizeof(Oid));
			appendStringInfo(buf, "%u", oid);
			dataPtr += sizeof(Oid);
		}
		/* a '|' closes the argument list of the current stack element */
		else if (*dataPtr == '|')
		{
			argStackPos--;
			opStackPos--;
			if (opStackPos != -1)
				appendStringInfoChar(buf,')');
			dataPtr++;
		}
		/* a '+'/'*' are pushed on the stack */
		else
		{
			op = *dataPtr++;

			if (opStackPos != -1)
			{
				if (argLengthStack[argStackPos] > 0)
					appendStringInfo(buf, " %c ", opStack[opStackPos]);
				argLengthStack[argStackPos]++;
				appendStringInfoChar(buf, '(');
			}

			opStack[++opStackPos] = op;
			argLengthStack[++argStackPos] = 0;
		}
	}

	/* generate result string and free StringInfo */
	result = (text *) palloc(buf->len + VARHDRSZ);
	SET_VARSIZE(result, buf->len + VARHDRSZ);
	memcpy(VARDATA(result), buf->data, buf->len);

	pfree(buf->data);
	pfree(buf);

	PG_RETURN_TEXT_P(result);
}

/*
 * Add two how provenance values
 */

Datum
howprov_add (PG_FUNCTION_ARGS)
{
	HowProv *left, *right;
	HowProv *result;

	/* handle NULL values */
	if (PG_ARGISNULL(0) && PG_ARGISNULL(1))
		PG_RETURN_NULL();
	if (PG_ARGISNULL(0))
		PG_RETURN_HOWPROV_P(PG_GETARG_HOWPROV_P(1));
	if (PG_ARGISNULL(1))
		PG_RETURN_HOWPROV_P(PG_GETARG_HOWPROV_P(0));

	left = PG_GETARG_HOWPROV_P(0);
	right = PG_GETARG_HOWPROV_P(1);

	result = addHowProv(left, right);

	PG_RETURN_HOWPROV_P(result);
}

/*
 *
 */

static HowProv *
addHowProv(HowProv *left, HowProv *right)
{
	return left;
}

/*
 * Multiply two how provenance values
 */

Datum
howprov_multiply (PG_FUNCTION_ARGS)
{
	HowProv *left, *right;
	HowProv *result;

	/* handle NULL values */
	if (PG_ARGISNULL(0) && PG_ARGISNULL(1))
		PG_RETURN_NULL();
	if (PG_ARGISNULL(0))
		PG_RETURN_HOWPROV_P(PG_GETARG_HOWPROV_P(1));
	if (PG_ARGISNULL(1))
		PG_RETURN_HOWPROV_P(PG_GETARG_HOWPROV_P(0));

	left = PG_GETARG_HOWPROV_P(0);
	right = PG_GETARG_HOWPROV_P(1);

	result = multiplyHowProv(left, right);

	PG_RETURN_HOWPROV_P(result);
}

/*
 *
 */

static HowProv *
multiplyHowProv(HowProv *left, HowProv *right)
{
	return left;
}
