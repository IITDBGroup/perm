/*-------------------------------------------------------------------------
 *
 * outxmlfuncs.c
 *	  POSTGRES C - conversion of node structures into XML text.
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/metaq/outxmlfuncs.c,v 1.542 15.07.2009 11:43:51 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <ctype.h>

#include "lib/stringinfo.h"
#include "nodes/plannodes.h"
#include "nodes/relation.h"
#include "utils/datum.h"
#include "fmgr.h"
#include "utils/lsyscache.h"

#include "provrewrite/prov_nodes.h"
#include "metaq/outxmlfuncs.h"

/*
 * Macros to simplify output of different kinds of fields.	Use these
 * wherever possible to reduce the chance for silly typos.	Note that these
 * hard-wire conventions about the names of the local variables in an Out
 * routine.
 */

/* Write a line break */
#define WRITE_BR \
	(appendStringInfoChar(str, '\n'))

/* Write depth times tabs */
#define WRITE_INDENT(depth) \
	do { \
		indendHelper = depth; \
		while ((indendHelper)-- > 0) { \
			appendStringInfoChar(str, ' '); \
		} \
	} while (0)

/* Write an string as an element */
#define WRITE_ELEMENT(name,value) \
	do { \
		WRITE_INDENT(depth); \
		appendStringInfo(str, "<" CppAsString(name) ">%s</" CppAsString(name) ">\n", (value)); \
	} while (0)

/* Header for all output methods */
#define OUTFUNC_HEADER \
	int indendHelper; \
	depth = depth + 1;

/* Write the element start for a node */
#define WRITE_NODE_START(nodelabel) \
	OUTFUNC_HEADER \
	do { \
		WRITE_INDENT(depth - 1); \
		appendStringInfoString(str, "<" nodelabel ">\n"); \
	} while (0)

/* Write the element end for a node */
#define WRITE_NODE_END(nodelabel) \
	do { \
		WRITE_INDENT(depth - 1); \
		appendStringInfoString(str, "</" nodelabel ">\n"); \
	} while (0)

/* Write an integer field (anything written as ":fldname %d") */
#define WRITE_INT_ELEM(fldname) \
	do { \
		WRITE_INDENT(depth); \
		appendStringInfo(str, "<" CppAsString(fldname) ">%d</" CppAsString(fldname) ">\n", node->fldname); \
	} while (0)

/* Write an integer list element */
#define WRITE_INT_LIST_ELEM(value) \
	do { \
		WRITE_INDENT(depth); \
		appendStringInfo(str, "<Int>%d</Int>\n", (value)); \
	} while (0)

/* Write an unsigned integer field (anything written as ":fldname %u") */
#define WRITE_UINT_ELEM(fldname) \
	do { \
		WRITE_INDENT(depth); \
		appendStringInfo(str, "<" CppAsString(fldname) ">%u</" CppAsString(fldname) ">\n", node->fldname); \
	} while (0)

/* Write an OID field (don't hard-wire assumption that OID is same as uint) */
#define WRITE_OID_ELEM(fldname) \
	do { \
		WRITE_INDENT(depth); \
		appendStringInfo(str, "<" CppAsString(fldname) ">%u</" CppAsString(fldname) ">\n", node->fldname); \
	} while (0)

/* Write an integer list element */
#define WRITE_OID_LIST_ELEM(value) \
	do { \
		WRITE_INDENT(depth); \
		appendStringInfo(str, "<Oid>%d</Oid>\n", (value)); \
	} while (0)

/* Write a long-integer field */
#define WRITE_LONG_ELEM(fldname) \
	do { \
		WRITE_INDENT(depth); \
		appendStringInfo(str, "<" CppAsString(fldname) ">%ld</" CppAsString(fldname) ">\n", node->fldname); \
	} while(0)

/* Write an integer list element */
#define WRITE_LONG_LIST_ELEM(value) \
	do { \
		WRITE_INDENT(depth); \
		appendStringInfo(str, "<Int>%ld</Int>\n", (value)); \
	} while (0)

/* Write a char field (ie, one ascii character) */
#define WRITE_CHAR_ELEM(fldname) \
	do { \
		WRITE_INDENT(depth); \
		appendStringInfo(str, "<" CppAsString(fldname) ">%c</" CppAsString(fldname) ">\n", node->fldname); \
	} while (0)

/* Write an enumerated-type field as an integer code */
#define WRITE_ENUM_ELEM(fldname, enumtype) \
	do { \
		WRITE_INDENT(depth); \
		appendStringInfo(str, "<" CppAsString(fldname) ">%d</" CppAsString(fldname) ">\n", \
					 (int) node->fldname); \
	} while (0)

/* Write a float field --- caller must give format to define precision */
#define WRITE_FLOAT_ELEM(fldname,format) \
	do { \
		WRITE_INDENT(depth); \
		appendStringInfo(str, "<" CppAsString(fldname) ">" format "</" CppAsString(fldname) ">\n", node->fldname); \
	} while (0)

/* Write a boolean field */
#define WRITE_BOOL_ELEM(fldname) \
	do { \
		WRITE_INDENT(depth); \
		appendStringInfo(str, "<" CppAsString(fldname) ">%s</" CppAsString(fldname) ">", \
					 booltostr(node->fldname)); \
		WRITE_BR; \
	} while (0)


/* Write a character-string (possibly NULL) field */
#define WRITE_STRING_ELEM(fldname) \
	do { \
		WRITE_INDENT(depth); \
		appendStringInfo(str, "<" CppAsString(fldname) ">"); \
		_outToken(str, node->fldname); \
		appendStringInfo(str, "</" CppAsString(fldname) ">\n"); \
	} while (0)

/* Write a Node field */
#define WRITE_NODE_ELEM(fldname) \
	do { \
		WRITE_INDENT(depth); \
		appendStringInfo(str, "<" CppAsString(fldname) ">\n"); \
		_outNodeXml(str, (void *) node->fldname, depth); \
		WRITE_INDENT(depth); \
		appendStringInfo(str, "</" CppAsString(fldname) ">\n"); \
	} while (0)

/* Write a bitmapset field */
#define WRITE_BITMAPSET_ELEM(fldname) \
	do { \
		WRITE_INDENT(depth); \
		appendStringInfo(str, "<" CppAsString(fldname) ">"); \
		_outBitmapsetXml(str, node->fldname); \
		appendStringInfo(str, "</" CppAsString(fldname) ">\n"); \
	} while (0)

#define booltostr(x)  ((x) ? "true" : "false")

/* Write the end of an element */
#define WRITE_INT_ATTR(fldname) \
	(appendStringInfo(str, " " CppAsString(fldname) "=\"%d\"", node->fldname))

/* declarations */
static void _outNodeXml (StringInfo str, void *obj, int depth);

/*
 * _outToken
 *	  Convert an ordinary string (eg, an identifier) into a form that
 *	  will be decoded back to a plain token by read.c's functions.
 *
 *	  If a null or empty string is given, it is encoded as "<>".
 */
static void
_outToken(StringInfo str, char *s)
{
	if (s == NULL || *s == '\0')
	{
		appendStringInfo(str, "<>");
		return;
	}

	/*
	 * Look for characters or patterns that are treated specially by read.c
	 * (either in pg_strtok() or in nodeRead()), and therefore need a
	 * protective backslash.
	 */
	/* These characters only need to be quoted at the start of the string */
	if (*s == '<' ||
		*s == '\"' ||
		isdigit((unsigned char) *s) ||
		((*s == '+' || *s == '-') &&
		 (isdigit((unsigned char) s[1]) || s[1] == '.')))
		appendStringInfoChar(str, '\\');
	while (*s)
	{
		/* These chars must be backslashed anywhere in the string */
		if (*s == ' ' || *s == '\n' || *s == '\t' ||
			*s == '(' || *s == ')' || *s == '{' || *s == '}' ||
			*s == '\\')
			appendStringInfoChar(str, '\\');
		appendStringInfoChar(str, *s++);
	}
}

static void
_outList(StringInfo str, List *node, int depth)
{
	int indendHelper;
	ListCell   *lc;

	foreach(lc, node)
	{
		if (IsA(node, List))
			_outNodeXml(str, (void *) lfirst(lc), depth);
		else if (IsA(node, IntList))
		{
			WRITE_INT_LIST_ELEM(lfirst_int(lc));
		}
		else if (IsA(node, OidList))
		{
			WRITE_OID_LIST_ELEM(lfirst_oid(lc));
		}
		else
			elog(ERROR, "unrecognized list node type: %d",
				 (int) node->type);
	}
}

/*
 * _outBitmapset -
 *	   converts a bitmap set of integers
 *
 * Note: the output format is "(b int int ...)", similar to an integer List.
 * Currently bitmapsets do not appear in any node type that is stored in
 * rules, so there is no support in readfuncs.c for reading this format.
 */
static void
_outBitmapsetXml(StringInfo str, Bitmapset *bms)
{
	Bitmapset  *tmpset;
	int			x;

	appendStringInfoChar(str, '(');
	appendStringInfoChar(str, 'b');
	tmpset = bms_copy(bms);
	while ((x = bms_first_member(tmpset)) >= 0)
		appendStringInfo(str, " %d", x);
	bms_free(tmpset);
	appendStringInfoChar(str, ')');
}

///*
// * Print the value of a Datum given its type.
// */
//static void
//_outDatum(StringInfo str, Datum value, int typlen, bool typbyval)
//{
//	Size		length,
//				i;
//	char	   *s;
//
//	length = datumGetSize(value, typbyval, typlen);
//
//	if (typbyval)
//	{
//		s = (char *) (&value);
//		appendStringInfo(str, "%u [ ", (unsigned int) length);
//		for (i = 0; i < (Size) sizeof(Datum); i++)
//			appendStringInfo(str, "%d ", (int) (s[i]));
//		appendStringInfo(str, "]");
//	}
//	else
//	{
//		s = (char *) DatumGetPointer(value);
//		if (!PointerIsValid(s))
//			appendStringInfo(str, "0 [ ]");
//		else
//		{
//			appendStringInfo(str, "%u [ ", (unsigned int) length);
//			for (i = 0; i < length; i++)
//				appendStringInfo(str, "%d ", (int) (s[i]));
//			appendStringInfo(str, "]");
//		}
//	}
//}


/*
 *	Stuff from plannodes.h
 */

static void
_outPlannedStmt(StringInfo str, PlannedStmt *node, int depth)
{
	WRITE_NODE_START("PLANNEDSTMT");

	WRITE_ENUM_ELEM(commandType, CmdType);
	WRITE_BOOL_ELEM(canSetTag);
	WRITE_NODE_ELEM(planTree);
	WRITE_NODE_ELEM(rtable);
	WRITE_NODE_ELEM(resultRelations);
	WRITE_NODE_ELEM(utilityStmt);
	WRITE_NODE_ELEM(intoClause);
	WRITE_NODE_ELEM(subplans);
	WRITE_BITMAPSET_ELEM(rewindPlanIDs);
	WRITE_NODE_ELEM(returningLists);
	WRITE_NODE_ELEM(rowMarks);
	WRITE_NODE_ELEM(relationOids);
	WRITE_INT_ELEM(nParamExec);

	WRITE_NODE_END("PLANNEDSTMT");

	WRITE_NODE_END("PLANNEDSTMT");
}

/*
 * print the basic stuff of all nodes that inherit from Plan
 */
static void
_outPlanInfo(StringInfo str, Plan *node, int depth)
{
	int indendHelper;

	WRITE_FLOAT_ELEM(startup_cost, "%.2f");
	WRITE_FLOAT_ELEM(total_cost, "%.2f");
	WRITE_FLOAT_ELEM(plan_rows, "%.0f");
	WRITE_INT_ELEM(plan_width);
	WRITE_NODE_ELEM(targetlist);
	WRITE_NODE_ELEM(qual);
	WRITE_NODE_ELEM(lefttree);
	WRITE_NODE_ELEM(righttree);
	WRITE_NODE_ELEM(initPlan);
	WRITE_BITMAPSET_ELEM(extParam);
	WRITE_BITMAPSET_ELEM(allParam);
}

/*
 * print the basic stuff of all nodes that inherit from Scan
 */
static void
_outScanInfo(StringInfo str, Scan *node, int depth)
{
	int indendHelper;

	_outPlanInfo(str, (Plan *) node, depth);

	WRITE_UINT_ELEM(scanrelid);
}

