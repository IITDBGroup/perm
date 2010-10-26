/*-------------------------------------------------------------------------
 *
 * prov_trans_parse_back_xml.c
 *	  POSTGRES C
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/transformationp/prov_trans_parse_back_xml.c,v 1.542 21.09.2009 10:41:42 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "c.h"

#include "utils/varbit.h"
#include "utils/builtins.h"
#include "nodes/value.h"
#include "optimizer/clauses.h"
#include "utils/lsyscache.h"
#include "parser/parsetree.h"
#include "parser/parse_expr.h"
#include "rewrite/rewriteHandler.h"
#include "catalog/pg_operator.h"
#include "utils/syscache.h"
#include "optimizer/tlist.h"
#include "utils/guc.h"

#include "provrewrite/provstack.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_trans_util.h"
#include "provrewrite/prov_parse_back_util.h"
#include "provrewrite/prov_trans_parse_back.h"
#include "provrewrite/prov_trans_parse_back_xml.h"

/*
 * Macros to simplify output of different kinds of fields.	Use these
 * wherever possible to reduce the chance for silly typos.	Note that these
 * hard-wire conventions about the names of the local variables in an Out
 * routine.
 */

/* Header for all parse back methods */
#define OUTFUNC_HEADER \
	StringInfo str; \
	TransSubInfo *newSub; \
	TransParseRange *range; \
	bool isRoot; \
	int indendHelper; \
	str = context->buf; \
	newSub = NULL; \
	isRoot = (!curSub || !info) ? false : \
			(equal(info->root, curSub) ? true : false);

/* parameters for all parse back methods */
#define OUTPARAMS \
	int depth, ParseXMLContext *context, bool inStatic, bool inDummy, \
	TransProvInfo *info, TransSubInfo *curSub

/* Write a line break */
#define WRITE_BR \
	do { \
		if (context->whitespace) \
			(appendStringInfoChar(str, '\n')); \
	} while (0)

/* Write depth times tabs */
#define WRITE_INDENT(depth) \
	do { \
		if (context->whitespace) \
		{ \
			indendHelper = depth; \
			while ((indendHelper)-- > 0) { \
				appendStringInfoChar(str, ' '); \
			} \
		} \
	} while (0)

/* Write an bufing as an element */
#define WRITE_ELEMENT(name,value) \
	do { \
		WRITE_INDENT(depth); \
		appendStringInfo(str, "<" CppAsString(name) ">%s</" \
				CppAsString(name) ">", (value)); \
		WRITE_BR; \
	} while (0)

/* Write an empty element */
#define WRITE_EMPTY(name) \
	do { \
		WRITE_INDENT(depth - 1); \
		appendStringInfo(str, "<%s/>", (name)); \
	} while (0)

/* Write the element start for a node */
#define WRITE_START(nodelabel) \
	do { \
		WRITE_INDENT(depth - 1); \
		appendStringInfo(str, "<%s>", (nodelabel)); \
		WRITE_BR; \
		depth++; \
	} while (0)

/* Write an element name and its opening '<' */
#define WRITE_OPEN(nodelabel) \
	do { \
		WRITE_INDENT(depth - 1); \
		appendStringInfo(str, "<%s", (nodelabel)); \
	} while(0)

/* Write an element name and its opening '<' without indentation */
#define WRITE_OPEN_ONE(nodelabel) \
	do { \
		appendStringInfo(str, "<%s", (nodelabel)); \
	} while(0)


/* write an attribute and its value */
#define WRITE_ATTR(name,value) \
	do { \
		appendStringInfo(str, " %s=\"%s\"", name, value); \
	} while (0)

#define WRITE_CLOSE() \
	do { \
		appendStringInfoString(str, ">"); \
		WRITE_BR; \
		depth++; \
	} while(0)

#define WRITE_CLOSE_NOBR() \
	do { \
		appendStringInfoString(str, ">"); \
	} while(0)

/* Write the element end for a node */
#define WRITE_END(nodelabel) \
	do { \
		depth--; \
		WRITE_INDENT(depth - 1); \
		appendStringInfo(str, "</%s>", (nodelabel)); \
		WRITE_BR; \
	} while (0)

/* Write the element end without linebreak but reduce depth */
#define WRITE_END_NOBR(nodelabel) \
	do { \
		depth--; \
		WRITE_INDENT(depth - 1); \
		appendStringInfo(str, "</%s>", (nodelabel)); \
	} while (0)

/* open an element without line break */
#define WRITE_START_ONE(name) \
	do { \
		appendStringInfo(str, "<%s>", (name)); \
	} while (0)

/* open an element without line break */
#define WRITE_END_ONE(name) \
	do { \
		appendStringInfo(str, "</%s>", (name)); \
	} while (0)

/* write content with the current indend */
#define WRITE_CONTENT(value) \
	do { \
		WRITE_INDENT(depth); \
		appendStringInfo(str, "%s", (value)) \
		WRITE_BR; \
	} while (0)

/* write an element for an OpExpr as either the verbose name we have defined
 * for it or as a function call if we do not have a verbose name for it
 */
#define WRITE_OP_OPEN \
	do \
	{ \
		if (verbName) \
			WRITE_START_ONE(verbName); \
		else \
		{ \
			WRITE_OPEN_ONE("FunctionCall"); \
			WRITE_ATTR("name",opName); \
			WRITE_CLOSE_NOBR(); \
		} \
	} while (0)

/* close an element for an OpExpr */
#define WRITE_OP_CLOSE \
	do \
	{ \
		if (verbName) \
			WRITE_END_ONE(verbName); \
		else \
			WRITE_END_ONE("FunctionCall"); \
	} while (0)

/* types */
typedef enum XmlRepType
{
	QueryTreeXML, SimpleXML
} XmlRepType;

typedef struct ParseXMLContext
{
	StringInfo buf; /* output buffer to append to */
	List *namespaces; /* List of deparse_namespace nodes */
	bool varprefix;
	bool whitespace;
	bool transProv;
	List **ranges;
	List **infoStack;
	XmlRepType repType;
} ParseXMLContext;

#define MAKE_PARSE_CONTEXT() \
	((ParseXMLContext *) palloc(sizeof(ParseXMLContext)))

/* functions */
static void parseBackXMLQueryNode(Query *query, char* alias, OUTPARAMS);
static void parseBackSetOp(Node *setOp, Query *query, OUTPARAMS);
static int openCloseAnnotationsQNode (Query *query, ParseXMLContext *context,
		bool open, int depth);
static int openCloseAnnotations(List *annotation, ParseXMLContext *context,
		bool open, int depth);
static void parseBackSelect(Query *query, TransProjType projType, OUTPARAMS);
static void parseBackHaving(Query *query, OUTPARAMS);
static void parseBackAgg(Query *query, OUTPARAMS);
static void parseBackGroupBy(Query *query, OUTPARAMS);
static void parseBackFrom(Query *query, OUTPARAMS);
static void parseBackFromItem(Node *fromItem, Query *query, OUTPARAMS);
static void parseBackWhere(Query *query, OUTPARAMS);

static void parseBackExpr(Node *node, ParseXMLContext *context,
		TransProjType proType, bool showImplicit, int depth);
static void parseBackConstExpr(Const *constval, ParseXMLContext *context,
		int depth);
static void parseBackAggExpr(Aggref *agg, ParseXMLContext *context,
		TransProjType projType, int depth);
static void parseBackFuncExpr(FuncExpr *func, ParseXMLContext *context,
		TransProjType projType, bool showimplicit, int depth);
static void parseBackOpExpr(OpExpr *op, ParseXMLContext *context,
		TransProjType projType, int depth);
static void parseBackSublink(SubLink *sublink, ParseXMLContext *context,
		TransProjType projType, int depth);
static void parseBackCoercionExpr(Node *arg, ParseXMLContext *context,
				Oid resulttype, int32 resulttypmod, Node *parentNode,
				TransProjType projType, int depth);
static char *parseBackVar(Var *var, int levelsup, bool showstar,
		ParseXMLContext *context);
static Node *parseBackSortgroupclause(SortClause *srt, List *tlist,
		bool force_colno, ParseXMLContext *context, TransProjType projType,
		int depth);
static char *getVerboseOpName(char *opName);

/*
 * Generate an XML representation for a transformation provenance query tree
 * and the needed auxiliary bufucture to be able to transform a T-provenance
 * bitset representation into XML with annotations and exclusions of
 * non-contribution parts of the query.
 */

StringInfo
parseBackTransToXML(Query *query, TransRepQueryInfo *newInfo, bool simple,
		MemoryContext funcPrivateCtx)
{
	deparse_namespace namespc;
	ParseXMLContext *context;
	List *ranges;
	List *dummyInfo;
	StringInfo str;

	ranges = NIL;
	dummyInfo = NIL;

	/* replace ambigous unnamed column names */
	replaceUnnamedColumnsWalker((Node *) query, NULL);
	correctRecurSubQueryAlias(query);

	/* init StringInfo */
	str = makeStringInfo();

	context = MAKE_PARSE_CONTEXT();
	context->buf = str;
	context->namespaces = list_make1(&namespc);
	context->ranges = &ranges;
	context->infoStack = &dummyInfo;
	context->varprefix = true;
	context->whitespace = prov_xml_whitespace;
	context->repType = (simple ? SimpleXML : QueryTreeXML);
	context->transProv = (newInfo != NULL);

	namespc.rtable = query->rtable;
	namespc.outer_plan = namespc.inner_plan = NULL;

	if (simple)
		parseBackXMLQueryNode(query, NULL, 0, context, false, false, NULL, NULL);
	else
		parseBackXMLQueryNode(query, NULL, 0, context, false, false, NULL, NULL); //TODO

	if (context->transProv)
		postprocessRanges(ranges, str, newInfo, funcPrivateCtx);

	return str;
}

/*
 *
 */

static void
parseBackXMLQueryNode(Query *query, char* alias, OUTPARAMS)
{
	TransProvInfo *newInfo;
	bool newInDummy;
	ParseXMLContext newContext;
	deparse_namespace dpns;
	OUTFUNC_HEADER;

	if (context->transProv)
	{
		newInfo = GET_TRANS_INFO(query);
		newInDummy = !(equal(newInfo->root, getRootSubForNode(newInfo)));
	}

	/*
	 * Before we begin to examine the query, acquire locks on referenced
	 * relations, and fix up deleted columns in JOIN RTEs.	This ensures
	 * consistent results.	Note we assume it's OK to scribble on the passed
	 * querytree!
	 */
	AcquireRewriteLocks(query);

	newContext.buf = str;
	newContext.namespaces = lcons(&dpns, list_copy(context->namespaces));
	newContext.ranges = context->ranges;
	newContext.infoStack = context->infoStack;
	newContext.varprefix = (context->namespaces != NIL || list_length(
			query->rtable) != 1);
	newContext.repType = context->repType;
	newContext.transProv = context->transProv;
	newContext.whitespace = context->whitespace;

	dpns.rtable = query->rtable;
	dpns.outer_plan = dpns.inner_plan = NULL;

	/* process query node */
	if (context->transProv && !inStatic)
	{
		newSub = getRootSubForNode(newInfo);
		if (!inDummy)
			MAKE_RANGE(range, newSub);
	}
	else
		newSub = curSub;

	/* handle set operation query node */
	if (query->setOperations)
	{
		parseBackSetOp(query->setOperations, query, depth, &newContext,
				newInfo->isStatic, false, newInfo,
				(TransSubInfo *) newInfo->root);
		return;
	}

	/* is a normal query node */
	depth = openCloseAnnotationsQNode(query, &newContext, true, depth);

	// write alias if any is used
	if (alias)
	{
		WRITE_OPEN("Query");
		WRITE_ATTR("alias", alias);
		WRITE_CLOSE();
	}
	else
		WRITE_START("Query");

	// parse back SELECT
	if (query->hasAggs)
		parseBackAgg(query, depth, &newContext, newInfo->isStatic, newInDummy,
				newInfo, newSub);
	else
		parseBackSelect(query, None, depth, &newContext, newInfo->isStatic,
				newInDummy, newInfo, newSub);

	// parse back FROM
	parseBackFrom(query, depth, &newContext, newInfo->isStatic, newInDummy,
			newInfo, (newContext.transProv ? NULL : NULL)); //CHECK that false is ok for topJoinNode

	// parse back WHERE if present
	if (query->jointree->quals)
		parseBackWhere(
				query,
				depth,
				&newContext,
				newInfo->isStatic,
				newInDummy,
				newInfo,
				(newContext.transProv ? getSpecificInfo(
						(TransSubInfo *) newInfo->root, SUBOP_Selection) : NULL));

	// parse back GROUP BY and HAVING if present
	if (query->groupClause)
		parseBackGroupBy(query, depth, &newContext, newInfo->isStatic,
				newInDummy, newInfo, (newContext.transProv ? getSpecificInfo(
						(TransSubInfo *) newInfo->root, SUBOP_Aggregation)
						: NULL));

	if (query->havingQual)
		parseBackHaving(query, depth, &newContext, newInfo->isStatic,
				newInDummy, newInfo, (newContext.transProv ? getSpecificInfo(
						(TransSubInfo *) newInfo->root, SUBOP_Having) : NULL));

	//TODO order by clause

	WRITE_END("Query");
	openCloseAnnotationsQNode(query, &newContext, false, depth);

	if (newContext.transProv && !inStatic && !inDummy)
		range->end = str->len;
}

/*
 *
 */

static void
parseBackSetOp(Node *setOp, Query *query, OUTPARAMS)
{
	char *setOpName;
	OUTFUNC_HEADER;

	if (IsA(setOp, SetOperationStmt))
	{
		SetOperationStmt *setOper;

		if (context->transProv && !inStatic)
			MAKE_RANGE(range, curSub);

		setOper = (SetOperationStmt *) setOp;

		switch (setOper->op)
		{
		case SETOP_UNION:
			if (setOper->all)
				setOpName = "UnionAll";
			else
				setOpName = "Union";
			break;
		case SETOP_EXCEPT:
			if (setOper->all)
				setOpName = "SetDifferenceAll";
			else
				setOpName = "SetDifference";
			break;
		case SETOP_INTERSECT:
			if (setOper->all)
				setOpName = "IntersectionAll";
			else
				setOpName = "Intersection";
			break;
		default:
			break;
		}

		WRITE_START(setOpName);

		parseBackSetOp(setOper->larg, query, depth, context, inStatic
				|| curSub->isStatic, false, info,
				(context->transProv ? TSET_LARG(curSub) : NULL));
		// for set difference the right node is never in the provenance.
		if (setOper->op == SETOP_EXCEPT && context->transProv)
			WRITE_START("NOT");
		parseBackSetOp(setOper->rarg, query, depth, context, inStatic
				|| curSub->isStatic, false, info,
				(context->transProv ? TSET_RARG(curSub) : NULL));
		if (setOper->op == SETOP_EXCEPT && context->transProv)
			WRITE_END("NOT");

		WRITE_END(setOpName);

		if (context->transProv && !inStatic)
			range->end = str->len;
	}
	else
	{
		RangeTblEntry *rte;
		char *aliasName = NULL;

		rte = rt_fetch(((RangeTblRef *) setOp)->rtindex, query->rtable);

		if (rte->alias)
			aliasName = rte->alias->aliasname;

		parseBackXMLQueryNode(rte->subquery, aliasName, depth, context, inStatic, inDummy,
				NULL, curSub);
	}
}

/*
 *
 */

static int
openCloseAnnotationsQNode (Query *query, ParseXMLContext *context, bool open,
		int depth)
{
	if (!query->provInfo)
		return depth;

	return openCloseAnnotations ((List *) Provinfo(query)->annotations,
			context, open, depth);
}

static int
openCloseAnnotations (List *annotations, ParseXMLContext *context, bool open,
		int depth)
{
	ListCell *lc;
	Value *val;
	StringInfo str;
	int indendHelper;

	str = context->buf;

	foreach(lc, annotations)
	{
		val = (Value *) lfirst(lc);

		if (open)
		{
			WRITE_OPEN("Annotation");
			WRITE_ATTR("value",strVal(val));
			WRITE_CLOSE();
		}
		else
			WRITE_END("Annotation");
	}

	return depth;
}


static void
parseBackHaving(Query *query, OUTPARAMS)
{
	OUTFUNC_HEADER;

	if (context->transProv && !inStatic && !isRoot)
		MAKE_RANGE(range, curSub);

	WRITE_START("Having");

	parseBackExpr(query->havingQual, context, None, false, depth);

	WRITE_END("Having");

	if (context->transProv && !inStatic && !isRoot)
		range->end = str->len;
}

/*
 *
 */

static void
parseBackSelect(Query *query, TransProjType projType, OUTPARAMS)
{
	ListCell *lc;
	TargetEntry *te;
	OUTFUNC_HEADER;

	if (context->transProv && !inStatic && !isRoot)
		MAKE_RANGE(range, curSub);

	WRITE_START("Select");
	WRITE_INDENT(depth);

	/* Add the DISTINCT clause if given */
	if (query->distinctClause != NIL)
	{
		if (has_distinct_on_clause(query))
		{
			WRITE_START_ONE("Distinct");
			foreach(lc, query->distinctClause)
			{
				SortClause *srt = (SortClause *) lfirst(lc);

				parseBackSortgroupclause(srt, query->targetList, false,
						context, projType, depth);
			}
			WRITE_END_ONE("Distinct");
		}
		else
			WRITE_EMPTY("Distinct");
	}

	/* handle each target list entry */
	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);

		WRITE_OPEN("Attr");
		WRITE_ATTR("name", te->resname);
		WRITE_CLOSE_NOBR();

		parseBackExpr((Node *) te->expr, context, None, false, depth);

		WRITE_END_ONE("Attr");
	}

	WRITE_BR;
	WRITE_END("Select");

	if (context->transProv && !inStatic && !isRoot)
		range->end = str->len;
}