/*
 * print the basic stuff of all nodes that inherit from Join
 */
static void
_outJoinPlanInfo(StringInfo str, Join *node, int depth)
{
	int indendHelper;

	_outPlanInfo(str, (Plan *) node, depth);

	WRITE_ENUM_ELEM(jointype, JoinType);
	WRITE_NODE_ELEM(joinqual);
}


static void
_outPlan(StringInfo str, Plan *node, int depth)
{
	WRITE_NODE_START("PLAN");

	_outPlanInfo(str, (Plan *) node, depth);

	WRITE_NODE_END("PLAN");
}

static void
_outResult(StringInfo str, Result *node, int depth)
{
	WRITE_NODE_START("RESULT");

	_outPlanInfo(str, (Plan *) node, depth);

	WRITE_NODE_ELEM(resconstantqual);

	WRITE_NODE_END("RESULT");
}

static void
_outAppend(StringInfo str, Append *node, int depth)
{
	WRITE_NODE_START("APPEND");

	_outPlanInfo(str, (Plan *) node, depth);

	WRITE_NODE_ELEM(appendplans);
	WRITE_BOOL_ELEM(isTarget);

	WRITE_NODE_END("APPEND");
}

static void
_outBitmapAnd(StringInfo str, BitmapAnd *node, int depth)
{
	WRITE_NODE_START("BITMAPAND");

	_outPlanInfo(str, (Plan *) node, depth);

	WRITE_NODE_ELEM(bitmapplans);

	WRITE_NODE_END("BITMAPAND");
}

static void
_outBitmapOr(StringInfo str, BitmapOr *node, int depth)
{
	WRITE_NODE_START("BITMAPOR");

	_outPlanInfo(str, (Plan *) node, depth);

	WRITE_NODE_ELEM(bitmapplans);

	WRITE_NODE_END("BITMAPOR");
}

static void
_outScan(StringInfo str, Scan *node, int depth)
{
	WRITE_NODE_START("SCAN");

	_outScanInfo(str, (Scan *) node, depth);

	WRITE_NODE_END("SCAN");
}

static void
_outSeqScan(StringInfo str, SeqScan *node, int depth)
{
	WRITE_NODE_START("SEQSCAN");

	_outScanInfo(str, (Scan *) node, depth);

	WRITE_NODE_END("SEQSCAN");
}

static void
_outIndexScan(StringInfo str, IndexScan *node, int depth)
{
	WRITE_NODE_START("INDEXSCAN");

	_outScanInfo(str, (Scan *) node, depth);

	WRITE_OID_ELEM(indexid);
	WRITE_NODE_ELEM(indexqual);
	WRITE_NODE_ELEM(indexqualorig);
	WRITE_NODE_ELEM(indexstrategy);
	WRITE_NODE_ELEM(indexsubtype);
	WRITE_ENUM_ELEM(indexorderdir, ScanDirection);

	WRITE_NODE_END("INDEXSCAN");
}

static void
_outBitmapIndexScan(StringInfo str, BitmapIndexScan *node, int depth)
{
	WRITE_NODE_START("BITMAPINDEXSCAN");

	_outScanInfo(str, (Scan *) node, depth);

	WRITE_OID_ELEM(indexid);
	WRITE_NODE_ELEM(indexqual);
	WRITE_NODE_ELEM(indexqualorig);
	WRITE_NODE_ELEM(indexstrategy);
	WRITE_NODE_ELEM(indexsubtype);

	WRITE_NODE_END("BITMAPINDEXSCAN");
}

static void
_outBitmapHeapScan(StringInfo str, BitmapHeapScan *node, int depth)
{
	WRITE_NODE_START("BITMAPHEAPSCAN");

	_outScanInfo(str, (Scan *) node, depth);

	WRITE_NODE_ELEM(bitmapqualorig);

	WRITE_NODE_END("BITMAPHEAPSCAN");
}

static void
_outTidScan(StringInfo str, TidScan *node, int depth)
{
	WRITE_NODE_START("TIDSCAN");

	_outScanInfo(str, (Scan *) node, depth);

	WRITE_NODE_ELEM(tidquals);

	WRITE_NODE_END("TIDSCAN");
}

static void
_outSubqueryScan(StringInfo str, SubqueryScan *node, int depth)
{
	WRITE_NODE_START("SUBQUERYSCAN");

	_outScanInfo(str, (Scan *) node, depth);

	WRITE_NODE_ELEM(subplan);
	WRITE_NODE_ELEM(subrtable);

	WRITE_NODE_END("SUBQUERYSCAN");
}

static void
_outFunctionScan(StringInfo str, FunctionScan *node, int depth)
{
	WRITE_NODE_START("FUNCTIONSCAN");

	_outScanInfo(str, (Scan *) node, depth);

	WRITE_NODE_ELEM(funcexpr);
	WRITE_NODE_ELEM(funccolnames);
	WRITE_NODE_ELEM(funccoltypes);
	WRITE_NODE_ELEM(funccoltypmods);

	WRITE_NODE_END("FUNCTIONSCAN");
}

static void
_outValuesScan(StringInfo str, ValuesScan *node, int depth)
{
	WRITE_NODE_START("VALUESSCAN");

	_outScanInfo(str, (Scan *) node, depth);

	WRITE_NODE_ELEM(values_lists);

	WRITE_NODE_END("VALUESSCAN");
}

static void
_outJoin(StringInfo str, Join *node, int depth)
{
	WRITE_NODE_START("JOIN");

	_outJoinPlanInfo(str, (Join *) node, depth);

	WRITE_NODE_END("JOIN");
}

static void
_outNestLoop(StringInfo str, NestLoop *node, int depth)
{
	WRITE_NODE_START("NESTLOOP");

	_outJoinPlanInfo(str, (Join *) node, depth);

	WRITE_NODE_END("NESTLOOP");
}

static void
_outMergeJoin(StringInfo str, MergeJoin *node, int depth)
{
	int			numCols;
	int			i;

	WRITE_NODE_START("MERGEJOIN");

	_outJoinPlanInfo(str, (Join *) node, depth);

	WRITE_NODE_ELEM(mergeclauses);

	numCols = list_length(node->mergeclauses);

	appendStringInfo(str, " :mergeFamilies");
	for (i = 0; i < numCols; i++)
		appendStringInfo(str, " %u", node->mergeFamilies[i]);

	appendStringInfo(str, " :mergeStrategies");
	for (i = 0; i < numCols; i++)
		appendStringInfo(str, " %d", node->mergeStrategies[i]);

	appendStringInfo(str, " :mergeNullsFirst");
	for (i = 0; i < numCols; i++)
		appendStringInfo(str, " %d", (int) node->mergeNullsFirst[i]);

	WRITE_NODE_END("MERGEJOIN");
}

static void
_outHashJoin(StringInfo str, HashJoin *node, int depth)
{
	WRITE_NODE_START("HASHJOIN");

	_outJoinPlanInfo(str, (Join *) node, depth);

	WRITE_NODE_ELEM(hashclauses);

	WRITE_NODE_END("HASHJOIN");
}