/*
 *
 */

static void
parseBackAgg(Query *query, OUTPARAMS)
{
	TransProjType type;
	bool underHaving = false;
	OUTFUNC_HEADER;

	/* get agg type */
	if (context->transProv)
	{
		type = getProjectionType(query, &underHaving);
		newSub = (TransSubInfo *) info->root;

		if (underHaving)
			newSub = TSET_LARG(newSub);
	}
	else
	{
		type = None;
		newSub = NULL;
	}

	switch (type)
	{
	case ProjUnderAgg:
	case ProjOverAgg:
		push(context->infoStack, TSET_LARG(newSub));
		/* fall through */
	case ProjBothAgg:
		push(context->infoStack, TSET_LARG(TSET_LARG(newSub)));
		break;
	default:
		break;
	}

	/* handle selection */
	parseBackSelect(query, type, depth, context, inStatic, inDummy, info,
			newSub);
}

/*
 *
 */

static void
parseBackGroupBy(Query *query, OUTPARAMS)
{
	ListCell *lc;
	OUTFUNC_HEADER;

	if (context->transProv && !inStatic && !isRoot)
		MAKE_RANGE(range, curSub);

	WRITE_START("GroupBy");

	foreach(lc, query->groupClause)
	{
		GroupClause *grp = (GroupClause *) lfirst(lc);

		parseBackSortgroupclause(grp, query->targetList, false, context, None,
				depth);
	}

	WRITE_END("GroupBy");

	if (context->transProv && !inStatic && !isRoot)
		range->end = str->len;
}

/*
 *
 */

static void
parseBackFrom(Query *query, OUTPARAMS)
{
	ListCell *lc;
	ListCell *infoLc;
	Node *fromItem;
	bool cross = false;
	OUTFUNC_HEADER;

	if (context->transProv)
	{
		if (context->transProv && !inStatic && list_length(
				query->jointree->fromlist) > 1)
		{
			curSub = getTopJoinInfo(query, true);
			cross = true;
			infoLc = list_head(curSub->children);
		}
		else
			newSub = curSub = getTopJoinInfo(query, false);

		if (!inStatic && !isRoot && !inDummy)
			MAKE_RANGE(range, curSub);
	}

	WRITE_START("From");

	foreach(lc, query->jointree->fromlist)
	{
		fromItem = lfirst(lc);

		if (cross)
		{
			newSub = (TransSubInfo *) lfirst(infoLc);
			infoLc = lnext(infoLc);
		}

		parseBackFromItem(fromItem, query, depth, context, inStatic, inDummy,
				info, newSub);
	}

	WRITE_END("From");

	if (context->transProv && !inStatic && !isRoot && !inDummy)
		range->end = str->len;
}

/*
 *
 */

static void
parseBackFromItem(Node *fromItem, Query *query, OUTPARAMS)
{
	JoinExpr *join;
	RangeTblRef *rtRef;
	char *joinName;
	OUTFUNC_HEADER;

	if (context->transProv && !inStatic && !isRoot && !inDummy
			&& !IsA(curSub,TransProvInfo))
		MAKE_RANGE(range, curSub);

	if (IsA(fromItem, JoinExpr))
	{
		join = (JoinExpr *) fromItem;

		switch (join->jointype)
		{
		case JOIN_INNER:
			joinName = "Join";
			break;
		case JOIN_LEFT:
			joinName = "LeftJoin";
			break;
		case JOIN_RIGHT:
			joinName = "RightJoin";
			break;
		case JOIN_FULL:
			joinName = "FullJoin";
			break;
		default:
			break;
		}

		WRITE_START(joinName);

		/* process join children */
		parseBackFromItem(join->larg, query, depth, context, inStatic
				|| curSub->isStatic, false, info,
				(context->transProv ? TSET_LARG(curSub) : NULL));
		parseBackFromItem(join->rarg, query, depth, context, inStatic
				|| curSub->isStatic, false, info,
				(context->transProv ? TSET_RARG(curSub) : NULL));

		/* process USING / ON */
		if (join->using)
		{
			ListCell *col;

			WRITE_INDENT(depth);
			WRITE_START_ONE("Using");
			foreach(col, join->using)
			{
				WRITE_START_ONE("Var");
				appendStringInfoString(str, strVal(lfirst(col)));
				WRITE_END_ONE("Var");
			}
			WRITE_END_ONE("Using");
			WRITE_BR;
		}
		else if (join->quals)
		{
			WRITE_INDENT(depth);
			WRITE_START_ONE("On");
			parseBackExpr(join->quals, context, None, false, depth);
			WRITE_END_ONE("On");
			WRITE_BR;
		}

		WRITE_END(joinName);
	}
	else
	{
		RangeTblEntry *rte;
		char *aliasName = NULL;

		rtRef = (RangeTblRef *) fromItem;
		rte = rt_fetch(rtRef->rtindex, query->rtable);

		if (rte->alias)
			aliasName = rte->alias->aliasname;

		if (rte->annotations)
			openCloseAnnotations(rte->annotations, context, true, depth++);

		switch (rte->rtekind)
		{
		case RTE_RELATION:
			WRITE_INDENT(depth);

			if (aliasName)
			{
				WRITE_OPEN("Relation");
				WRITE_ATTR("alias", aliasName);
				WRITE_CLOSE_NOBR();
			}
			else
				WRITE_START_ONE("Relation");

			appendStringInfo(str, "%s%s", only_marker(rte),
					generate_relation_name(rte->relid));

			WRITE_END_ONE("Relation");
			WRITE_BR;
			break;
		case RTE_SUBQUERY:
			parseBackXMLQueryNode(rte->subquery, aliasName, depth, context,
					inStatic, inDummy, NULL, NULL);
			break;
		default:
			//TODO
			break;
		}

		if (rte->annotations)
		{
			openCloseAnnotations(rte->annotations, context, false, ++depth);
			depth--;
		}
	}

	if (context->transProv && !inStatic && !isRoot && !inDummy
			&& !IsA(curSub,TransProvInfo))
		range->end = str->len;
}

/*
 *
 */

static void
parseBackWhere(Query *query, OUTPARAMS)
{
	OUTFUNC_HEADER;

	if (context->transProv && !inStatic && !isRoot)
		MAKE_RANGE(range, curSub);

	WRITE_START("Where");

	parseBackExpr(query->jointree->quals, context, None, false, depth);

	WRITE_BR;
	WRITE_END("Where");

	if (context->transProv && !inStatic && !isRoot)
		range->end = str->len;
}

/*
 *
 */

static void
parseBackExpr(Node *node, ParseXMLContext *context, TransProjType proType,
		bool showImplicit, int depth)
{
	int indendHelper;
	StringInfo str = context->buf;

	if (node == NULL)
		return;

	/*
	 * Each level of get_rule_expr must emit an indivisible term
	 * (parenthesized if necessary) to ensure result is reparsed into the same
	 * expression tree.  The only exception is that when the input is a List,
	 * we emit the component items comma-separated with no surrounding
	 * decoration; this is convenient for most callers.
	 */
	switch (nodeTag(node))
	{
	case T_Var:
		(void) parseBackVar((Var *) node, 0, true, context);
		break;

	case T_Const:
		parseBackConstExpr((Const *) node, context, depth);
		break;

	case T_Param:
		WRITE_START("Param");
		appendStringInfo(str, "$%d", ((Param *) node)->paramid);
		WRITE_END("Param");
		break;

	case T_Aggref:
		parseBackAggExpr((Aggref *) node, context, proType, depth);
		break;

	case T_ArrayRef:
		//			{
		//				ArrayRef   *aref = (ArrayRef *) node;
		//				bool		need_parens;
		//
		//				/*
		//				 * Parenthesize the argument unless it's a simple Var or a
		//				 * FieldSelect.  (In particular, if it's another ArrayRef, we
		//				 * *must* parenthesize to avoid confusion.)
		//				 */
		//				need_parens = !IsA(aref->refexpr, Var) &&
		//					!IsA(aref->refexpr, FieldSelect);
		//				if (need_parens)
		//					appendStringInfoChar(str, '(');
		//				get_rule_expr((Node *) aref->refexpr, context, showimplicit);
		//				if (need_parens)
		//					appendStringInfoChar(str, ')');
		//				printSubscripts(aref, context);
		//
		//				/*
		//				 * Array assignment nodes should have been handled in
		//				 * processIndirection().
		//				 */
		//				if (aref->refassgnexpr)
		//					elog(ERROR, "unexpected refassgnexpr");
		//			}
		break;

	case T_FuncExpr:
		parseBackFuncExpr((FuncExpr *) node, context, proType, showImplicit,
				depth);
		break;

	case T_OpExpr:
		parseBackOpExpr((OpExpr *) node, context, proType, depth);
		break;

	case T_DistinctExpr:
	{
		DistinctExpr *expr = (DistinctExpr *) node;
		List *args = expr->args;
		Node *arg1 = (Node *) linitial(args);
		Node *arg2 = (Node *) lsecond(args);

		WRITE_START_ONE("IsDistinctFrom");

		parseBackExpr(arg1, context, proType, true, depth);
		parseBackExpr(arg2, context, proType, true, depth);

		WRITE_END_ONE("IsDistinctFrom");
	}
		break;

	case T_ScalarArrayOpExpr:
	{
		ScalarArrayOpExpr *expr = (ScalarArrayOpExpr *) node;
		List *args = expr->args;
		Node *arg1 = (Node *) linitial(args);
		Node *arg2 = (Node *) lsecond(args);
		//TODO
		//				if (!PRETTY_PAREN(context))
		//					appendStringInfoChar(str, '(');
		//				get_rule_expr_paren(arg1, context, true, node);
		//				appendStringInfo(str, " %s %s (",
		//								 generate_operator_name(expr->opno,
		//														exprType(arg1),
		//										   get_element_type(exprType(arg2))),
		//								 expr->useOr ? "ANY" : "ALL");
		//				get_rule_expr_paren(arg2, context, true, node);
		//				appendStringInfoChar(str, ')');
		//				if (!PRETTY_PAREN(context))
		//					appendStringInfoChar(str, ')');
	}
		break;

	case T_BoolExpr:
	{
		BoolExpr *expr = (BoolExpr *) node;
		ListCell *arg;

		switch (expr->boolop)
		{
		case AND_EXPR:
			WRITE_START("And");

			foreach(arg, expr->args)
			{
				WRITE_INDENT(depth - 1);
				parseBackExpr((Node *) lfirst(arg), context, proType, false,
						depth);
				WRITE_BR;
			}

			WRITE_END_NOBR("And");
			break;

		case OR_EXPR:
			WRITE_START("Or");

			foreach(arg, expr->args)
			{
				WRITE_INDENT(depth - 1);
				parseBackExpr((Node *) lfirst(arg), context, proType, false,
						depth);
				WRITE_BR;
			}

			WRITE_END_NOBR("Or");
			break;

		case NOT_EXPR:
			WRITE_START("Not");
			WRITE_INDENT(depth - 1);

			parseBackExpr((Node *) lfirst(list_head(expr->args)), context,
					proType, false, depth);

			WRITE_BR;
			WRITE_END_NOBR("Not");
			break;

		default:
			elog(ERROR, "unrecognized boolop: %d",
			(int) expr->boolop);
		}
	}
	break;

	case T_SubLink:
		parseBackSublink ((SubLink *) node, context, proType, depth);
	break;

	case T_FieldSelect:
	{
		//				FieldSelect *fselect = (FieldSelect *) node;
		//				Node	   *arg = (Node *) fselect->arg;
		//				int			fno = fselect->fieldnum;
		//				const char *fieldname;
		//				bool		need_parens;
		//
		//				/*
		//				 * Parenthesize the argument unless it's an ArrayRef or
		//				 * another FieldSelect.  Note in particular that it would be
		//				 * WRONG to not parenthesize a Var argument; simplicity is not
		//				 * the issue here, having the right number of names is.
		//				 */
		//				need_parens = !IsA(arg, ArrayRef) &&!IsA(arg, FieldSelect);
		//				if (need_parens)
		//					appendStringInfoChar(str, '(');
		//				get_rule_expr(arg, context, true);
		//				if (need_parens)
		//					appendStringInfoChar(str, ')');
		//
		//				/*
		//				 * Get and print the field name.
		//				 */
		//				fieldname = get_name_for_var_field((Var *) arg, fno,
		//												   0, context);
		//				appendStringInfo(str, ".%s", quote_identifier(fieldname));
	}
	break;

	case T_FieldStore:

	/*
	 * We shouldn't see FieldStore here; it should have been stripped
	 * off by processIndirection().
	 */
	elog(ERROR, "unexpected FieldStore");
	break;

	case T_RelabelType:
	{
		RelabelType *relabel = (RelabelType *) node;
		Node *arg = (Node *) relabel->arg;

		if (relabel->relabelformat == COERCE_IMPLICIT_CAST &&
				!showImplicit)
		{
			/* don't show the implicit cast */
			parseBackExpr(arg, context, proType, false, depth);
		}
		else
		{
			parseBackCoercionExpr(arg, context,
					relabel->resulttype,
					relabel->resulttypmod,
					node, proType, depth);
		}
	}
	break;

	case T_CoerceViaIO:
	{
		CoerceViaIO *iocoerce = (CoerceViaIO *) node;
		Node *arg = (Node *) iocoerce->arg;

		if (iocoerce->coerceformat == COERCE_IMPLICIT_CAST &&
				!showImplicit)
		{
			/* don't show the implicit cast */
			parseBackExpr(arg, context, proType, false, depth);
		}
		else
		{
			parseBackCoercionExpr(arg, context,
					iocoerce->resulttype,
					-1,
					node, proType, depth);
		}
	}
	break;

	case T_ArrayCoerceExpr:
	{
		ArrayCoerceExpr *acoerce = (ArrayCoerceExpr *) node;
		Node *arg = (Node *) acoerce->arg;

		if (acoerce->coerceformat == COERCE_IMPLICIT_CAST &&
				!showImplicit)
		{
			/* don't show the implicit cast */
			parseBackExpr(arg, context, proType, false, depth);
		}
		else
		{
			parseBackCoercionExpr(arg, context,
					acoerce->resulttype,
					-1,
					node, proType, depth);
		}
	}
	break;

	case T_ConvertRowtypeExpr:
	{
		ConvertRowtypeExpr *convert = (ConvertRowtypeExpr *) node;
		Node *arg = (Node *) convert->arg;

		if (convert->convertformat == COERCE_IMPLICIT_CAST &&
				!showImplicit)
		{
			/* don't show the implicit cast */
			parseBackExpr(arg, context, proType, false, depth);
		}
		else
		{
			parseBackCoercionExpr(arg, context,
					convert->resulttype,
					-1,
					node, proType, depth);
		}
	}
	break;

	case T_CaseExpr:
	{
		//				CaseExpr   *caseexpr = (CaseExpr *) node;
		//				ListCell   *temp;
		//
		//				appendContextKeyword(context, "CASE",
		//									 0, PRETTYINDENT_VAR, 0);
		//				if (caseexpr->arg)
		//				{
		//					appendStringInfoChar(str, ' ');
		//					get_rule_expr((Node *) caseexpr->arg, context, true);
		//				}
		//				foreach(temp, caseexpr->args)
		//				{
		//					CaseWhen   *when = (CaseWhen *) lfirst(temp);
		//					Node	   *w = (Node *) when->expr;
		//
		//					if (!PRETTY_INDENT(context))
		//						appendStringInfoChar(str, ' ');
		//					appendContextKeyword(context, "WHEN ",
		//										 0, 0, 0);
		//					if (caseexpr->arg)
		//					{
		//						/*
		//						 * The parser should have produced WHEN clauses of the
		//						 * form "CaseTestExpr = RHS"; we want to show just the
		//						 * RHS.  If the user wrote something silly like "CASE
		//						 * boolexpr WHEN TRUE THEN ...", then the optimizer's
		//						 * simplify_boolean_equality() may have reduced this
		//						 * to just "CaseTestExpr" or "NOT CaseTestExpr", for
		//						 * which we have to show "TRUE" or "FALSE".  Also,
		//						 * depending on context the original CaseTestExpr
		//						 * might have been reduced to a Const (but we won't
		//						 * see "WHEN Const").
		//						 */
		//						if (IsA(w, OpExpr))
		//						{
		//							Node	   *rhs;
		//
		//							Assert(IsA(linitial(((OpExpr *) w)->args),
		//									   CaseTestExpr) ||
		//								   IsA(linitial(((OpExpr *) w)->args),
		//									   Const));
		//							rhs = (Node *) lsecond(((OpExpr *) w)->args);
		//							get_rule_expr(rhs, context, false);
		//						}
		//						else if (IsA(w, CaseTestExpr))
		//							appendStringInfo(str, "TRUE");
		//						else if (not_clause(w))
		//						{
		//							Assert(IsA(get_notclausearg((Expr *) w),
		//									   CaseTestExpr));
		//							appendStringInfo(str, "FALSE");
		//						}
		//						else
		//							elog(ERROR, "unexpected CASE WHEN clause: %d",
		//								 (int) nodeTag(w));
		//					}
		//					else
		//						get_rule_expr(w, context, false);
		//					appendStringInfo(str, " THEN ");
		//					get_rule_expr((Node *) when->result, context, true);
		//				}
		//				if (!PRETTY_INDENT(context))
		//					appendStringInfoChar(str, ' ');
		//				appendContextKeyword(context, "ELSE ",
		//									 0, 0, 0);
		//				get_rule_expr((Node *) caseexpr->defresult, context, true);
		//				if (!PRETTY_INDENT(context))
		//					appendStringInfoChar(str, ' ');
		//				appendContextKeyword(context, "END",
		//									 -PRETTYINDENT_VAR, 0, 0);
	}
	break;

	case T_ArrayExpr:
	{
		ArrayExpr *arrayexpr = (ArrayExpr *) node;

		//				appendStringInfo(str, "ARRAY[");
		//				get_rule_expr((Node *) arrayexpr->elements, context, true);
		//				appendStringInfoChar(str, ']');
	}
	break;

	case T_RowExpr:
	{
		//				RowExpr    *rowexpr = (RowExpr *) node;
		//				TupleDesc	tupdesc = NULL;
		//				ListCell   *arg;
		//				int			i;
		//				char	   *sep;
		//
		//				/*
		//				 * If it's a named type and not RECORD, we may have to skip
		//				 * dropped columns and/or claim there are NULLs for added
		//				 * columns.
		//				 */
		//				if (rowexpr->row_typeid != RECORDOID)
		//				{
		//					tupdesc = lookup_rowtype_tupdesc(rowexpr->row_typeid, -1);
		//					Assert(list_length(rowexpr->args) <= tupdesc->natts);
		//				}
		//
		//				/*
		//				 * SQL99 allows "ROW" to be omitted when there is more than
		//				 * one column, but for simplicity we always print it.
		//				 */
		//				appendStringInfo(str, "ROW(");
		//				sep = "";
		//				i = 0;
		//				foreach(arg, rowexpr->args)
		//				{
		//					Node	   *e = (Node *) lfirst(arg);
		//
		//					if (tupdesc == NULL ||
		//						!tupdesc->attrs[i]->attisdropped)
		//					{
		//						appendStringInfoString(str, sep);
		//						get_rule_expr(e, context, true);
		//						sep = ", ";
		//					}
		//					i++;
		//				}
		//				if (tupdesc != NULL)
		//				{
		//					while (i < tupdesc->natts)
		//					{
		//						if (!tupdesc->attrs[i]->attisdropped)
		//						{
		//							appendStringInfoString(str, sep);
		//							appendStringInfo(str, "NULL");
		//							sep = ", ";
		//						}
		//						i++;
		//					}
		//
		//					ReleaseTupleDesc(tupdesc);
		//				}
		//				appendStringInfo(str, ")");
		//				if (rowexpr->row_format == COERCE_EXPLICIT_CAST)
		//					appendStringInfo(str, "::%s",
		//						  format_type_with_typemod(rowexpr->row_typeid, -1));
	}
	break;

	case T_RowCompareExpr:
	{
		//				RowCompareExpr *rcexpr = (RowCompareExpr *) node;
		//				ListCell   *arg;
		//				char	   *sep;
		//
		//				/*
		//				 * SQL99 allows "ROW" to be omitted when there is more than
		//				 * one column, but for simplicity we always print it.
		//				 */
		//				appendStringInfo(str, "(ROW(");
		//				sep = "";
		//				foreach(arg, rcexpr->largs)
		//				{
		//					Node	   *e = (Node *) lfirst(arg);
		//
		//					appendStringInfoString(str, sep);
		//					get_rule_expr(e, context, true);
		//					sep = ", ";
		//				}
		//
		//				/*
		//				 * We assume that the name of the first-column operator will
		//				 * do for all the rest too.  This is definitely open to
		//				 * failure, eg if some but not all operators were renamed
		//				 * since the construct was parsed, but there seems no way to
		//				 * be perfect.
		//				 */
		//				appendStringInfo(str, ") %s ROW(",
		//						  generate_operator_name(linitial_oid(rcexpr->opnos),
		//										   exprType(linitial(rcexpr->largs)),
		//										 exprType(linitial(rcexpr->rargs))));
		//				sep = "";
		//				foreach(arg, rcexpr->rargs)
		//				{
		//					Node	   *e = (Node *) lfirst(arg);
		//
		//					appendStringInfoString(str, sep);
		//					get_rule_expr(e, context, true);
		//					sep = ", ";
		//				}
		//				appendStringInfo(str, "))");
	}
	break;

	case T_CoalesceExpr:
	{
		CoalesceExpr *coalesceexpr = (CoalesceExpr *) node;

		WRITE_START("Coalesce");
		parseBackExpr((Node *) coalesceexpr->args, context, proType, true, depth);
		WRITE_END("Coalesce");
	}
	break;

	case T_MinMaxExpr:
	{
		MinMaxExpr *minmaxexpr = (MinMaxExpr *) node;

		switch (minmaxexpr->op)
		{
			case IS_GREATEST:
				WRITE_START("Greatest");
				parseBackExpr((Node *) minmaxexpr->args, context, proType, true, depth);
				WRITE_END("Greatest");
			break;
			case IS_LEAST:
				WRITE_START("Least");
				parseBackExpr((Node *) minmaxexpr->args, context, proType, true, depth);
				WRITE_END("Least");
			break;
		}
	}
	break;

	case T_XmlExpr:
	{
		//				XmlExpr    *xexpr = (XmlExpr *) node;
		//				bool		needcomma = false;
		//				ListCell   *arg;
		//				ListCell   *narg;
		//				Const	   *con;
		//
		//				switch (xexpr->op)
		//				{
		//					case IS_XMLCONCAT:
		//						appendStringInfoString(str, "XMLCONCAT(");
		//						break;
		//					case IS_XMLELEMENT:
		//						appendStringInfoString(str, "XMLELEMENT(");
		//						break;
		//					case IS_XMLFOREST:
		//						appendStringInfoString(str, "XMLFOREST(");
		//						break;
		//					case IS_XMLPARSE:
		//						appendStringInfoString(str, "XMLPARSE(");
		//						break;
		//					case IS_XMLPI:
		//						appendStringInfoString(str, "XMLPI(");
		//						break;
		//					case IS_XMLROOT:
		//						appendStringInfoString(str, "XMLROOT(");
		//						break;
		//					case IS_XMLSERIALIZE:
		//						appendStringInfoString(str, "XMLSERIALIZE(");
		//						break;
		//					case IS_DOCUMENT:
		//						break;
		//				}
		//				if (xexpr->op == IS_XMLPARSE || xexpr->op == IS_XMLSERIALIZE)
		//				{
		//					if (xexpr->xmloption == XMLOPTION_DOCUMENT)
		//						appendStringInfoString(str, "DOCUMENT ");
		//					else
		//						appendStringInfoString(str, "CONTENT ");
		//				}
		//				if (xexpr->name)
		//				{
		//					appendStringInfo(str, "NAME %s",
		//									 quote_identifier(map_xml_name_to_sql_identifier(xexpr->name)));
		//					needcomma = true;
		//				}
		//				if (xexpr->named_args)
		//				{
		//					if (xexpr->op != IS_XMLFOREST)
		//					{
		//						if (needcomma)
		//							appendStringInfoString(str, ", ");
		//						appendStringInfoString(str, "XMLATTRIBUTES(");
		//						needcomma = false;
		//					}
		//					forboth(arg, xexpr->named_args, narg, xexpr->arg_names)
		//					{
		//						Node	   *e = (Node *) lfirst(arg);
		//						char	   *argname = strVal(lfirst(narg));
		//
		//						if (needcomma)
		//							appendStringInfoString(str, ", ");
		//						get_rule_expr((Node *) e, context, true);
		//						appendStringInfo(str, " AS %s",
		//										 quote_identifier(map_xml_name_to_sql_identifier(argname)));
		//						needcomma = true;
		//					}
		//					if (xexpr->op != IS_XMLFOREST)
		//						appendStringInfoChar(str, ')');
		//				}
		//				if (xexpr->args)
		//				{
		//					if (needcomma)
		//						appendStringInfoString(str, ", ");
		//					switch (xexpr->op)
		//					{
		//						case IS_XMLCONCAT:
		//						case IS_XMLELEMENT:
		//						case IS_XMLFOREST:
		//						case IS_XMLPI:
		//						case IS_XMLSERIALIZE:
		//							/* no extra decoration needed */
		//							get_rule_expr((Node *) xexpr->args, context, true);
		//							break;
		//						case IS_XMLPARSE:
		//							Assert(list_length(xexpr->args) == 2);
		//
		//							get_rule_expr((Node *) linitial(xexpr->args),
		//										  context, true);
		//
		//							con = (Const *) lsecond(xexpr->args);
		//							Assert(IsA(con, Const));
		//							Assert(!con->constisnull);
		//							if (DatumGetBool(con->constvalue))
		//								appendStringInfoString(str,
		//													 " PRESERVE WHITESPACE");
		//							else
		//								appendStringInfoString(str,
		//													   " STRIP WHITESPACE");
		//							break;
		//						case IS_XMLROOT:
		//							Assert(list_length(xexpr->args) == 3);
		//
		//							get_rule_expr((Node *) linitial(xexpr->args),
		//										  context, true);
		//
		//							appendStringInfoString(str, ", VERSION ");
		//							con = (Const *) lsecond(xexpr->args);
		//							if (IsA(con, Const) &&
		//								con->constisnull)
		//								appendStringInfoString(str, "NO VALUE");
		//							else
		//								get_rule_expr((Node *) con, context, false);
		//
		//							con = (Const *) lthird(xexpr->args);
		//							Assert(IsA(con, Const));
		//							if (con->constisnull)
		//								 /* suppress STANDALONE NO VALUE */ ;
		//							else
		//							{
		//								switch (DatumGetInt32(con->constvalue))
		//								{
		//									case XML_STANDALONE_YES:
		//										appendStringInfoString(str,
		//														 ", STANDALONE YES");
		//										break;
		//									case XML_STANDALONE_NO:
		//										appendStringInfoString(str,
		//														  ", STANDALONE NO");
		//										break;
		//									case XML_STANDALONE_NO_VALUE:
		//										appendStringInfoString(str,
		//													", STANDALONE NO VALUE");
		//										break;
		//									default:
		//										break;
		//								}
		//							}
		//							break;
		//						case IS_DOCUMENT:
		//							get_rule_expr_paren((Node *) xexpr->args, context, false, node);
		//							break;
		//					}
		//
		//				}
		//				if (xexpr->op == IS_XMLSERIALIZE)
		//					appendStringInfo(str, " AS %s", format_type_with_typemod(xexpr->type,
		//															 xexpr->typmod));
		//				if (xexpr->op == IS_DOCUMENT)
		//					appendStringInfoString(str, " IS DOCUMENT");
		//				else
		//					appendStringInfoChar(str, ')');
	}
	break;

	case T_NullIfExpr:
	{
		NullIfExpr *nullifexpr = (NullIfExpr *) node;

		WRITE_START("NullIf");
		parseBackExpr((Node *) nullifexpr->args, context, proType, true, depth);
		WRITE_END("NullIf");
	}
	break;

	case T_NullTest:
	{
		NullTest *ntest = (NullTest *) node;

		switch (ntest->nulltesttype)
		{
			case IS_NULL:
			WRITE_START("IsNull");
			parseBackExpr((Node *) ntest->arg, context, proType, true, depth);
			WRITE_END("IsNull");
			break;
			case IS_NOT_NULL:
			WRITE_START("IsNotNull");
			parseBackExpr((Node *) ntest->arg, context, proType, true, depth);
			WRITE_END("IsNotNull");
			break;
			default:
			elog(ERROR, "unrecognized nulltesttype: %d",
					(int) ntest->nulltesttype);
		}
	}
	break;

	case T_BooleanTest:
	{
		BooleanTest *btest = (BooleanTest *) node;

		switch (btest->booltesttype)
		{
			case IS_TRUE:
			WRITE_START("IsTrue");
			parseBackExpr((Node *) btest->arg, context, proType, false, depth);
			WRITE_END("IsTrue");
			break;
			case IS_NOT_TRUE:
			WRITE_START("IsNotTrue");
			parseBackExpr((Node *) btest->arg, context, proType, false, depth);
			WRITE_END("IsNotTrue");
			break;
			case IS_FALSE:
			WRITE_START("IsFalse");
			parseBackExpr((Node *) btest->arg, context, proType, false, depth);
			WRITE_END("IsFalse");
			break;
			case IS_NOT_FALSE:
			WRITE_START("IsNotFalse");
			parseBackExpr((Node *) btest->arg, context, proType, false, depth);
			WRITE_END("IsNotFalse");
			break;
			case IS_UNKNOWN:
			WRITE_START("IsUnknown");
			parseBackExpr((Node *) btest->arg, context, proType, false, depth);
			WRITE_END("IsUnknown");
			break;
			case IS_NOT_UNKNOWN:
			WRITE_START("IsNotUnknown");
			parseBackExpr((Node *) btest->arg, context, proType, false, depth);
			WRITE_END("IsNotUnknown");
			break;
			default:
			elog(ERROR, "unrecognized booltesttype: %d",
					(int) btest->booltesttype);
		}
	}
	break;

	case T_CoerceToDomain:
	{
		CoerceToDomain *ctest = (CoerceToDomain *) node;
		Node *arg = (Node *) ctest->arg;

		if (ctest->coercionformat == COERCE_IMPLICIT_CAST &&
				!showImplicit)
		{
			/* don't show the implicit cast */
			parseBackExpr(arg, context, proType, false, depth);
		}
		else
		{
			parseBackCoercionExpr(arg, context,
					ctest->resulttype,
					ctest->resulttypmod,
					node, proType, depth);
		}
	}
	break;

	case T_CoerceToDomainValue:
	appendStringInfo(str, "VALUE");
	break;

	//			case T_SetToDefault:
	//				appendStringInfo(str, "DEFAULT");
	//				break;

	case T_CurrentOfExpr:
	{
		CurrentOfExpr *cexpr = (CurrentOfExpr *) node;

		if (cexpr->cursor_name)
		appendStringInfo(str, "CURRENT OF %s",
				quote_identifier(cexpr->cursor_name));
		else
		appendStringInfo(str, "CURRENT OF $%d",
				cexpr->cursor_param);
	}
	break;

	case T_List:
	{
		ListCell *l;

		foreach(l, (List *) node)
		{
			parseBackExpr((Node *) lfirst(l), context, proType, showImplicit, depth);
		}
	}
	break;

	default:
	elog(ERROR, "unrecognized node type: %d", (int) nodeTag(node));
	break;
}
}