static void
_outAgg(StringInfo str, Agg *node, int depth)
{
	int i;

	WRITE_NODE_START("AGG");

	_outPlanInfo(str, (Plan *) node, depth);

	WRITE_ENUM_ELEM(aggstrategy, AggStrategy);
	WRITE_INT_ELEM(numCols);

	appendStringInfo(str, " :grpColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->grpColIdx[i]);

	appendStringInfo(str, " :grpOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->grpOperators[i]);

	WRITE_LONG_ELEM(numGroups);

	WRITE_NODE_END("AGG");
}

static void
_outGroup(StringInfo str, Group *node, int depth)
{
	int			i;

	WRITE_NODE_START("GROUP");

	_outPlanInfo(str, (Plan *) node, depth);

	WRITE_INT_ELEM(numCols);

	appendStringInfo(str, " :grpColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->grpColIdx[i]);

	appendStringInfo(str, " :grpOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->grpOperators[i]);

	WRITE_NODE_END("GROUP");
}

static void
_outMaterial(StringInfo str, Material *node, int depth)
{
	WRITE_NODE_START("MATERIAL");

	_outPlanInfo(str, (Plan *) node, depth);

	WRITE_NODE_END("MATERIAL");
}

static void
_outSort(StringInfo str, Sort *node, int depth)
{
	int			i;

	WRITE_NODE_START("SORT");

	_outPlanInfo(str, (Plan *) node, depth);

	WRITE_INT_ELEM(numCols);

	appendStringInfo(str, " :sortColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->sortColIdx[i]);

	appendStringInfo(str, " :sortOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->sortOperators[i]);

	appendStringInfo(str, " :nullsFirst");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %s", booltostr(node->nullsFirst[i]));

	WRITE_NODE_END("SORT");
}

static void
_outUnique(StringInfo str, Unique *node, int depth)
{
	int			i;

	WRITE_NODE_START("UNIQUE");

	_outPlanInfo(str, (Plan *) node, depth);

	WRITE_INT_ELEM(numCols);

	appendStringInfo(str, " :uniqColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->uniqColIdx[i]);

	appendStringInfo(str, " :uniqOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->uniqOperators[i]);

	WRITE_NODE_END("UNIQUE");
}

static void
_outSetOp(StringInfo str, SetOp *node, int depth)
{
	int			i;

	WRITE_NODE_START("SETOP");

	_outPlanInfo(str, (Plan *) node, depth);

	WRITE_ENUM_ELEM(cmd, SetOpCmd);
	WRITE_INT_ELEM(numCols);

	appendStringInfo(str, " :dupColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->dupColIdx[i]);

	appendStringInfo(str, " :dupOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->dupOperators[i]);

	WRITE_INT_ELEM(flagColIdx);

	WRITE_NODE_END("SETOP");
}

static void
_outLimit(StringInfo str, Limit *node, int depth)
{
	WRITE_NODE_START("LIMIT");

	_outPlanInfo(str, (Plan *) node, depth);

	WRITE_NODE_ELEM(limitOffset);
	WRITE_NODE_ELEM(limitCount);

	WRITE_NODE_END("LIMIT");
}

static void
_outHash(StringInfo str, Hash *node, int depth)
{
	WRITE_NODE_START("HASH");

	_outPlanInfo(str, (Plan *) node, depth);

	WRITE_NODE_END("HASH");
}

/*****************************************************************************
 *
 *	Stuff from primnodes.h.
 *
 *****************************************************************************/

static void
_outAlias(StringInfo str, Alias *node, int depth)
{
	WRITE_NODE_START("ALIAS");

	WRITE_STRING_ELEM(aliasname);
	WRITE_NODE_ELEM(colnames);

	WRITE_NODE_END("ALIAS");
}

static void
_outRangeVar(StringInfo str, RangeVar *node, int depth)
{
	WRITE_NODE_START("RANGEVAR");

	/*
	 * we deliberately ignore catalogname here, since it is presently not
	 * semantically meaningful
	 */
	WRITE_STRING_ELEM(schemaname);
	WRITE_STRING_ELEM(relname);
	WRITE_ENUM_ELEM(inhOpt, InhOption);
	WRITE_BOOL_ELEM(istemp);
	WRITE_NODE_ELEM(alias);
	WRITE_BOOL_ELEM(isProvBase);
	WRITE_NODE_ELEM(provAttrs);

	WRITE_NODE_END("RANGEVAR");
}

static void
_outIntoClause(StringInfo str, IntoClause *node, int depth)
{
	WRITE_NODE_START("INTOCLAUSE");

	WRITE_NODE_ELEM(rel);
	WRITE_NODE_ELEM(colNames);
	WRITE_NODE_ELEM(options);
	WRITE_ENUM_ELEM(onCommit, OnCommitAction);
	WRITE_STRING_ELEM(tableSpaceName);

	WRITE_NODE_END("INTOCLAUSE");
}

static void
_outVar(StringInfo str, Var *node, int depth)
{
	WRITE_NODE_START("VAR");

	WRITE_UINT_ELEM(varno);
	WRITE_INT_ELEM(varattno);
	WRITE_OID_ELEM(vartype);
	WRITE_INT_ELEM(vartypmod);
	WRITE_UINT_ELEM(varlevelsup);
	WRITE_UINT_ELEM(varnoold);
	WRITE_INT_ELEM(varoattno);

	WRITE_NODE_END("VAR");
}

static void
_outConst(StringInfo str, Const *node, int depth)
{
	char *datumString;

	WRITE_NODE_START("CONST");

	WRITE_OID_ELEM(consttype);
	WRITE_INT_ELEM(consttypmod);
	WRITE_INT_ELEM(constlen);
	WRITE_BOOL_ELEM(constbyval);
	WRITE_BOOL_ELEM(constisnull);

	if (node->constisnull)
		WRITE_ELEMENT(constvalue, "null");
	else
	{
		FmgrInfo fInfo;
		Oid fOid;
		bool dummy;

		getTypeOutputInfo(node->consttype, &fOid, &dummy);
		fmgr_info(fOid, &fInfo);
		datumString = OutputFunctionCall(&fInfo,node->constvalue);
		WRITE_ELEMENT(constvalue, datumString);
	}

	WRITE_NODE_END("CONST");
}

static void
_outParam(StringInfo str, Param *node, int depth)
{
	WRITE_NODE_START("PARAM");

	WRITE_ENUM_ELEM(paramkind, ParamKind);
	WRITE_INT_ELEM(paramid);
	WRITE_OID_ELEM(paramtype);
	WRITE_INT_ELEM(paramtypmod);

	WRITE_NODE_END("PARAM");
}

static void
_outAggref(StringInfo str, Aggref *node, int depth)
{
	WRITE_NODE_START("AGGREF");

	WRITE_OID_ELEM(aggfnoid);
	WRITE_OID_ELEM(aggtype);
	WRITE_NODE_ELEM(args);
	WRITE_UINT_ELEM(agglevelsup);
	WRITE_BOOL_ELEM(aggstar);
	WRITE_BOOL_ELEM(aggdistinct);

	WRITE_NODE_END("AGGREF");
}

static void
_outArrayRef(StringInfo str, ArrayRef *node, int depth)
{
	WRITE_NODE_START("ARRAYREF");

	WRITE_OID_ELEM(refarraytype);
	WRITE_OID_ELEM(refelemtype);
	WRITE_INT_ELEM(reftypmod);
	WRITE_NODE_ELEM(refupperindexpr);
	WRITE_NODE_ELEM(reflowerindexpr);
	WRITE_NODE_ELEM(refexpr);
	WRITE_NODE_ELEM(refassgnexpr);

	WRITE_NODE_END("ARRAYREF");
}

static void
_outFuncExpr(StringInfo str, FuncExpr *node, int depth)
{
	WRITE_NODE_START("FUNCEXPR");

	WRITE_OID_ELEM(funcid);
	WRITE_OID_ELEM(funcresulttype);
	WRITE_BOOL_ELEM(funcretset);
	WRITE_ENUM_ELEM(funcformat, CoercionForm);
	WRITE_NODE_ELEM(args);

	WRITE_NODE_END("FUNCEXPR");
}

static void
_outOpExpr(StringInfo str, OpExpr *node, int depth)
{
	WRITE_NODE_START("OPEXPR");

	WRITE_OID_ELEM(opno);
	WRITE_OID_ELEM(opfuncid);
	WRITE_OID_ELEM(opresulttype);
	WRITE_BOOL_ELEM(opretset);
	WRITE_NODE_ELEM(args);

	WRITE_NODE_END("OPEXPR");
}

static void
_outDistinctExpr(StringInfo str, DistinctExpr *node, int depth)
{
	WRITE_NODE_START("DISTINCTEXPR");

	WRITE_OID_ELEM(opno);
	WRITE_OID_ELEM(opfuncid);
	WRITE_OID_ELEM(opresulttype);
	WRITE_BOOL_ELEM(opretset);
	WRITE_NODE_ELEM(args);

	WRITE_NODE_END("DISTINCTEXPR");
}

static void
_outScalarArrayOpExpr(StringInfo str, ScalarArrayOpExpr *node, int depth)
{
	WRITE_NODE_START("SCALARARRAYOPEXPR");

	WRITE_OID_ELEM(opno);
	WRITE_OID_ELEM(opfuncid);
	WRITE_BOOL_ELEM(useOr);
	WRITE_NODE_ELEM(args);

	WRITE_NODE_END("SCALARARRAYOPEXPR");
}

static void
_outBoolExpr(StringInfo str, BoolExpr *node, int depth)
{
	char	   *opstr = NULL;

	WRITE_NODE_START("BOOLEXPR");

	/* do-it-yourself enum representation */
	switch (node->boolop)
	{
		case AND_EXPR:
			opstr = "and";
			break;
		case OR_EXPR:
			opstr = "or";
			break;
		case NOT_EXPR:
			opstr = "not";
			break;
	}
	appendStringInfo(str, " :boolop ");
	_outToken(str, opstr);

	WRITE_NODE_ELEM(args);

	WRITE_NODE_END("BOOLEXPR");
}

static void
_outSubLink(StringInfo str, SubLink *node, int depth)
{
	WRITE_NODE_START("SUBLINK");

	WRITE_ENUM_ELEM(subLinkType, SubLinkType);
	WRITE_NODE_ELEM(testexpr);
	WRITE_NODE_ELEM(operName);
	WRITE_NODE_ELEM(subselect);

	WRITE_NODE_END("SUBLINK");
}

static void
_outSubPlan(StringInfo str, SubPlan *node, int depth)
{
	WRITE_NODE_START("SUBPLAN");

	WRITE_ENUM_ELEM(subLinkType, SubLinkType);
	WRITE_NODE_ELEM(testexpr);
	WRITE_NODE_ELEM(paramIds);
	WRITE_INT_ELEM(plan_id);
	WRITE_OID_ELEM(firstColType);
	WRITE_BOOL_ELEM(useHashTable);
	WRITE_BOOL_ELEM(unknownEqFalse);
	WRITE_NODE_ELEM(setParam);
	WRITE_NODE_ELEM(parParam);
	WRITE_NODE_ELEM(args);

	WRITE_NODE_END("SUBPLAN");
}

static void
_outFieldSelect(StringInfo str, FieldSelect *node, int depth)
{
	WRITE_NODE_START("FIELDSELECT");

	WRITE_NODE_ELEM(arg);
	WRITE_INT_ELEM(fieldnum);
	WRITE_OID_ELEM(resulttype);
	WRITE_INT_ELEM(resulttypmod);

	WRITE_NODE_END("FIELDSELECT");
}

static void
_outFieldStore(StringInfo str, FieldStore *node, int depth)
{
	WRITE_NODE_START("FIELDSTORE");

	WRITE_NODE_ELEM(arg);
	WRITE_NODE_ELEM(newvals);
	WRITE_NODE_ELEM(fieldnums);
	WRITE_OID_ELEM(resulttype);

	WRITE_NODE_END("FIELDSTORE");
}

static void
_outRelabelType(StringInfo str, RelabelType *node, int depth)
{
	WRITE_NODE_START("RELABELTYPE");

	WRITE_NODE_ELEM(arg);
	WRITE_OID_ELEM(resulttype);
	WRITE_INT_ELEM(resulttypmod);
	WRITE_ENUM_ELEM(relabelformat, CoercionForm);

	WRITE_NODE_END("RELABELTYPE");
}

static void
_outCoerceViaIO(StringInfo str, CoerceViaIO *node, int depth)
{
	WRITE_NODE_START("COERCEVIAIO");

	WRITE_NODE_ELEM(arg);
	WRITE_OID_ELEM(resulttype);
	WRITE_ENUM_ELEM(coerceformat, CoercionForm);

	WRITE_NODE_END("COERCEVIAIO");
}

static void
_outArrayCoerceExpr(StringInfo str, ArrayCoerceExpr *node, int depth)
{
	WRITE_NODE_START("ARRAYCOERCEEXPR");

	WRITE_NODE_ELEM(arg);
	WRITE_OID_ELEM(elemfuncid);
	WRITE_OID_ELEM(resulttype);
	WRITE_INT_ELEM(resulttypmod);
	WRITE_BOOL_ELEM(isExplicit);
	WRITE_ENUM_ELEM(coerceformat, CoercionForm);

	WRITE_NODE_END("ARRAYCOERCEEXPR");
}

static void
_outConvertRowtypeExpr(StringInfo str, ConvertRowtypeExpr *node, int depth)
{
	WRITE_NODE_START("CONVERTROWTYPEEXPR");

	WRITE_NODE_ELEM(arg);
	WRITE_OID_ELEM(resulttype);
	WRITE_ENUM_ELEM(convertformat, CoercionForm);

	WRITE_NODE_END("CONVERTROWTYPEEXPR");
}

static void
_outCaseExpr(StringInfo str, CaseExpr *node, int depth)
{
	WRITE_NODE_START("CASE");

	WRITE_OID_ELEM(casetype);
	WRITE_NODE_ELEM(arg);
	WRITE_NODE_ELEM(args);
	WRITE_NODE_ELEM(defresult);

	WRITE_NODE_END("CASE");
}

static void
_outCaseWhen(StringInfo str, CaseWhen *node, int depth)
{
	WRITE_NODE_START("WHEN");

	WRITE_NODE_ELEM(expr);
	WRITE_NODE_ELEM(result);

	WRITE_NODE_END("WHEN");
}

static void
_outCaseTestExpr(StringInfo str, CaseTestExpr *node, int depth)
{
	WRITE_NODE_START("CASETESTEXPR");

	WRITE_OID_ELEM(typeId);
	WRITE_INT_ELEM(typeMod);

	WRITE_NODE_END("CASETESTEXPR");
}

static void
_outArrayExpr(StringInfo str, ArrayExpr *node, int depth)
{
	WRITE_NODE_START("ARRAY");

	WRITE_OID_ELEM(array_typeid);
	WRITE_OID_ELEM(element_typeid);
	WRITE_NODE_ELEM(elements);
	WRITE_BOOL_ELEM(multidims);

	WRITE_NODE_END("ARRAY");
}

static void
_outRowExpr(StringInfo str, RowExpr *node, int depth)
{
	WRITE_NODE_START("ROW");

	WRITE_NODE_ELEM(args);
	WRITE_OID_ELEM(row_typeid);
	WRITE_ENUM_ELEM(row_format, CoercionForm);

	WRITE_NODE_END("ROW");
}

static void
_outRowCompareExpr(StringInfo str, RowCompareExpr *node, int depth)
{
	WRITE_NODE_START("ROWCOMPARE");

	WRITE_ENUM_ELEM(rctype, RowCompareType);
	WRITE_NODE_ELEM(opnos);
	WRITE_NODE_ELEM(opfamilies);
	WRITE_NODE_ELEM(largs);
	WRITE_NODE_ELEM(rargs);

	WRITE_NODE_END("ROWCOMPARE");
}

static void
_outCoalesceExpr(StringInfo str, CoalesceExpr *node, int depth)
{
	WRITE_NODE_START("COALESCE");

	WRITE_OID_ELEM(coalescetype);
	WRITE_NODE_ELEM(args);

	WRITE_NODE_END("COALESCE");
}

static void
_outMinMaxExpr(StringInfo str, MinMaxExpr *node, int depth)
{
	WRITE_NODE_START("MINMAX");

	WRITE_OID_ELEM(minmaxtype);
	WRITE_ENUM_ELEM(op, MinMaxOp);
	WRITE_NODE_ELEM(args);

	WRITE_NODE_END("MINMAX");
}

static void
_outXmlExpr(StringInfo str, XmlExpr *node, int depth)
{
	WRITE_NODE_START("XMLEXPR");

	WRITE_ENUM_ELEM(op, XmlExprOp);
	WRITE_STRING_ELEM(name);
	WRITE_NODE_ELEM(named_args);
	WRITE_NODE_ELEM(arg_names);
	WRITE_NODE_ELEM(args);
	WRITE_ENUM_ELEM(xmloption, XmlOptionType);
	WRITE_OID_ELEM(type);
	WRITE_INT_ELEM(typmod);

	WRITE_NODE_END("XMLEXPR");
}

static void
_outNullIfExpr(StringInfo str, NullIfExpr *node, int depth)
{
	WRITE_NODE_START("NULLIFEXPR");

	WRITE_OID_ELEM(opno);
	WRITE_OID_ELEM(opfuncid);
	WRITE_OID_ELEM(opresulttype);
	WRITE_BOOL_ELEM(opretset);
	WRITE_NODE_ELEM(args);

	WRITE_NODE_END("NULLIFEXPR");
}

static void
_outNullTest(StringInfo str, NullTest *node, int depth)
{
	WRITE_NODE_START("NULLTEST");

	WRITE_NODE_ELEM(arg);
	WRITE_ENUM_ELEM(nulltesttype, NullTestType);

	WRITE_NODE_END("NULLTEST");
}

static void
_outBooleanTest(StringInfo str, BooleanTest *node, int depth)
{
	WRITE_NODE_START("BOOLEANTEST");

	WRITE_NODE_ELEM(arg);
	WRITE_ENUM_ELEM(booltesttype, BoolTestType);

	WRITE_NODE_END("BOOLEANTEST");
}

static void
_outCoerceToDomain(StringInfo str, CoerceToDomain *node, int depth)
{
	WRITE_NODE_START("COERCETODOMAIN");

	WRITE_NODE_ELEM(arg);
	WRITE_OID_ELEM(resulttype);
	WRITE_INT_ELEM(resulttypmod);
	WRITE_ENUM_ELEM(coercionformat, CoercionForm);

	WRITE_NODE_END("COERCETODOMAIN");
}

static void
_outCoerceToDomainValue(StringInfo str, CoerceToDomainValue *node, int depth)
{
	WRITE_NODE_START("COERCETODOMAINVALUE");

	WRITE_OID_ELEM(typeId);
	WRITE_INT_ELEM(typeMod);

	WRITE_NODE_END("COERCETODOMAINVALUE");
}

static void
_outSetToDefault(StringInfo str, SetToDefault *node, int depth)
{
	WRITE_NODE_START("SETTODEFAULT");

	WRITE_OID_ELEM(typeId);
	WRITE_INT_ELEM(typeMod);

	WRITE_NODE_END("SETTODEFAULT");
}

static void
_outCurrentOfExpr(StringInfo str, CurrentOfExpr *node, int depth)
{
	WRITE_NODE_START("CURRENTOFEXPR");

	WRITE_UINT_ELEM(cvarno);
	WRITE_STRING_ELEM(cursor_name);
	WRITE_INT_ELEM(cursor_param);

	WRITE_NODE_END("CURRENTOFEXPR");
}

static void
_outTargetEntry(StringInfo str, TargetEntry *node, int depth)
{
	WRITE_NODE_START("TARGETENTRY");

	WRITE_NODE_ELEM(expr);
	WRITE_INT_ELEM(resno);
	WRITE_STRING_ELEM(resname);
	WRITE_UINT_ELEM(ressortgroupref);
	WRITE_OID_ELEM(resorigtbl);
	WRITE_INT_ELEM(resorigcol);
	WRITE_BOOL_ELEM(resjunk);

	WRITE_NODE_END("TARGETENTRY");
}

static void
_outRangeTblRef(StringInfo str, RangeTblRef *node, int depth)
{
	WRITE_NODE_START("RANGETBLREF");

	WRITE_INT_ELEM(rtindex);

	WRITE_NODE_END("RANGETBLREF");
}

static void
_outJoinExpr(StringInfo str, JoinExpr *node, int depth)
{
	WRITE_NODE_START("JOINEXPR");

	WRITE_ENUM_ELEM(jointype, JoinType);
	WRITE_BOOL_ELEM(isNatural);
	WRITE_NODE_ELEM(larg);
	WRITE_NODE_ELEM(rarg);
	WRITE_NODE_ELEM(using);
	WRITE_NODE_ELEM(quals);
	WRITE_NODE_ELEM(alias);
	WRITE_INT_ELEM(rtindex);

	WRITE_NODE_END("JOINEXPR");
}

static void
_outFromExpr(StringInfo str, FromExpr *node, int depth)
{
	WRITE_NODE_START("FROMEXPR");

	WRITE_NODE_ELEM(fromlist);
	WRITE_NODE_ELEM(quals);

	WRITE_NODE_END("FROMEXPR");
}

/*****************************************************************************
 *
 *	Stuff from relation.h.
 *
 *****************************************************************************/

/*
 * print the basic stuff of all nodes that inherit from Path
 *
 * Note we do NOT print the parent, else we'd be in infinite recursion
 */
static void
_outPathInfo(StringInfo str, Path *node, int depth)
{
	int indendHelper;

	WRITE_ENUM_ELEM(pathtype, NodeTag);
	WRITE_FLOAT_ELEM(startup_cost, "%.2f");
	WRITE_FLOAT_ELEM(total_cost, "%.2f");
	WRITE_NODE_ELEM(pathkeys);
}

/*
 * print the basic stuff of all nodes that inherit from JoinPath
 */
static void
_outJoinPathInfo(StringInfo str, JoinPath *node, int depth)
{
	int indendHelper;

	_outPathInfo(str, (Path *) node, depth);

	WRITE_ENUM_ELEM(jointype, JoinType);
	WRITE_NODE_ELEM(outerjoinpath);
	WRITE_NODE_ELEM(innerjoinpath);
	WRITE_NODE_ELEM(joinrestrictinfo);
}

static void
_outPath(StringInfo str, Path *node, int depth)
{
	WRITE_NODE_START("PATH");

	_outPathInfo(str, (Path *) node, depth);

	WRITE_NODE_END("PATH");
}

static void
_outIndexPath(StringInfo str, IndexPath *node, int depth)
{
	WRITE_NODE_START("INDEXPATH");

	_outPathInfo(str, (Path *) node, depth);

	WRITE_NODE_ELEM(indexinfo);
	WRITE_NODE_ELEM(indexclauses);
	WRITE_NODE_ELEM(indexquals);
	WRITE_BOOL_ELEM(isjoininner);
	WRITE_ENUM_ELEM(indexscandir, ScanDirection);
	WRITE_FLOAT_ELEM(indextotalcost, "%.2f");
	WRITE_FLOAT_ELEM(indexselectivity, "%.4f");
	WRITE_FLOAT_ELEM(rows, "%.0f");

	WRITE_NODE_END("INDEXPATH");
}

static void
_outBitmapHeapPath(StringInfo str, BitmapHeapPath *node, int depth)
{
	WRITE_NODE_START("BITMAPHEAPPATH");

	_outPathInfo(str, (Path *) node, depth);

	WRITE_NODE_ELEM(bitmapqual);
	WRITE_BOOL_ELEM(isjoininner);
	WRITE_FLOAT_ELEM(rows, "%.0f");

	WRITE_NODE_END("BITMAPHEAPPATH");
}

static void
_outBitmapAndPath(StringInfo str, BitmapAndPath *node, int depth)
{
	WRITE_NODE_START("BITMAPANDPATH");

	_outPathInfo(str, (Path *) node, depth);

	WRITE_NODE_ELEM(bitmapquals);
	WRITE_FLOAT_ELEM(bitmapselectivity, "%.4f");

	WRITE_NODE_END("BITMAPANDPATH");
}

static void
_outBitmapOrPath(StringInfo str, BitmapOrPath *node, int depth)
{
	WRITE_NODE_START("BITMAPORPATH");

	_outPathInfo(str, (Path *) node, depth);

	WRITE_NODE_ELEM(bitmapquals);
	WRITE_FLOAT_ELEM(bitmapselectivity, "%.4f");

	WRITE_NODE_END("BITMAPORPATH");
}

static void
_outTidPath(StringInfo str, TidPath *node, int depth)
{
	WRITE_NODE_START("TIDPATH");

	_outPathInfo(str, (Path *) node, depth);

	WRITE_NODE_ELEM(tidquals);

	WRITE_NODE_END("TIDPATH");
}

static void
_outAppendPath(StringInfo str, AppendPath *node, int depth)
{
	WRITE_NODE_START("APPENDPATH");

	_outPathInfo(str, (Path *) node, depth);

	WRITE_NODE_ELEM(subpaths);

	WRITE_NODE_END("APPENDPATH");
}

static void
_outResultPath(StringInfo str, ResultPath *node, int depth)
{
	WRITE_NODE_START("RESULTPATH");

	_outPathInfo(str, (Path *) node, depth);

	WRITE_NODE_ELEM(quals);

	WRITE_NODE_END("RESULTPATH");
}

static void
_outMaterialPath(StringInfo str, MaterialPath *node, int depth)
{
	WRITE_NODE_START("MATERIALPATH");

	_outPathInfo(str, (Path *) node, depth);

	WRITE_NODE_ELEM(subpath);

	WRITE_NODE_END("MATERIALPATH");
}

static void
_outUniquePath(StringInfo str, UniquePath *node, int depth)
{
	WRITE_NODE_START("UNIQUEPATH");

	_outPathInfo(str, (Path *) node, depth);

	WRITE_NODE_ELEM(subpath);
	WRITE_ENUM_ELEM(umethod, UniquePathMethod);
	WRITE_FLOAT_ELEM(rows, "%.0f");

	WRITE_NODE_END("UNIQUEPATH");
}

static void
_outNestPath(StringInfo str, NestPath *node, int depth)
{
	WRITE_NODE_START("NESTPATH");

	_outJoinPathInfo(str, (JoinPath *) node, depth);

	WRITE_NODE_END("NESTPATH");
}

static void
_outMergePath(StringInfo str, MergePath *node, int depth)
{
	WRITE_NODE_START("MERGEPATH");

	_outJoinPathInfo(str, (JoinPath *) node, depth);

	WRITE_NODE_ELEM(path_mergeclauses);
	WRITE_NODE_ELEM(outersortkeys);
	WRITE_NODE_ELEM(innersortkeys);

	WRITE_NODE_END("MERGEPATH");
}

static void
_outHashPath(StringInfo str, HashPath *node, int depth)
{
	WRITE_NODE_START("HASHPATH");

	_outJoinPathInfo(str, (JoinPath *) node, depth);

	WRITE_NODE_ELEM(path_hashclauses);

	WRITE_NODE_END("HASHPATH");
}

static void
_outPlannerGlobal(StringInfo str, PlannerGlobal *node, int depth)
{
	WRITE_NODE_START("PLANNERGLOBAL");

	/* NB: this isn't a complete set of fields */
	WRITE_NODE_ELEM(paramlist);
	WRITE_NODE_ELEM(subplans);
	WRITE_NODE_ELEM(subrtables);
	WRITE_BITMAPSET_ELEM(rewindPlanIDs);
	WRITE_NODE_ELEM(finalrtable);
	WRITE_NODE_ELEM(relationOids);

	WRITE_NODE_END("PLANNERGLOBAL");
}

static void
_outPlannerInfo(StringInfo str, PlannerInfo *node, int depth)
{
	WRITE_NODE_START("PLANNERINFO");

	/* NB: this isn't a complete set of fields */
	WRITE_NODE_ELEM(parse);
	WRITE_NODE_ELEM(glob);
	WRITE_UINT_ELEM(query_level);
	WRITE_NODE_ELEM(join_rel_list);
	WRITE_NODE_ELEM(resultRelations);
	WRITE_NODE_ELEM(returningLists);
	WRITE_NODE_ELEM(init_plans);
	WRITE_NODE_ELEM(eq_classes);
	WRITE_NODE_ELEM(canon_pathkeys);
	WRITE_NODE_ELEM(left_join_clauses);
	WRITE_NODE_ELEM(right_join_clauses);
	WRITE_NODE_ELEM(full_join_clauses);
	WRITE_NODE_ELEM(oj_info_list);
	WRITE_NODE_ELEM(in_info_list);
	WRITE_NODE_ELEM(append_rel_list);
	WRITE_NODE_ELEM(query_pathkeys);
	WRITE_NODE_ELEM(group_pathkeys);
	WRITE_NODE_ELEM(sort_pathkeys);
	WRITE_FLOAT_ELEM(total_table_pages, "%.0f");
	WRITE_FLOAT_ELEM(tuple_fraction, "%.4f");
	WRITE_BOOL_ELEM(hasJoinRTEs);
	WRITE_BOOL_ELEM(hasOuterJoins);
	WRITE_BOOL_ELEM(hasHavingQual);
	WRITE_BOOL_ELEM(hasPseudoConstantQuals);

	WRITE_NODE_END("PLANNERINFO");
}

static void
_outRelOptInfo(StringInfo str, RelOptInfo *node, int depth)
{
	WRITE_NODE_START("RELOPTINFO");

	/* NB: this isn't a complete set of fields */
	WRITE_ENUM_ELEM(reloptkind, RelOptKind);
	WRITE_BITMAPSET_ELEM(relids);
	WRITE_FLOAT_ELEM(rows, "%.0f");
	WRITE_INT_ELEM(width);
	WRITE_NODE_ELEM(reltargetlist);
	WRITE_NODE_ELEM(pathlist);
	WRITE_NODE_ELEM(cheapest_startup_path);
	WRITE_NODE_ELEM(cheapest_total_path);
	WRITE_NODE_ELEM(cheapest_unique_path);
	WRITE_UINT_ELEM(relid);
	WRITE_ENUM_ELEM(rtekind, RTEKind);
	WRITE_INT_ELEM(min_attr);
	WRITE_INT_ELEM(max_attr);
	WRITE_NODE_ELEM(indexlist);
	WRITE_UINT_ELEM(pages);
	WRITE_FLOAT_ELEM(tuples, "%.0f");
	WRITE_NODE_ELEM(subplan);
	WRITE_NODE_ELEM(subrtable);
	WRITE_NODE_ELEM(baserestrictinfo);
	WRITE_NODE_ELEM(joininfo);
	WRITE_BOOL_ELEM(has_eclass_joins);
	WRITE_BITMAPSET_ELEM(index_outer_relids);
	WRITE_NODE_ELEM(index_inner_paths);

	WRITE_NODE_END("RELOPTINFO");
}

static void
_outIndexOptInfo(StringInfo str, IndexOptInfo *node, int depth)
{
	WRITE_NODE_START("INDEXOPTINFO");

	/* NB: this isn't a complete set of fields */
	WRITE_OID_ELEM(indexoid);
	/* Do NOT print rel field, else infinite recursion */
	WRITE_UINT_ELEM(pages);
	WRITE_FLOAT_ELEM(tuples, "%.0f");
	WRITE_INT_ELEM(ncolumns);
	WRITE_NODE_ELEM(indexprs);
	WRITE_NODE_ELEM(indpred);
	WRITE_BOOL_ELEM(predOK);
	WRITE_BOOL_ELEM(unique);

	WRITE_NODE_END("INDEXOPTINFO");
}

static void
_outEquivalenceClass(StringInfo str, EquivalenceClass *node, int depth)
{
	WRITE_NODE_START("EQUIVALENCECLASS");

	/*
	 * To simplify reading, we just chase up to the topmost merged EC and
	 * print that, without bothering to show the merge-ees separately.
	 */
	while (node->ec_merged)
		node = node->ec_merged;

	WRITE_NODE_ELEM(ec_opfamilies);
	WRITE_NODE_ELEM(ec_members);
	WRITE_NODE_ELEM(ec_sources);
	WRITE_NODE_ELEM(ec_derives);
	WRITE_BITMAPSET_ELEM(ec_relids);
	WRITE_BOOL_ELEM(ec_has_const);
	WRITE_BOOL_ELEM(ec_has_volatile);
	WRITE_BOOL_ELEM(ec_below_outer_join);
	WRITE_BOOL_ELEM(ec_broken);
	WRITE_UINT_ELEM(ec_sortref);

	WRITE_NODE_END("EQUIVALENCECLASS");
}

static void
_outEquivalenceMember(StringInfo str, EquivalenceMember *node, int depth)
{
	WRITE_NODE_START("EQUIVALENCEMEMBER");

	WRITE_NODE_ELEM(em_expr);
	WRITE_BITMAPSET_ELEM(em_relids);
	WRITE_BOOL_ELEM(em_is_const);
	WRITE_BOOL_ELEM(em_is_child);
	WRITE_OID_ELEM(em_datatype);

	WRITE_NODE_END("EQUIVALENCEMEMBER");
}

static void
_outPathKey(StringInfo str, PathKey *node, int depth)
{
	WRITE_NODE_START("PATHKEY");

	WRITE_NODE_ELEM(pk_eclass);
	WRITE_OID_ELEM(pk_opfamily);
	WRITE_INT_ELEM(pk_strategy);
	WRITE_BOOL_ELEM(pk_nulls_first);

	WRITE_NODE_END("PATHKEY");
}

static void
_outRestrictInfo(StringInfo str, RestrictInfo *node, int depth)
{
	WRITE_NODE_START("RESTRICTINFO");

	/* NB: this isn't a complete set of fields */
	WRITE_NODE_ELEM(clause);
	WRITE_BOOL_ELEM(is_pushed_down);
	WRITE_BOOL_ELEM(outerjoin_delayed);
	WRITE_BOOL_ELEM(can_join);
	WRITE_BOOL_ELEM(pseudoconstant);
	WRITE_BITMAPSET_ELEM(clause_relids);
	WRITE_BITMAPSET_ELEM(required_relids);
	WRITE_BITMAPSET_ELEM(left_relids);
	WRITE_BITMAPSET_ELEM(right_relids);
	WRITE_NODE_ELEM(orclause);
	/* don't write parent_ec, leads to infinite recursion in plan tree dump */
	WRITE_NODE_ELEM(mergeopfamilies);
	/* don't write left_ec, leads to infinite recursion in plan tree dump */
	/* don't write right_ec, leads to infinite recursion in plan tree dump */
	WRITE_NODE_ELEM(left_em);
	WRITE_NODE_ELEM(right_em);
	WRITE_BOOL_ELEM(outer_is_left);
	WRITE_OID_ELEM(hashjoinoperator);

	WRITE_NODE_END("RESTRICTINFO");
}

static void
_outInnerIndexscanInfo(StringInfo str, InnerIndexscanInfo *node, int depth)
{
	WRITE_NODE_START("INNERINDEXSCANINFO");
	WRITE_BITMAPSET_ELEM(other_relids);
	WRITE_BOOL_ELEM(isouterjoin);
	WRITE_NODE_ELEM(cheapest_startup_innerpath);
	WRITE_NODE_ELEM(cheapest_total_innerpath);

	WRITE_NODE_END("INNERINDEXSCANINFO");
}

static void
_outOuterJoinInfo(StringInfo str, OuterJoinInfo *node, int depth)
{
	WRITE_NODE_START("OUTERJOININFO");

	WRITE_BITMAPSET_ELEM(min_lefthand);
	WRITE_BITMAPSET_ELEM(min_righthand);
	WRITE_BITMAPSET_ELEM(syn_lefthand);
	WRITE_BITMAPSET_ELEM(syn_righthand);
	WRITE_BOOL_ELEM(is_full_join);
	WRITE_BOOL_ELEM(lhs_strict);
	WRITE_BOOL_ELEM(delay_upper_joins);

	WRITE_NODE_END("OUTERJOININFO");
}

static void
_outInClauseInfo(StringInfo str, InClauseInfo *node, int depth)
{
	WRITE_NODE_START("INCLAUSEINFO");

	WRITE_BITMAPSET_ELEM(lefthand);
	WRITE_BITMAPSET_ELEM(righthand);
	WRITE_NODE_ELEM(sub_targetlist);
	WRITE_NODE_ELEM(in_operators);

	WRITE_NODE_END("INCLAUSEINFO");
}

static void
_outAppendRelInfo(StringInfo str, AppendRelInfo *node, int depth)
{
	WRITE_NODE_START("APPENDRELINFO");

	WRITE_UINT_ELEM(parent_relid);
	WRITE_UINT_ELEM(child_relid);
	WRITE_OID_ELEM(parent_reltype);
	WRITE_OID_ELEM(child_reltype);
	WRITE_NODE_ELEM(col_mappings);
	WRITE_NODE_ELEM(translated_vars);
	WRITE_OID_ELEM(parent_reloid);

	WRITE_NODE_END("APPENDRELINFO");
}

static void
_outPlannerParamItem(StringInfo str, PlannerParamItem *node, int depth)
{
	WRITE_NODE_START("PLANNERPARAMITEM");

	WRITE_NODE_ELEM(item);
	WRITE_UINT_ELEM(abslevel);

	WRITE_NODE_END("PLANNERPARAMITEM");
}

/*****************************************************************************
 *
 *	Stuff from parsenodes.h.
 *
 *****************************************************************************/

static void
_outCreateStmt(StringInfo str, CreateStmt *node, int depth)
{
	WRITE_NODE_START("CREATESTMT");

	WRITE_NODE_ELEM(relation);
	WRITE_NODE_ELEM(tableElts);
	WRITE_NODE_ELEM(inhRelations);
	WRITE_NODE_ELEM(constraints);
	WRITE_NODE_ELEM(options);
	WRITE_ENUM_ELEM(oncommit, OnCommitAction);
	WRITE_STRING_ELEM(tablespacename);

	WRITE_NODE_END("CREATESTMT");
}

static void
_outIndexStmt(StringInfo str, IndexStmt *node, int depth)
{
	WRITE_NODE_START("INDEXSTMT");

	WRITE_STRING_ELEM(idxname);
	WRITE_NODE_ELEM(relation);
	WRITE_STRING_ELEM(accessMethod);
	WRITE_STRING_ELEM(tableSpace);
	WRITE_NODE_ELEM(indexParams);
	WRITE_NODE_ELEM(options);
	WRITE_NODE_ELEM(whereClause);
	WRITE_BOOL_ELEM(unique);
	WRITE_BOOL_ELEM(primary);
	WRITE_BOOL_ELEM(isconstraint);
	WRITE_BOOL_ELEM(concurrent);

	WRITE_NODE_END("INDEXSTMT");
}

static void
_outNotifyStmt(StringInfo str, NotifyStmt *node, int depth)
{
	WRITE_NODE_START("NOTIFY");

	WRITE_NODE_ELEM(relation);

	WRITE_NODE_END("NOTIFY");
}

static void
_outDeclareCursorStmt(StringInfo str, DeclareCursorStmt *node, int depth)
{
	WRITE_NODE_START("DECLARECURSOR");

	WRITE_STRING_ELEM(portalname);
	WRITE_INT_ELEM(options);
	WRITE_NODE_ELEM(query);

	WRITE_NODE_END("DECLARECURSOR");
}

static void
_outSelectStmt(StringInfo str, SelectStmt *node, int depth)
{
	WRITE_NODE_START("SELECT");

	WRITE_NODE_ELEM(distinctClause);
	WRITE_NODE_ELEM(intoClause);
	WRITE_NODE_ELEM(targetList);
	WRITE_NODE_ELEM(fromClause);
	WRITE_NODE_ELEM(whereClause);
	WRITE_NODE_ELEM(groupClause);
	WRITE_NODE_ELEM(havingClause);
	WRITE_NODE_ELEM(valuesLists);
	WRITE_NODE_ELEM(sortClause);
	WRITE_NODE_ELEM(limitOffset);
	WRITE_NODE_ELEM(limitCount);
	WRITE_NODE_ELEM(lockingClause);
	WRITE_ENUM_ELEM(op, SetOperation);
	WRITE_BOOL_ELEM(all);
	WRITE_NODE_ELEM(larg);
	WRITE_NODE_ELEM(rarg);
	WRITE_NODE_ELEM(provenanceClause);

	WRITE_NODE_END("SELECT");
}

static void
_outFuncCall(StringInfo str, FuncCall *node, int depth)
{
	WRITE_NODE_START("FUNCCALL");

	WRITE_NODE_ELEM(funcname);
	WRITE_NODE_ELEM(args);
	WRITE_BOOL_ELEM(agg_star);
	WRITE_BOOL_ELEM(agg_distinct);
	WRITE_INT_ELEM(location);

	WRITE_NODE_END("FUNCCALL");
}

static void
_outDefElem(StringInfo str, DefElem *node, int depth)
{
	WRITE_NODE_START("DEFELEM");

	WRITE_STRING_ELEM(defname);
	WRITE_NODE_ELEM(arg);

	WRITE_NODE_END("DEFELEM");
}

static void
_outLockingClause(StringInfo str, LockingClause *node, int depth)
{
	WRITE_NODE_START("LOCKINGCLAUSE");

	WRITE_NODE_ELEM(lockedRels);
	WRITE_BOOL_ELEM(forUpdate);
	WRITE_BOOL_ELEM(noWait);

	WRITE_NODE_END("LOCKINGCLAUSE");
}

static void
_outXmlSerialize(StringInfo str, XmlSerialize *node, int depth)
{
	WRITE_NODE_START("XMLSERIALIZE");

	WRITE_ENUM_ELEM(xmloption, XmlOptionType);
	WRITE_NODE_ELEM(expr);
	WRITE_NODE_ELEM(typename);

	WRITE_NODE_END("XMLSERIALIZE");
}

static void
_outColumnDef(StringInfo str, ColumnDef *node, int depth)
{
	WRITE_NODE_START("COLUMNDEF");

	WRITE_STRING_ELEM(colname);
	WRITE_NODE_ELEM(typename);
	WRITE_INT_ELEM(inhcount);
	WRITE_BOOL_ELEM(is_local);
	WRITE_BOOL_ELEM(is_not_null);
	WRITE_NODE_ELEM(raw_default);
	WRITE_STRING_ELEM(cooked_default);
	WRITE_NODE_ELEM(constraints);

	WRITE_NODE_END("COLUMNDEF");
}

static void
_outTypeName(StringInfo str, TypeName *node, int depth)
{
	WRITE_NODE_START("TYPENAME");

	WRITE_NODE_ELEM(names);
	WRITE_OID_ELEM(typeid);
	WRITE_BOOL_ELEM(timezone);
	WRITE_BOOL_ELEM(setof);
	WRITE_BOOL_ELEM(pct_type);
	WRITE_NODE_ELEM(typmods);
	WRITE_INT_ELEM(typemod);
	WRITE_NODE_ELEM(arrayBounds);
	WRITE_INT_ELEM(location);

	WRITE_NODE_END("TYPENAME");
}

static void
_outTypeCast(StringInfo str, TypeCast *node, int depth)
{
	WRITE_NODE_START("TYPECAST");

	WRITE_NODE_ELEM(arg);
	WRITE_NODE_ELEM(typename);

	WRITE_NODE_END("TYPECAST");
}

static void
_outIndexElem(StringInfo str, IndexElem *node, int depth)
{
	WRITE_NODE_START("INDEXELEM");

	WRITE_STRING_ELEM(name);
	WRITE_NODE_ELEM(expr);
	WRITE_NODE_ELEM(opclass);
	WRITE_ENUM_ELEM(ordering, SortByDir);
	WRITE_ENUM_ELEM(nulls_ordering, SortByNulls);

	WRITE_NODE_END("INDEXELEM");
}

static void
_outQuery(StringInfo str, Query *node, int depth)
{
	WRITE_NODE_START("QUERY");

	WRITE_ENUM_ELEM(commandType, CmdType);
	WRITE_ENUM_ELEM(querySource, QuerySource);
	WRITE_BOOL_ELEM(canSetTag);

	/*
	 * Hack to work around missing outfuncs routines for a lot of the
	 * utility-statement node types.  (The only one we actually *need* for
	 * rules support is NotifyStmt.)  Someday we ought to support 'em all, but
	 * for the meantime do this to avoid getting lots of warnings when running
	 * with debug_print_parse on.
	 */
	if (node->utilityStmt)
	{
		switch (nodeTag(node->utilityStmt))
		{
			case T_CreateStmt:
			case T_IndexStmt:
			case T_NotifyStmt:
			case T_DeclareCursorStmt:
				WRITE_NODE_ELEM(utilityStmt);
				break;
			default:
				WRITE_ELEMENT(utilityStmt, "?");
				break;
		}
	}
	else
		WRITE_ELEMENT(utilityStmt, "");

	WRITE_INT_ELEM(resultRelation);
	WRITE_NODE_ELEM(intoClause);
	WRITE_BOOL_ELEM(hasAggs);
	WRITE_BOOL_ELEM(hasSubLinks);
	WRITE_NODE_ELEM(rtable);
	WRITE_NODE_ELEM(jointree);
	WRITE_NODE_ELEM(targetList);
	WRITE_NODE_ELEM(returningList);
	WRITE_NODE_ELEM(groupClause);
	WRITE_NODE_ELEM(havingQual);
	WRITE_NODE_ELEM(distinctClause);
	WRITE_NODE_ELEM(sortClause);
	WRITE_NODE_ELEM(limitOffset);
	WRITE_NODE_ELEM(limitCount);
	WRITE_NODE_ELEM(rowMarks);
	WRITE_NODE_ELEM(setOperations);
	WRITE_NODE_ELEM(provInfo);

	WRITE_NODE_END("QUERY");
}

static void
_outSortClause(StringInfo str, SortClause *node, int depth)
{
	WRITE_NODE_START("SORTCLAUSE");

	WRITE_UINT_ELEM(tleSortGroupRef);
	WRITE_OID_ELEM(sortop);
	WRITE_BOOL_ELEM(nulls_first);

	WRITE_NODE_END("SORTCLAUSE");
}

static void
_outGroupClause(StringInfo str, GroupClause *node, int depth)
{
	WRITE_NODE_START("GROUPCLAUSE");

	WRITE_UINT_ELEM(tleSortGroupRef);
	WRITE_OID_ELEM(sortop);
	WRITE_BOOL_ELEM(nulls_first);

	WRITE_NODE_END("GROUPCLAUSE");
}

static void
_outRowMarkClause(StringInfo str, RowMarkClause *node, int depth)
{
	WRITE_NODE_START("ROWMARKCLAUSE");

	WRITE_UINT_ELEM(rti);
	WRITE_BOOL_ELEM(forUpdate);
	WRITE_BOOL_ELEM(noWait);

	WRITE_NODE_END("ROWMARKCLAUSE");
}

static void
_outSetOperationStmt(StringInfo str, SetOperationStmt *node, int depth)
{
	WRITE_NODE_START("SETOPERATIONSTMT");

	WRITE_ENUM_ELEM(op, SetOperation);
	WRITE_BOOL_ELEM(all);
	WRITE_NODE_ELEM(larg);
	WRITE_NODE_ELEM(rarg);
	WRITE_NODE_ELEM(colTypes);
	WRITE_NODE_ELEM(colTypmods);

	WRITE_NODE_END("SETOPERATIONSTMT");
}

static void
_outRangeTblEntry(StringInfo str, RangeTblEntry *node, int depth)
{
	WRITE_NODE_START("RTE");

	/* put alias + eref first to make dump more legible */
	WRITE_NODE_ELEM(alias);
	WRITE_NODE_ELEM(eref);
	WRITE_ENUM_ELEM(rtekind, RTEKind);

	switch (node->rtekind)
	{
		case RTE_RELATION:
		case RTE_SPECIAL:
			WRITE_OID_ELEM(relid);
			break;
		case RTE_SUBQUERY:
			WRITE_NODE_ELEM(subquery);
			break;
		case RTE_FUNCTION:
			WRITE_NODE_ELEM(funcexpr);
			WRITE_NODE_ELEM(funccoltypes);
			WRITE_NODE_ELEM(funccoltypmods);
			break;
		case RTE_VALUES:
			WRITE_NODE_ELEM(values_lists);
			break;
		case RTE_JOIN:
			WRITE_ENUM_ELEM(jointype, JoinType);
			WRITE_NODE_ELEM(joinaliasvars);
			break;
		default:
			elog(ERROR, "unrecognized RTE kind: %d", (int) node->rtekind);
			break;
	}

	WRITE_BOOL_ELEM(inh);
	WRITE_BOOL_ELEM(inFromCl);
	WRITE_UINT_ELEM(requiredPerms);
	WRITE_OID_ELEM(checkAsUser);
	WRITE_BOOL_ELEM(isProvBase);
	WRITE_NODE_ELEM(provAttrs);
	WRITE_NODE_ELEM(annotations);

	WRITE_NODE_END("RTE");
}

static void
_outAExpr(StringInfo str, A_Expr *node, int depth)
{
	WRITE_NODE_START("AEXPR");

	switch (node->kind)
	{
		case AEXPR_OP:
			appendStringInfo(str, " ");
			WRITE_NODE_ELEM(name);
			break;
		case AEXPR_AND:
			appendStringInfo(str, " AND");
			break;
		case AEXPR_OR:
			appendStringInfo(str, " OR");
			break;
		case AEXPR_NOT:
			appendStringInfo(str, " NOT");
			break;
		case AEXPR_OP_ANY:
			appendStringInfo(str, " ");
			WRITE_NODE_ELEM(name);
			appendStringInfo(str, " ANY ");
			break;
		case AEXPR_OP_ALL:
			appendStringInfo(str, " ");
			WRITE_NODE_ELEM(name);
			appendStringInfo(str, " ALL ");
			break;
		case AEXPR_DISTINCT:
			appendStringInfo(str, " DISTINCT ");
			WRITE_NODE_ELEM(name);
			break;
		case AEXPR_NULLIF:
			appendStringInfo(str, " NULLIF ");
			WRITE_NODE_ELEM(name);
			break;
		case AEXPR_OF:
			appendStringInfo(str, " OF ");
			WRITE_NODE_ELEM(name);
			break;
		case AEXPR_IN:
			appendStringInfo(str, " IN ");
			WRITE_NODE_ELEM(name);
			break;
		default:
			appendStringInfo(str, " ??");
			break;
	}

	WRITE_NODE_ELEM(lexpr);
	WRITE_NODE_ELEM(rexpr);
	WRITE_INT_ELEM(location);

	WRITE_NODE_END("AEXPR");
}

static void
_outValue(StringInfo str, Value *value, int depth)
{
	OUTFUNC_HEADER;

	switch (value->type)
	{
		case T_Integer:
			WRITE_LONG_LIST_ELEM(value->val.ival);
			break;
		case T_Float:

			/*
			 * We assume the value is a valid numeric literal and so does not
			 * need quoting.
			 */
			appendStringInfoString(str, value->val.str);
			break;
		case T_String:
			WRITE_ELEMENT(String, value->val.str);
			break;
		case T_BitString:
			/* internal representation already has leading 'b' */
			WRITE_ELEMENT(Bitstring, value->val.str);
			break;
		case T_Null:
			/* this is seen only within A_Const, not in transformed trees */
			appendStringInfoString(str, "NULL");
			break;
		default:
			elog(ERROR, "unrecognized node type: %d", (int) value->type);
			break;
	}
}

static void
_outColumnRef(StringInfo str, ColumnRef *node, int depth)
{
	WRITE_NODE_START("COLUMNREF");

	WRITE_NODE_ELEM(fields);
	WRITE_INT_ELEM(location);

	WRITE_NODE_END("COLUMNREF");
}

static void
_outParamRef(StringInfo str, ParamRef *node, int depth)
{
	WRITE_NODE_START("PARAMREF");

	WRITE_INT_ELEM(number);

	WRITE_NODE_END("PARAMREF");
}

static void
_outAConst(StringInfo str, A_Const *node, int depth)
{
	WRITE_NODE_START("A_CONST");

	appendStringInfo(str, " :val ");
	_outValue(str, &(node->val), depth);
	WRITE_NODE_ELEM(typename);

	WRITE_NODE_END("A_CONST");
}

static void
_outA_Indices(StringInfo str, A_Indices *node, int depth)
{
	WRITE_NODE_START("A_INDICES");

	WRITE_NODE_ELEM(lidx);
	WRITE_NODE_ELEM(uidx);

	WRITE_NODE_END("A_INDICES");
}

static void
_outA_Indirection(StringInfo str, A_Indirection *node, int depth)
{
	WRITE_NODE_START("A_INDIRECTION");

	WRITE_NODE_ELEM(arg);
	WRITE_NODE_ELEM(indirection);

	WRITE_NODE_END("A_INDIRECTION");
}

static void
_outResTarget(StringInfo str, ResTarget *node, int depth)
{
	WRITE_NODE_START("RESTARGET");

	WRITE_STRING_ELEM(name);
	WRITE_NODE_ELEM(indirection);
	WRITE_NODE_ELEM(val);
	WRITE_INT_ELEM(location);

	WRITE_NODE_END("RESTARGET");
}

static void
_outConstraint(StringInfo str, Constraint *node, int depth)
{
	WRITE_NODE_START("CONSTRAINT");

	WRITE_STRING_ELEM(name);

	appendStringInfo(str, " :contype ");
	switch (node->contype)
	{
		case CONSTR_PRIMARY:
			appendStringInfo(str, "PRIMARY_KEY");
			WRITE_NODE_ELEM(keys);
			WRITE_NODE_ELEM(options);
			WRITE_STRING_ELEM(indexspace);
			break;

		case CONSTR_UNIQUE:
			appendStringInfo(str, "UNIQUE");
			WRITE_NODE_ELEM(keys);
			WRITE_NODE_ELEM(options);
			WRITE_STRING_ELEM(indexspace);
			break;

		case CONSTR_CHECK:
			appendStringInfo(str, "CHECK");
			WRITE_NODE_ELEM(raw_expr);
			WRITE_STRING_ELEM(cooked_expr);
			break;

		case CONSTR_DEFAULT:
			appendStringInfo(str, "DEFAULT");
			WRITE_NODE_ELEM(raw_expr);
			WRITE_STRING_ELEM(cooked_expr);
			break;

		case CONSTR_NOTNULL:
			appendStringInfo(str, "NOT_NULL");
			break;

		default:
			appendStringInfo(str, "<unrecognized_constraint>");
			break;
	}

	WRITE_NODE_END("CONSTRAINT");
}

static void
_outFkConstraint(StringInfo str, FkConstraint *node, int depth)
{
	WRITE_NODE_START("FKCONSTRAINT");

	WRITE_STRING_ELEM(constr_name);
	WRITE_NODE_ELEM(pktable);
	WRITE_NODE_ELEM(fk_attrs);
	WRITE_NODE_ELEM(pk_attrs);
	WRITE_CHAR_ELEM(fk_matchtype);
	WRITE_CHAR_ELEM(fk_upd_action);
	WRITE_CHAR_ELEM(fk_del_action);
	WRITE_BOOL_ELEM(deferrable);
	WRITE_BOOL_ELEM(initdeferred);
	WRITE_BOOL_ELEM(skip_validation);

	WRITE_NODE_END("FKCONSTRAINT");
}

static void
_outSublinkInfo(StringInfo str, SublinkInfo *node, int depth)
{
	WRITE_NODE_START("SUBLINKINFO");

	WRITE_INT_ELEM(sublinkPos);
	WRITE_ENUM_ELEM(location, SublinkLocation);
	WRITE_ENUM_ELEM(category, SublinkCat);
	WRITE_ENUM_ELEM(aggLoc, SublinkAggLocation);
	WRITE_NODE_ELEM(sublink);
	WRITE_NODE_ELEM(parent);
	//WRITE_POINTER_ELEM(grandParentExprPointer);
	WRITE_NODE_ELEM(exprRoot);
	WRITE_NODE_ELEM(rootCopy);
	//WRITE_POINTER_ELEM(subLinkExprPointer);
	WRITE_NODE_ELEM(aggOrGroup);
	//WRITE_POINTER_ELEM(aggOrGroupPointer);
	WRITE_NODE_ELEM(corrVarInfos);
//	WRITE_NODE_ELEM(corrSuperInsideVars);
//	WRITE_NODE_ELEM(corrSubVars);
	WRITE_NODE_ELEM(condRTEs);
	WRITE_NODE_ELEM(corrRTEs);
	WRITE_NODE_ELEM(targetVar);
	WRITE_INT_ELEM(sublinkRTindex);
	WRITE_NODE_ELEM(subLinkJoinQuery);
	WRITE_INT_ELEM(leftJoinPos);
	WRITE_NODE_ELEM(rewrittenSublinkQuery);

	WRITE_NODE_END("SUBLINKINFO");
}

static void
_outCorrVarInfo(StringInfo str, CorrVarInfo *node, int depth)
{
	WRITE_NODE_START("CORRVARINFO");

	WRITE_NODE_ELEM(corrVar);
	WRITE_NODE_ELEM(vars);
	WRITE_NODE_ELEM(parent);
	WRITE_NODE_ELEM(parentQuery);
	WRITE_NODE_ELEM(exprRoot);
	WRITE_BOOL_ELEM(outside);
	WRITE_NODE_ELEM(refRTE);
	WRITE_ENUM_ELEM(location, SublinkLocation);
	WRITE_INT_ELEM(trueVarLevelsUp);
	WRITE_BOOL_ELEM(belowAgg);
	WRITE_BOOL_ELEM(belowSet);

	WRITE_NODE_END("CORRVARINFO");
}

static void
_outProvInfo(StringInfo str, ProvInfo *node, int depth)
{
	WRITE_NODE_START("PROVINFO");

	WRITE_BOOL_ELEM(provSublinkRewritten);
	WRITE_BOOL_ELEM(shouldRewrite);
	WRITE_ENUM_ELEM(contribution, ContributionType);
	WRITE_NODE_ELEM(copyInfo);
	WRITE_NODE_ELEM(rewriteInfo);
	WRITE_NODE_ELEM(annotations);

	WRITE_NODE_END("PROVINFO");
}

static void
_outSelectionInfo(StringInfo str, SelectionInfo *node, int depth)
{
	WRITE_NODE_START("SELECTIONINFO");

	WRITE_NODE_ELEM(expr);
	WRITE_NODE_ELEM(eqExpr);
	WRITE_NODE_ELEM(vars);
	WRITE_BOOL_ELEM(notMovable);
	WRITE_BOOL_ELEM(derived);
	WRITE_INT_ELEM(rtOrigin);

	WRITE_NODE_END("SELECTIONINFO");
}

static void
_outEquivalenceList(StringInfo str, EquivalenceList *node, int depth)
{
	WRITE_NODE_START("EQUIVALENCELIST");

	WRITE_NODE_ELEM(exprs);
	WRITE_NODE_ELEM(constant);
	//TODO

	WRITE_NODE_END("EQUIVALENCELIST");
}

static void
_outPushdownInfo(StringInfo str, PushdownInfo *node, int depth)
{
	WRITE_NODE_START("PUSHDOWNINFO");

	WRITE_NODE_ELEM(conjuncts);
	WRITE_NODE_ELEM(equiLists);
	WRITE_BOOL_ELEM(contradiction);

	WRITE_NODE_END("PUSHDOWNINFO");
}

static void
_outInequalityGraph(StringInfo str, InequalityGraph *node, int depth)
{
	WRITE_NODE_START("INEQUALITYGRAPH");

	WRITE_NODE_ELEM(equiLists);
	WRITE_NODE_ELEM(nodes);

	WRITE_NODE_END("INEQUALITYGRAPH");
}

static void
_outInequalityGraphNode(StringInfo str, InequalityGraphNode *node, int depth)
{
	WRITE_NODE_START("INEQUALITYGRAPHNODE");

	WRITE_NODE_ELEM(equis);
	WRITE_NODE_ELEM(lessThen);
	WRITE_NODE_ELEM(lessEqThen);
	WRITE_NODE_ELEM(greaterThen);
	WRITE_NODE_ELEM(greaterEqThen);
	WRITE_NODE_ELEM(consts);

	WRITE_NODE_END("INEQUALITYGRAPHNODE");
}

static void
_outQueryPushdownInfo(StringInfo str, QueryPushdownInfo *node, int depth)
{
	WRITE_NODE_START("QUERYPUSHDOWNINFO");

	//WRITE_NODE_ELEM(query);
	//WRITE_NODE_ELEM(scopes);
	WRITE_NODE_ELEM(topScope);
	WRITE_NODE_ELEM(qualPointers);
	//WRITE_NODE_ELEM(parent);
	WRITE_NODE_ELEM(children);
	WRITE_NODE_ELEM(validityScopes);

	WRITE_NODE_END("QUERYPUSHDOWNINFO");
}

static void
_outSelScope(StringInfo str, SelScope *node, int depth)
{
	WRITE_NODE_START("SELSCOPE");

	WRITE_BITMAPSET_ELEM(baseRTEs);
	WRITE_BITMAPSET_ELEM(joinRTEs);
	WRITE_NODE_ELEM(equiLists);
	WRITE_NODE_ELEM(selInfos);
	//WRITE_NODE_ELEM(parent);
	WRITE_BITMAPSET_ELEM(childRTEs);
	WRITE_NODE_ELEM(children);
	WRITE_BOOL_ELEM(contradiction);
	WRITE_ENUM_ELEM(scopeType,SelScopeType);
	//WRITE_NODE_ELEM(pushdown);
	WRITE_INT_ELEM(topIndex);
	//WRITE_VARLENGTHINT_ELEM(baseRelMap, bms_num_members(node->baseRTEs));

	WRITE_NODE_END("SELSCOPE");
}


static void
_outCopyMap (StringInfo str, CopyMap *node, int depth)
{
	WRITE_NODE_START("COPYMAP");

	WRITE_NODE_ELEM(entries);
	WRITE_INT_ELEM(rtindex);

	WRITE_NODE_END("COPYMAP");
}

//static void
//_outCopyProvInfo(StringInfo str, CopyProvInfo *node, int depth)
//{
//	WRITE_NODE_START("COPYPROVINFO");
//
//	WRITE_NODE_ELEM(inMap);
//	WRITE_NODE_ELEM(outMap);
//
//	WRITE_NODE_END("COPYPROVINFO");
//}



static void
_outCopyMapRelEntry(StringInfo str, CopyMapRelEntry *node, int depth)
{
	WRITE_NODE_START("COPYMAPRELENTRY");

	WRITE_OID_ELEM(relation);
	WRITE_INT_ELEM(refNum);
	WRITE_NODE_ELEM(attrEntries);
	WRITE_INT_ELEM(rtindex);
	WRITE_BOOL_ELEM(isStatic);
	WRITE_BOOL_ELEM(noRewrite);
	WRITE_NODE_ELEM(child);

	WRITE_NODE_END("COPYMAPRELENTRY");
}

static void
_outCopyMapEntry(StringInfo str, CopyMapEntry *node, int depth)
{
	WRITE_NODE_START("COPYMAPENTRY");

	WRITE_NODE_ELEM(baseRelAttr);
	WRITE_STRING_ELEM(provAttrName);
	WRITE_NODE_ELEM(outAttrIncls);
	WRITE_BOOL_ELEM(isStaticTrue);
	WRITE_BOOL_ELEM(isStaticFalse);

	WRITE_NODE_END("COPYMAPENTRY");
}

static void
_outAttrInclusions(StringInfo str, AttrInclusions *node, int depth)
{
	WRITE_NODE_START("ATTRINCLUSIONS");

	WRITE_NODE_ELEM(attr);
	WRITE_NODE_ELEM(inclConds);
	WRITE_BOOL_ELEM(isStatic);

	WRITE_NODE_END("ATTRINCLUSIONS");
}

static void
_outInclusionCond(StringInfo str, InclusionCond *node, int depth)
{
	WRITE_NODE_START("INCLUSIONCOND");

	WRITE_NODE_ELEM(existsAttr);
	WRITE_NODE_ELEM(eqVars);
	WRITE_NODE_ELEM(cond);

	WRITE_NODE_END("INCLUSIONCOND");
}

static void
_outVarLenghtIntArray (StringInfo str, int *array, int numMembers) //CHECK remove
{
	int i;

	if(array == NULL || numMembers == 0)
	{
		appendStringInfo(str, "[]");
		return;
	}

	appendStringInfo(str, "[");
	for(i = 0; i < numMembers - 1; i++)
	{
		appendStringInfo(str, "%i ,", array[i]);
	}
	appendStringInfo (str, "%i]", array[numMembers - 1]);
}


/*
 * _outNode -
 *	  converts a Node into ascii string and append it to 'str'
 */
static void
_outNodeXml(StringInfo str, void *obj, int depth)
{
	if (obj == NULL)
		appendStringInfoString(str, "");
	else if (IsA(obj, List) ||IsA(obj, IntList) || IsA(obj, OidList))
		_outList(str, obj, depth);
	else if (IsA(obj, Integer) ||
			 IsA(obj, Float) ||
			 IsA(obj, String) ||
			 IsA(obj, BitString))
	{
		/* nodeRead does not want to see { } around these! */
		_outValue(str, obj, depth);
	}
	else
	{
		switch (nodeTag(obj))
		{
			case T_PlannedStmt:
				_outPlannedStmt(str, obj, depth);
				break;
			case T_Plan:
				_outPlan(str, obj, depth);
				break;
			case T_Result:
				_outResult(str, obj, depth);
				break;
			case T_Append:
				_outAppend(str, obj, depth);
				break;
			case T_BitmapAnd:
				_outBitmapAnd(str, obj, depth);
				break;
			case T_BitmapOr:
				_outBitmapOr(str, obj, depth);
				break;
			case T_Scan:
				_outScan(str, obj, depth);
				break;
			case T_SeqScan:
				_outSeqScan(str, obj, depth);
				break;
			case T_IndexScan:
				_outIndexScan(str, obj, depth);
				break;
			case T_BitmapIndexScan:
				_outBitmapIndexScan(str, obj, depth);
				break;
			case T_BitmapHeapScan:
				_outBitmapHeapScan(str, obj, depth);
				break;
			case T_TidScan:
				_outTidScan(str, obj, depth);
				break;
			case T_SubqueryScan:
				_outSubqueryScan(str, obj, depth);
				break;
			case T_FunctionScan:
				_outFunctionScan(str, obj, depth);
				break;
			case T_ValuesScan:
				_outValuesScan(str, obj, depth);
				break;
			case T_Join:
				_outJoin(str, obj, depth);
				break;
			case T_NestLoop:
				_outNestLoop(str, obj, depth);
				break;
			case T_MergeJoin:
				_outMergeJoin(str, obj, depth);
				break;
			case T_HashJoin:
				_outHashJoin(str, obj, depth);
				break;
			case T_Agg:
				_outAgg(str, obj, depth);
				break;
			case T_Group:
				_outGroup(str, obj, depth);
				break;
			case T_Material:
				_outMaterial(str, obj, depth);
				break;
			case T_Sort:
				_outSort(str, obj, depth);
				break;
			case T_Unique:
				_outUnique(str, obj, depth);
				break;
			case T_SetOp:
				_outSetOp(str, obj, depth);
				break;
			case T_Limit:
				_outLimit(str, obj, depth);
				break;
			case T_Hash:
				_outHash(str, obj, depth);
				break;
			case T_Alias:
				_outAlias(str, obj, depth);
				break;
			case T_RangeVar:
				_outRangeVar(str, obj, depth);
				break;
			case T_IntoClause:
				_outIntoClause(str, obj, depth);
				break;
			case T_Var:
				_outVar(str, obj, depth);
				break;
			case T_Const:
				_outConst(str, obj, depth);
				break;
			case T_Param:
				_outParam(str, obj, depth);
				break;
			case T_Aggref:
				_outAggref(str, obj, depth);
				break;
			case T_ArrayRef:
				_outArrayRef(str, obj, depth);
				break;
			case T_FuncExpr:
				_outFuncExpr(str, obj, depth);
				break;
			case T_OpExpr:
				_outOpExpr(str, obj, depth);
				break;
			case T_DistinctExpr:
				_outDistinctExpr(str, obj, depth);
				break;
			case T_ScalarArrayOpExpr:
				_outScalarArrayOpExpr(str, obj, depth);
				break;
			case T_BoolExpr:
				_outBoolExpr(str, obj, depth);
				break;
			case T_SubLink:
				_outSubLink(str, obj, depth);
				break;
			case T_SubPlan:
				_outSubPlan(str, obj, depth);
				break;
			case T_FieldSelect:
				_outFieldSelect(str, obj, depth);
				break;
			case T_FieldStore:
				_outFieldStore(str, obj, depth);
				break;
			case T_RelabelType:
				_outRelabelType(str, obj, depth);
				break;
			case T_CoerceViaIO:
				_outCoerceViaIO(str, obj, depth);
				break;
			case T_ArrayCoerceExpr:
				_outArrayCoerceExpr(str, obj, depth);
				break;
			case T_ConvertRowtypeExpr:
				_outConvertRowtypeExpr(str, obj, depth);
				break;
			case T_CaseExpr:
				_outCaseExpr(str, obj, depth);
				break;
			case T_CaseWhen:
				_outCaseWhen(str, obj, depth);
				break;
			case T_CaseTestExpr:
				_outCaseTestExpr(str, obj, depth);
				break;
			case T_ArrayExpr:
				_outArrayExpr(str, obj, depth);
				break;
			case T_RowExpr:
				_outRowExpr(str, obj, depth);
				break;
			case T_RowCompareExpr:
				_outRowCompareExpr(str, obj, depth);
				break;
			case T_CoalesceExpr:
				_outCoalesceExpr(str, obj, depth);
				break;
			case T_MinMaxExpr:
				_outMinMaxExpr(str, obj, depth);
				break;
			case T_XmlExpr:
				_outXmlExpr(str, obj, depth);
				break;
			case T_NullIfExpr:
				_outNullIfExpr(str, obj, depth);
				break;
			case T_NullTest:
				_outNullTest(str, obj, depth);
				break;
			case T_BooleanTest:
				_outBooleanTest(str, obj, depth);
				break;
			case T_CoerceToDomain:
				_outCoerceToDomain(str, obj, depth);
				break;
			case T_CoerceToDomainValue:
				_outCoerceToDomainValue(str, obj, depth);
				break;
			case T_SetToDefault:
				_outSetToDefault(str, obj, depth);
				break;
			case T_CurrentOfExpr:
				_outCurrentOfExpr(str, obj, depth);
				break;
			case T_TargetEntry:
				_outTargetEntry(str, obj, depth);
				break;
			case T_RangeTblRef:
				_outRangeTblRef(str, obj, depth);
				break;
			case T_JoinExpr:
				_outJoinExpr(str, obj, depth);
				break;
			case T_FromExpr:
				_outFromExpr(str, obj, depth);
				break;

			case T_Path:
				_outPath(str, obj, depth);
				break;
			case T_IndexPath:
				_outIndexPath(str, obj, depth);
				break;
			case T_BitmapHeapPath:
				_outBitmapHeapPath(str, obj, depth);
				break;
			case T_BitmapAndPath:
				_outBitmapAndPath(str, obj, depth);
				break;
			case T_BitmapOrPath:
				_outBitmapOrPath(str, obj, depth);
				break;
			case T_TidPath:
				_outTidPath(str, obj, depth);
				break;
			case T_AppendPath:
				_outAppendPath(str, obj, depth);
				break;
			case T_ResultPath:
				_outResultPath(str, obj, depth);
				break;
			case T_MaterialPath:
				_outMaterialPath(str, obj, depth);
				break;
			case T_UniquePath:
				_outUniquePath(str, obj, depth);
				break;
			case T_NestPath:
				_outNestPath(str, obj, depth);
				break;
			case T_MergePath:
				_outMergePath(str, obj, depth);
				break;
			case T_HashPath:
				_outHashPath(str, obj, depth);
				break;
			case T_PlannerGlobal:
				_outPlannerGlobal(str, obj, depth);
				break;
			case T_PlannerInfo:
				_outPlannerInfo(str, obj, depth);
				break;
			case T_RelOptInfo:
				_outRelOptInfo(str, obj, depth);
				break;
			case T_IndexOptInfo:
				_outIndexOptInfo(str, obj, depth);
				break;
			case T_EquivalenceClass:
				_outEquivalenceClass(str, obj, depth);
				break;
			case T_EquivalenceMember:
				_outEquivalenceMember(str, obj, depth);
				break;
			case T_PathKey:
				_outPathKey(str, obj, depth);
				break;
			case T_RestrictInfo:
				_outRestrictInfo(str, obj, depth);
				break;
			case T_InnerIndexscanInfo:
				_outInnerIndexscanInfo(str, obj, depth);
				break;
			case T_OuterJoinInfo:
				_outOuterJoinInfo(str, obj, depth);
				break;
			case T_InClauseInfo:
				_outInClauseInfo(str, obj, depth);
				break;
			case T_AppendRelInfo:
				_outAppendRelInfo(str, obj, depth);
				break;
			case T_PlannerParamItem:
				_outPlannerParamItem(str, obj, depth);
				break;

			case T_CreateStmt:
				_outCreateStmt(str, obj, depth);
				break;
			case T_IndexStmt:
				_outIndexStmt(str, obj, depth);
				break;
			case T_NotifyStmt:
				_outNotifyStmt(str, obj, depth);
				break;
			case T_DeclareCursorStmt:
				_outDeclareCursorStmt(str, obj, depth);
				break;
			case T_SelectStmt:
				_outSelectStmt(str, obj, depth);
				break;
			case T_ColumnDef:
				_outColumnDef(str, obj, depth);
				break;
			case T_TypeName:
				_outTypeName(str, obj, depth);
				break;
			case T_TypeCast:
				_outTypeCast(str, obj, depth);
				break;
			case T_IndexElem:
				_outIndexElem(str, obj, depth);
				break;
			case T_Query:
				_outQuery(str, obj, depth);
				break;
			case T_SortClause:
				_outSortClause(str, obj, depth);
				break;
			case T_GroupClause:
				_outGroupClause(str, obj, depth);
				break;
			case T_RowMarkClause:
				_outRowMarkClause(str, obj, depth);
				break;
			case T_SetOperationStmt:
				_outSetOperationStmt(str, obj, depth);
				break;
			case T_RangeTblEntry:
				_outRangeTblEntry(str, obj, depth);
				break;
			case T_A_Expr:
				_outAExpr(str, obj, depth);
				break;
			case T_ColumnRef:
				_outColumnRef(str, obj, depth);
				break;
			case T_ParamRef:
				_outParamRef(str, obj, depth);
				break;
			case T_A_Const:
				_outAConst(str, obj, depth);
				break;
			case T_A_Indices:
				_outA_Indices(str, obj, depth);
				break;
			case T_A_Indirection:
				_outA_Indirection(str, obj, depth);
				break;
			case T_ResTarget:
				_outResTarget(str, obj, depth);
				break;
			case T_Constraint:
				_outConstraint(str, obj, depth);
				break;
			case T_FkConstraint:
				_outFkConstraint(str, obj, depth);
				break;
			case T_FuncCall:
				_outFuncCall(str, obj, depth);
				break;
			case T_DefElem:
				_outDefElem(str, obj, depth);
				break;
			case T_LockingClause:
				_outLockingClause(str, obj, depth);
				break;
			case T_XmlSerialize:
				_outXmlSerialize(str, obj, depth);
				break;
			case T_SublinkInfo:
				_outSublinkInfo(str, obj, depth);
				break;
			case T_CorrVarInfo:
				_outCorrVarInfo(str, obj, depth);
				break;
			case T_ProvInfo:
				_outProvInfo(str, obj, depth);
				break;
			case T_SelectionInfo:
				_outSelectionInfo(str, obj, depth);
				break;
			case T_EquivalenceList:
				_outEquivalenceList(str, obj, depth);
				break;
			case T_PushdownInfo:
				_outPushdownInfo(str, obj, depth);
				break;
			case T_InequalityGraph:
				_outInequalityGraph(str, obj, depth);
				break;
			case T_InequalityGraphNode:
				_outInequalityGraphNode(str, obj, depth);
				break;
			case T_QueryPushdownInfo:
				_outQueryPushdownInfo(str, obj, depth);
				break;
			case T_SelScope:
				_outSelScope(str, obj, depth);
				break;
			case T_CopyMap:
				_outCopyMap(str, obj, depth);
				break;
//			case T_CopyProvInfo:
//				_outCopyProvInfo(str, obj, depth);
//				break;
			case T_CopyMapRelEntry:
				_outCopyMapRelEntry(str, obj, depth);
				break;
			case T_CopyMapEntry:
				_outCopyMapEntry(str, obj, depth);
				break;
			case T_AttrInclusions:
				_outAttrInclusions(str, obj, depth);
				break;
			case T_InclusionCond:
				_outInclusionCond(str, obj, depth);
				break;
			default:

				/*
				 * This should be an ERROR, but it's too useful to be able to
				 * dump structures that _outNode only understands part of.
				 */
				elog(WARNING, "could not dump unrecognized node type: %d",
					 (int) nodeTag(obj));
				break;
		}
	}
}

/*
 *
 */

static void
_outXmlHeader (StringInfo str)
{
	appendStringInfo(str, "<?xml version=\"1.0\" ?>\n");
}

/*
 * nodeToString -
 *	   returns the ascii representation of the Node as a palloc'd string
 */
char *
nodeToXml(void *obj)
{
	StringInfoData str;

	initStringInfo(&str);
	_outXmlHeader(&str);
	_outNodeXml(&str, obj, 0);

	return str.data;
}


/*
 *
 */
StringInfo
nodeToXmlStringInfo(void *obj)
{
	StringInfo str;

	str = makeStringInfo ();

	_outXmlHeader(str);
	_outNodeXml(str, obj, 0);

	return str;
}