/*
 *
 */

static void
parseBackConstExpr(Const *constval, ParseXMLContext *context, int depth)
{
	int indendHelper;
	StringInfo str = context->buf;
	Oid typoutput;
	bool typIsVarlena;
	char *extval;
	char *valptr;
	char *typeName;
	bool isfloat = false;

	typeName = format_type_with_typemod(constval->consttype,
			constval->consttypmod);

	WRITE_OPEN_ONE("Const");
	WRITE_ATTR("type", typeName);
	WRITE_CLOSE_NOBR();

	// is a null constant?
	if (constval->constisnull)
	{
		WRITE_EMPTY("Null");
		WRITE_END_ONE("Const");

		return;
	}

	getTypeOutputInfo(constval->consttype, &typoutput, &typIsVarlena);

	extval = OidOutputFunctionCall(typoutput, constval->constvalue);

	switch (constval->consttype)
	{
	case INT2OID:
	case INT4OID:
	case INT8OID:
	case OIDOID:
	case FLOAT4OID:
	case FLOAT8OID:
	case NUMERICOID:
	{
		/*
		 * These types are printed without quotes unless they contain
		 * values that aren't accepted by the scanner unquoted (e.g.,
		 * 'NaN').	Note that strtod() and friends might accept NaN,
		 * so we can't use that to test.
		 *
		 * In reality we only need to defend against infinity and NaN,
		 * so we need not get too crazy about pattern matching here.
		 */
		if (strspn(extval, "0123456789+-eE.") == strlen(extval))
		{
			appendStringInfoString(str, extval);
			if (strcspn(extval, "eE.") != strlen(extval))
				isfloat = true; /* it looks like a float */
		}
		else
			appendStringInfo(str, "'%s'", extval);
	}
		break;

	case BITOID:
	case VARBITOID:
		appendStringInfo(str, "B'%s'", extval);
		break;

	case BOOLOID:
		if (strcmp(extval, "t") == 0)
			appendStringInfo(str, "true");
		else
			appendStringInfo(str, "false");
		break;

	default:

		/* We have to escape special characters in the string representation
		 * of the constant because we are generating XML. */
		for (valptr = extval; *valptr; valptr++)
		{
			char ch = *valptr;

			switch (ch)
			{
			case '<':
				appendStringInfoString(str,"&lt;");
				break;
			case '>':
				appendStringInfoString(str,"&gt;");
				break;
			case '&':
				appendStringInfoString(str,"&amp;");
				break;
			case '\'':
				appendStringInfoString(str,"&apos;");
				break;
			case '\"':
				appendStringInfoString(str,"&quot;");
				break;
			default:
				appendStringInfoChar(str, ch);
			}
		}
		break;
	}

	WRITE_END_ONE("Const");

	pfree(extval);
}

/*
 *
 */

static void
parseBackAggExpr (Aggref *agg, ParseXMLContext *context, TransProjType projType, int depth)
{
	StringInfo str = context->buf;
	Oid			argtypes[FUNC_MAX_ARGS];
	int			nargs;
	ListCell   *l;
	TransSubInfo *aggInfo;
	TransSubInfo *projInfo;
	TransParseRange *range;
	TransParseRange *projRange;
	char *aggFuncName;


	aggInfo = NULL;
	projInfo = NULL;
	if (projType == ProjOverAgg)
		aggInfo = (TransSubInfo *) list_nth(*context->infoStack, 0);
	else if (projType == ProjUnderAgg)
		projInfo = (TransSubInfo *) list_nth(*context->infoStack, 0);
	else if (projType == ProjBothAgg)
	{
		aggInfo = (TransSubInfo *) list_nth(*context->infoStack, 0);
		projInfo = (TransSubInfo *) list_nth(*context->infoStack, 1);
	}

	if ((aggInfo && aggInfo->isStatic) || (projInfo && projInfo->isStatic))
	{
		aggInfo = NULL;
		projInfo = NULL;
	}

	if (aggInfo)
		MAKE_RANGE(range, aggInfo);

	nargs = 0;
	foreach(l, agg->args)
	{
		if (nargs >= FUNC_MAX_ARGS)
			ereport(ERROR,
					(errcode(ERRCODE_TOO_MANY_ARGUMENTS),
					 errmsg("too many arguments")));
		argtypes[nargs] = exprType((Node *) lfirst(l));
		nargs++;
	}

	aggFuncName = generate_function_name(agg->aggfnoid, nargs, argtypes);

	WRITE_START_ONE(aggFuncName);

	if (agg->aggdistinct)
		WRITE_START_ONE("Distinct");

	if (projInfo)
		MAKE_RANGE(projRange, projInfo);

	/* aggstar can be set only in zero-argument aggregates */
	if (agg->aggstar)
		appendStringInfoChar(str, '*');
	else
		parseBackExpr((Node *) agg->args, context, projType, true, depth);

	if (projInfo)
		projRange->end = str->len;

	if (agg->aggdistinct)
			WRITE_END_ONE("Distinct");

	WRITE_END_ONE(aggFuncName);

	if (aggInfo)
		range->end = str->len;
}


/*
 *
 */

static void
parseBackFuncExpr(FuncExpr *func, ParseXMLContext *context,
		TransProjType projType, bool showimplicit, int depth)
{
	StringInfo str = context->buf;
	Oid funcoid = func->funcid;
	Oid argtypes[FUNC_MAX_ARGS];
	int nargs;
	ListCell *l;
	char *funcName;

	/*
	 * If the function call came from an implicit coercion, then just show the
	 * first argument --- unless caller wants to see implicit coercions.
	 */
	if (func->funcformat == COERCE_IMPLICIT_CAST && !showimplicit)
	{
		parseBackExpr((Node *) linitial(func->args), context, projType, false,
				depth);
		return;
	}

	/*
	 * If the function call came from a cast, then show the first argument
	 * plus an explicit cast operation.
	 */
	if (func->funcformat == COERCE_EXPLICIT_CAST || func->funcformat
			== COERCE_IMPLICIT_CAST)
	{
		Node *arg = linitial(func->args);
		Oid rettype = func->funcresulttype;
		int32 coercedTypmod;

		/* Get the typmod if this is a length-coercion function */
		(void) exprIsLengthCoercion((Node *) func, &coercedTypmod);

		parseBackCoercionExpr(arg, context, rettype, coercedTypmod,
				(Node *) func, projType, depth);

		return;
	}

	/*
	 * Normal function: display as proname(args).  First we need to extract
	 * the argument datatypes.
	 */
	nargs = 0;
	foreach(l, func->args)
	{
		if (nargs >= FUNC_MAX_ARGS)
			ereport(ERROR,
					(errcode(ERRCODE_TOO_MANY_ARGUMENTS),
							errmsg("too many arguments")));
		argtypes[nargs] = exprType((Node *) lfirst(l));
		nargs++;
	}

	funcName = generate_function_name(funcoid, nargs, argtypes);

	WRITE_START_ONE(funcName);
	parseBackExpr((Node *) func->args, context, projType, true, depth);
	WRITE_END_ONE(funcName);
}

/*
 * Parse back a sublink expression.
 */

static void
parseBackSublink(SubLink *sublink, ParseXMLContext *context,
		TransProjType projType, int depth)
{
	OpExpr *opexpr;
	char *opName = NULL;
	char *verbName = NULL;
	Node *outerInput;
	StringInfo str = context->buf;
	int indendHelper;

	/*
	 * Note that we print the name of only the first operator, when there are
	 * multiple combining operators.  This is an approximation that could go
	 * wrong in various scenarios (operators in different schemas, renamed
	 * operators, etc) but there is not a whole lot we can do about it, since
	 * the syntax allows only one operator to be shown.
	 */
	if (sublink->testexpr)
	{
		if (IsA(sublink->testexpr, OpExpr))
		{
			/* single combining operator */
			opexpr = (OpExpr *) sublink->testexpr;
			outerInput = (Node * ) linitial(opexpr->args);
			opName = generate_operator_name(opexpr->opno,
											exprType(linitial(opexpr->args)),
											exprType(lsecond(opexpr->args)));
		}
		else if (IsA(sublink->testexpr, BoolExpr))
		{
			/* multiple combining operators, = or <> cases */
			ListCell   *l;

			foreach(l, ((BoolExpr *) sublink->testexpr)->args)
			{
				opexpr = (OpExpr *) lfirst(l);

				Assert(IsA(opexpr, OpExpr));

				outerInput = (Node *) linitial(opexpr->args);
				if (!opName)
					opName = generate_operator_name(opexpr->opno,
											exprType(linitial(opexpr->args)),
											exprType(lsecond(opexpr->args)));
			}
		}
		else if (IsA(sublink->testexpr, RowCompareExpr))
		{
			/* multiple combining operators, < <= > >= cases */
			RowCompareExpr *rcexpr = (RowCompareExpr *) sublink->testexpr;

			outerInput = (Node *) rcexpr->largs;
			opName = generate_operator_name(linitial_oid(rcexpr->opnos),
											exprType(linitial(rcexpr->largs)),
										  exprType(linitial(rcexpr->rargs)));
		}
		else
			elog(ERROR, "unrecognized testexpr type: %d",
				 (int) nodeTag(sublink->testexpr));

		verbName = getVerboseOpName(opName);
	}

	switch(sublink->subLinkType)
	{
		case EXISTS_SUBLINK:
			WRITE_START_ONE("Exists");
			parseBackXMLQueryNode((Query *) sublink->subselect, NULL, depth, context,
					true, false, NULL, NULL);
			WRITE_END_ONE("Exists");
			break;
		case ALL_SUBLINK:
			WRITE_START_ONE("All");
			WRITE_OP_OPEN;
			parseBackExpr(outerInput, context, projType, false, depth);
			parseBackXMLQueryNode((Query *) sublink->subselect, NULL, depth, context,
					true, false, NULL, NULL);
			WRITE_OP_CLOSE;
			WRITE_END_ONE("All");
			break;
		case ANY_SUBLINK:
			WRITE_START_ONE("Any");
			WRITE_OP_OPEN;
			parseBackExpr(outerInput, context, projType, false, depth);
			parseBackXMLQueryNode((Query *) sublink->subselect, NULL, depth, context,
					true, false, NULL, NULL);
			WRITE_OP_CLOSE;
			WRITE_END_ONE("Any");
			break;
		case ROWCOMPARE_SUBLINK:
			WRITE_START_ONE("ROW");
			WRITE_OP_OPEN;
			parseBackExpr(outerInput, context, projType, false, depth);
			parseBackXMLQueryNode((Query *) sublink->subselect, NULL, depth, context,
					true, false, NULL, NULL);
			WRITE_OP_CLOSE;
			WRITE_END_ONE("ROW");
			break;
		case EXPR_SUBLINK:
			parseBackXMLQueryNode((Query *) sublink->subselect, NULL, depth, context,
					true, false, NULL, NULL);
			break;
		case ARRAY_SUBLINK:
			WRITE_START_ONE("Array");
			parseBackXMLQueryNode((Query *) sublink->subselect, NULL, depth, context,
					true, false, NULL, NULL);
			WRITE_END_ONE("Array");
			break;
		default:
			//TODO error
			break;
	}
}

static void
parseBackOpExpr(OpExpr *op, ParseXMLContext *context, TransProjType projType,
		int depth)
{
	StringInfo str = context->buf;
	Oid opno = op->opno;
	List *args = op->args;
	char *opName;
	char *verbName;
	Node *arg1, *arg2;
	int indendHelper;

	if (list_length(args) == 2)
	{
		/* binary operator */
		arg1 = (Node *) linitial(args);
		arg2 = (Node *) lsecond(args);

		opName = generate_operator_name(opno, exprType(arg1), exprType(arg2));

		verbName = getVerboseOpName(opName);
	}
	else
	{
		/* unary operator --- but which side? */
		HeapTuple tp;
		Form_pg_operator optup;

		arg1 = (Node *) linitial(args);
		tp = SearchSysCache(OPEROID, ObjectIdGetDatum(opno), 0, 0, 0);
		if (!HeapTupleIsValid(tp))
			elog(ERROR, "cache lookup failed for operator %u", opno);
		optup = (Form_pg_operator) GETSTRUCT(tp);
		switch (optup->oprkind)
		{
			case 'l':
			opName = generate_operator_name(opno,
					InvalidOid,
					exprType(arg1));
			break;
			case 'r':
			opName = generate_operator_name(opno,
					exprType(arg1),
					InvalidOid);
			break;
			default:
			elog(ERROR, "bogus oprkind: %d", optup->oprkind);
		}

		verbName = getVerboseOpName(opName);

		ReleaseSysCache(tp);
	}

	/* if we have a name for the operator use it
	 * otherwise represent it as a function call */
	WRITE_OP_OPEN;

	// write arguments
	if (list_length(args) == 2)
	{
		parseBackExpr(arg1, context, projType, true, depth);
		parseBackExpr(arg2, context, projType, true, depth);
	}
	else
		parseBackExpr(arg1, context, projType, true, depth);

	// close element
	WRITE_OP_CLOSE;
}

/*
 *
 */
static char *
getVerboseOpName(char *opName)
{
	if (strncmp(opName, "*", 1) == 0)
		return "Mult";
	if (strncmp(opName, "/", 1) == 0)
		return "Div";
	if (strncmp(opName, "%", 1) == 0)
		return "Mod";
	if (strncmp(opName, "<", 1) == 0)
		return "LessThan";
	if (strncmp(opName, ">", 1) == 0)
		return "GreaterThan";
	if (strncmp(opName, "<=", 1) == 0)
		return "LessEqual";
	if (strncmp(opName, ">=", 1) == 0)
		return "GreaterEqual";
	if (strncmp(opName, "=", 1) == 0)
		return "Equal";
	if (strncmp(opName, "!=", 1) == 0 || strncmp(opName, "<>", 1) == 0)
		return "NotEqual";
	if (strncmp(opName, "~~", 1) == 0)
		return "Like";

	return NULL;
}

/*
 *
 */

static void
parseBackCoercionExpr(Node *arg, ParseXMLContext *context, Oid resulttype,
		int32 resulttypmod, Node *parentNode, TransProjType projType, int depth)
{
	StringInfo str = context->buf;
	char *typeName;

	typeName = format_type_with_typemod(resulttype, resulttypmod);

	WRITE_OPEN_ONE("CastAs");
	WRITE_ATTR("type", typeName);
	WRITE_CLOSE_NOBR();

	/*
	 * Since parse_coerce.c doesn't immediately collapse application of
	 * length-coercion functions to constants, what we'll typically see in
	 * such cases is a Const with typmod -1 and a length-coercion function
	 * right above it.	Avoid generating redundant output. However, beware of
	 * suppressing casts when the user actually wrote something like
	 * 'foo'::text::char(3).
	 */
	if (arg && IsA(arg, Const) && ((Const *) arg)->consttype == resulttype
			&& ((Const *) arg)->consttypmod == -1)
	{
		/* Show the constant without normal ::typename decoration */
		parseBackConstExpr((Const *) arg, context, -1);
	}
	else
	{
		parseBackExpr(arg, context, projType, false, depth);
	}

	WRITE_END_ONE("CastAs");
}

/*
 *
 */

static char *
parseBackVar(Var *var, int levelsup, bool showstar, ParseXMLContext *context)
{
	StringInfo str = context->buf;
	RangeTblEntry *rte;
	AttrNumber attnum;
	int netlevelsup;
	deparse_namespace *dpns;
	char *schemaname;
	char *refname;
	char *attname;

	/* Find appropriate nesting depth */
	netlevelsup = var->varlevelsup + levelsup;
	if (netlevelsup >= list_length(context->namespaces))
		elog(ERROR, "bogus varlevelsup: %d offset %d", var->varlevelsup,
				levelsup);

	dpns = (deparse_namespace *) list_nth(context->namespaces,
				netlevelsup);

	/*
	 * Try to find the relevant RTE in this rtable.  In a plan tree, it's
	 * likely that varno is OUTER or INNER, in which case we must dig down
	 * into the subplans.
	 */
	if (var->varno >= 1 && var->varno <= list_length(dpns->rtable))
	{
		rte = rt_fetch(var->varno, dpns->rtable);
		attnum = var->varattno;
	}
	else
	{
		elog(ERROR, "bogus varno: %d", var->varno);
		return NULL; /* keep compiler quiet */
	}

	/* Identify names to use */
	schemaname = NULL; /* default assumptions */
	refname = rte->eref->aliasname;

	/* Exceptions occur only if the RTE is alias-less */
	if (rte->alias == NULL)
	{
		if (rte->rtekind == RTE_RELATION)
		{
			/*
			 * It's possible that use of the bare refname would find another
			 * more-closely-nested RTE, or be ambiguous, in which case we need
			 * to specify the schemaname to avoid these errors.
			 */
			if (find_rte_by_refname(rte->eref->aliasname,
					context->namespaces) != rte)
				schemaname = get_namespace_name(
						get_rel_namespace(rte->relid));
		}
		else if (rte->rtekind == RTE_JOIN)
		{
			/*
			 * If it's an unnamed join, look at the expansion of the alias
			 * variable.  If it's a simple reference to one of the input vars
			 * then recursively print the name of that var, instead. (This
			 * allows correct decompiling of cases where there are identically
			 * named columns on both sides of the join.) When it's not a
			 * simple reference, we have to just print the unqualified
			 * variable name (this can only happen with columns that were
			 * merged by USING or NATURAL clauses).
			 */
			if (rte->joinaliasvars == NIL)
				elog(ERROR, "cannot decompile join alias var in plan tree");
			if (attnum > 0)
			{
				Var *aliasvar;

				aliasvar = (Var *) list_nth(rte->joinaliasvars, attnum - 1);
				if (IsA(aliasvar, Var))
				{
					attname = parseBackVar(aliasvar,
							var->varlevelsup + levelsup, showstar, context);

					return attname;
				}
			}
			/* Unnamed join has neither schemaname nor refname */
			refname = NULL;
		}
	}

	WRITE_START_ONE("Var");

	if (attnum == InvalidAttrNumber)
		attname = NULL;
	else
		attname = get_rte_attribute_name(rte, attnum);

	if (refname && (context->varprefix || attname == NULL))
	{
		if (schemaname)
			appendStringInfo(str, "%s.",
					quote_identifier(schemaname));

		if (strcmp(refname, "*NEW*") == 0)
			appendStringInfoString(str, "new");
		else if (strcmp(refname, "*OLD*") == 0)
			appendStringInfoString(str, "old");
		else
			appendStringInfoString(str, quote_identifier(refname));

		if (attname || showstar)
			appendStringInfoChar(str, '.');
	}
	if (attname)
		appendStringInfoString(str, quote_identifier(attname));
	else if (showstar)
		appendStringInfoChar(str, '*');

	WRITE_END_ONE("Var");

	return attname;
}

		/*
		 * Display a sort/group clause.
		 *
		 * Also returns the expression tree, so caller need not find it again.
		 */
static Node *
parseBackSortgroupclause(SortClause *srt, List *tlist, bool force_colno,
		ParseXMLContext *context, TransProjType projType, int depth)
{
	StringInfo str = context->buf;
	TargetEntry *tle;
	Node *expr;

	tle = get_sortgroupclause_tle(srt, tlist);
	expr = (Node *) tle->expr;

	/*
	 * Use column-number form if requested by caller.  Otherwise, if
	 * expression is a constant, force it to be dumped with an explicit
	 * cast as decoration --- this is because a simple integer constant
	 * is ambiguous (and will be misinterpreted by findTargetlistEntry())
	 * if we dump it without any decoration.  Otherwise, just dump the
	 * expression normally.
	 */
	if (force_colno)
	{
		Assert(!tle->resjunk);
		appendStringInfo(str, "%d", tle->resno);
	}
	else if (expr && IsA(expr, Const))
		parseBackConstExpr((Const *) expr, context, 1);
	else
		parseBackExpr(expr, context, projType, true, depth);

	return expr;
}
