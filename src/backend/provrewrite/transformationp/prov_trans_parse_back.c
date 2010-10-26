/*-------------------------------------------------------------------------
 *
 * prov_trans_parse_back.c
 *	  	PERM C -
 *
 * Portions Copyright (c) 2009 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/transformationp/prov_trans_parse_back.c,v 1.542 09.09.2009 10:51:52 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "utils/varbit.h"
#include <unistd.h>
#include <fcntl.h>

#include "access/genam.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_depend.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_trigger.h"
#include "commands/defrem.h"
#include "commands/tablespace.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "nodes/makefuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/tlist.h"
#include "parser/gramparse.h"
#include "parser/keywords.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteHandler.h"
#include "rewrite/rewriteManip.h"
#include "rewrite/rewriteSupport.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/typcache.h"
#include "utils/xml.h"

#include "provrewrite/prov_parse_back_util.h"
#include "provrewrite/prov_trans_parse_back.h"
#include "provrewrite/prov_trans_util.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_trans_bitset.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/provstack.h"

/* typedefs */
typedef struct TransParseContext
{
	StringInfo	buf;			/* output buffer to append to */
	List	   *namespaces;		/* List of deparse_namespace nodes */
	int			prettyFlags;	/* enabling of pretty-print functions */
	int			indentLevel;	/* current indent level for prettyprint */
	bool		varprefix;		/* TRUE to print prefixes on Vars */
	TransProjType curProjType;
	List 		**ranges;
	List 		**rangeStack;
	List		**infoStack;
} TransParseContext;


/* macros */
#define MAKE_TRANSPARSECONTEXT(context) \
	do { \
		context = (TransParseContext *) palloc(sizeof(TransParseContext)); \
	} while (0)

#define TRANSPARSE_ARGS \
	TransParseContext *context, Node *parentInfo, bool inStatic





/* ----------
 * Local functions
 *
 * Most of these functions used to use fixed-size buffers to build their
 * results.  Now, they take an (already initialized) StringInfo object
 * as a parameter, and append their text output to its contents.
 * ----------
 */
static StringInfo parseBackTrans(Query *query, List **result);
static void get_query_def(Query *query, StringInfo buf, List *parentnamespace,
			  TupleDesc resultDesc, int prettyFlags, int startIndent, TRANSPARSE_ARGS);
static void get_values_def(List *values_lists, TransParseContext *context);
static void get_select_query_def(Query *query, TupleDesc resultDesc, TRANSPARSE_ARGS);
static void get_basic_select_query(Query *query, TupleDesc resultDesc, TRANSPARSE_ARGS);
static void get_target_list(Query *query, TupleDesc resultDesc, TRANSPARSE_ARGS);
static void get_setop_query(Node *setOp, Query *query,
				TupleDesc resultDesc, TRANSPARSE_ARGS, bool isRoot);
static Node *get_rule_sortgroupclause(SortClause *srt, List *tlist,
						 bool force_colno,
						 TransParseContext *context);
static char *get_variable(Var *var, int levelsup, bool showstar,
			 TransParseContext *context);
static void appendContextKeyword(TransParseContext *context, const char *str,
					 int indentBefore, int indentAfter, int indentPlus);
static void get_rule_expr(Node *node, TransParseContext *context,
			  bool showimplicit);
static void get_oper_expr(OpExpr *expr, TransParseContext *context);
static void get_func_expr(FuncExpr *expr, TransParseContext *context,
			  bool showimplicit);
static void get_agg_expr(Aggref *aggref, TransParseContext *context);
static void get_coercion_expr(Node *arg, TransParseContext *context,
				  Oid resulttype, int32 resulttypmod,
				  Node *parentNode);
static void get_const_expr(Const *constval, TransParseContext *context,
			   int showtype);
static void get_sublink_expr(SubLink *sublink, TRANSPARSE_ARGS);
static void get_from_clause(Query *query, const char *prefix,
				TRANSPARSE_ARGS);
static void get_from_clause_item(Node *jtnode, Query *query,
					 TRANSPARSE_ARGS, bool isRoot);
static void get_from_clause_alias(Alias *alias, RangeTblEntry *rte,
					  TransParseContext *context);
static void get_from_clause_coldeflist(List *names, List *types, List *typmods,
						   TransParseContext *context);
static Node *processIndirection(Node *node, TransParseContext *context,
				   bool printit);
static void printSubscripts(ArrayRef *aref, TransParseContext *context);
static void parseAnnotations (List *annots, bool start, StringInfo buf);


/*
 *
 */

void
parseBackTransToSQL (Query *query, TransRepQueryInfo *repInfo,
		MemoryContext funcPrivateCtx)
{
	TransProvInfo *info;
	TransSubInfo *subInfo;
	List *ranges;
	StringInfo str;
//	MemoryContext old;

	info = GET_TRANS_INFO(query);
	subInfo = getRootSubForNode(info);
	ranges = NIL;

	str = parseBackTrans(query, &ranges);

	postprocessRanges (ranges, str, repInfo, funcPrivateCtx);
}

/*
 *
 */

void
postprocessRanges (List *ranges, StringInfo str, TransRepQueryInfo *repInfo,
		MemoryContext funcPrivateCtx)
{
	TransParseRange *range;
	int numRanges;
	int i;
	ListCell *lc;
	size_t varbitLen;

	numRanges = list_length(ranges);

	repInfo->numRanges = numRanges;
	repInfo->begins = (int *) MemoryContextAlloc(funcPrivateCtx, sizeof(int) * numRanges);
	repInfo->ends = (int *) MemoryContextAlloc(funcPrivateCtx, sizeof(int) * numRanges);
	repInfo->sets = (VarBit **) MemoryContextAlloc(funcPrivateCtx, sizeof(VarBit *) * numRanges);
	repInfo->stringPointers = (char **) MemoryContextAlloc(funcPrivateCtx, sizeof(char *) * numRanges);
	repInfo->string = MemoryContextAlloc(funcPrivateCtx, str->len + 1);
	memcpy(repInfo->string, str->data, str->len + 1);

	//CHECK need ordering of ranges?
//	foreachi(lc, i, ranges);
	for((lc) = list_head(ranges), (i) = 0; (lc) != NULL; (lc) = (lc)->next, i++)
	{
		range = (TransParseRange *) lfirst(lc);

		repInfo->begins[i] = range->begin;
		repInfo->ends[i] = range->end;
		repInfo->stringPointers[i] = repInfo->string + range->begin;

		varbitLen = VARBITTOTALLEN(range->set->bit_len);
		repInfo->sets[i] = MemoryContextAlloc(funcPrivateCtx, varbitLen);
		memcpy(repInfo->sets[i], range->set, varbitLen);
	}
}

/*
 *
 */

static StringInfo
parseBackTrans(Query *query, List **result)
{
	StringInfo buf;
	TransParseContext *context;
	deparse_namespace namespc;
	TupleDesc resultDesc;
	List *attrList;
	ListCell *t;
	List *dummyStack;
	List *dummyInfo;

	dummyStack = NIL;
	dummyInfo = NIL;

	/* replace ambigous unnamed column names */
	replaceUnnamedColumnsWalker ((Node *) query, NULL);//CHECK that is does not break TransInfo strucutre of query
	correctRecurSubQueryAlias(query);

	/* init StringInfo */
	buf = makeStringInfo();

	/* create context */
	MAKE_TRANSPARSECONTEXT(context);
	context->buf = buf;
	context->indentLevel = 0;
	context->prettyFlags = 3;
	context->varprefix = true;
	context->namespaces = list_make1(&namespc);
	context->ranges = result;
	context->rangeStack = &dummyStack;
	context->infoStack = &dummyInfo;

	namespc.rtable = query->rtable;
	namespc.outer_plan = namespc.inner_plan = NULL;

	/* create TupleDesc for columns of query result */
	attrList = NIL;
	foreach(t, query->targetList)
	{
		TargetEntry *tle = lfirst(t);

		if (!tle->resjunk)
		{
			ColumnDef  *def = makeNode(ColumnDef);

			def->colname = pstrdup(tle->resname);
			def->typename = makeTypeNameFromOid(exprType((Node *) tle->expr),
											 exprTypmod((Node *) tle->expr));
			def->inhcount = 0;
			def->is_local = true;
			def->is_not_null = false;
			def->raw_default = NULL;
			def->cooked_default = NULL;
			def->constraints = NIL;

			attrList = lappend(attrList, def);
		}
	}

	resultDesc = BuildDescForRelation(attrList);

	/* parse back query */
	get_select_query_def(query, resultDesc, context,
			(Node *) getRootSubForNode(GET_TRANS_INFO(query)), false);

	return buf;
}


/* ----------
 * get_query_def			- Parse back one query parsetree
 *
 * If resultDesc is not NULL, then it is the output tuple descriptor for the
 * view represented by a SELECT query.
 * ----------
 */
static void
get_query_def(Query *query, StringInfo buf, List *parentnamespace,
			  TupleDesc resultDesc, int prettyFlags, int startIndent,
			  TRANSPARSE_ARGS)
{
	TransParseContext newContext;
	deparse_namespace dpns;

	/*
	 * Before we begin to examine the query, acquire locks on referenced
	 * relations, and fix up deleted columns in JOIN RTEs.	This ensures
	 * consistent results.	Note we assume it's OK to scribble on the passed
	 * querytree!
	 */
	AcquireRewriteLocks(query);

	newContext.buf = buf;
	newContext.namespaces = lcons(&dpns, list_copy(parentnamespace));
	newContext.varprefix = (parentnamespace != NIL ||
						 list_length(query->rtable) != 1);
	newContext.prettyFlags = prettyFlags;
	newContext.indentLevel = startIndent;
	newContext.ranges = context->ranges;
	newContext.rangeStack = context->rangeStack;
	newContext.infoStack = context->infoStack;
	newContext.curProjType = None;

	dpns.rtable = query->rtable;
	dpns.outer_plan = dpns.inner_plan = NULL;

	switch (query->commandType)
	{
		case CMD_SELECT:
			get_select_query_def(query, resultDesc, &newContext, parentInfo,
					inStatic);
			break;

		default:
			elog(ERROR, "unrecognized query command type: %d",
				 query->commandType);
			break;
	}
}

/* ----------
 * get_values_def			- Parse back a VALUES list
 * ----------
 */
static void
get_values_def(List *values_lists, TransParseContext *context)
{
	StringInfo	str = context->buf;
	bool		first_list = true;
	ListCell   *vtl;

	appendStringInfoString(str, "VALUES ");

	foreach(vtl, values_lists)
	{
		List	   *sublist = (List *) lfirst(vtl);
		bool		first_col = true;
		ListCell   *lc;

		if (first_list)
			first_list = false;
		else
			appendStringInfoString(str, ", ");

		appendStringInfoChar(str, '(');
		foreach(lc, sublist)
		{
			Node	   *col = (Node *) lfirst(lc);

			if (first_col)
				first_col = false;
			else
				appendStringInfoChar(str, ',');

			/*
			 * Strip any top-level nodes representing indirection assignments,
			 * then print the result.
			 */
			get_rule_expr(processIndirection(col, context, false),
						  context, false);
		}
		appendStringInfoChar(str, ')');
	}
}

/* ----------
 * get_select_query_def			- Parse back a SELECT parsetree
 * ----------
 */
static void
get_select_query_def(Query *query, TupleDesc resultDesc, TRANSPARSE_ARGS)
{
	StringInfo	str = context->buf;
	TransSubInfo *curSub;
	TransProvInfo *info;
	TransParseRange *range;
	bool		force_colno;
	char	   *sep;
	ListCell   *l;
	bool newStatic;

	info = GET_TRANS_INFO(query);
	newStatic = inStatic || info->isStatic;
	curSub = NULL;

	/* Get next subInfo if parent is not static */
	if (!inStatic)
	{
		curSub = getRootSubForNode(info);

		MAKE_RANGE(range, curSub);

		push(context->infoStack, curSub);
	}

	parseAnnotations(((List *) Provinfo(query)->annotations), true, str);

	/*
	 * If the Query node has a setOperations tree, then it's the top level of
	 * a UNION/INTERSECT/EXCEPT query; only the ORDER BY and LIMIT fields are
	 * interesting in the top query itself.
	 */
	if (query->setOperations)
	{
		curSub = (TransSubInfo *) info->root;
		get_setop_query(query->setOperations, query, resultDesc, context,
				(Node *) curSub, newStatic, true);
		/* ORDER BY clauses must be simple in this case */
		force_colno = true;
	}
	else
	{
		get_basic_select_query(query, resultDesc, context, (Node *) curSub,
				newStatic);
		force_colno = false;
	}

	/* Add the ORDER BY clause if given */
	if (query->sortClause != NIL)
	{
		appendContextKeyword(context, " ORDER BY ",
							 -PRETTYINDENT_STD, PRETTYINDENT_STD, 1);
		sep = "";
		foreach(l, query->sortClause)
		{
			SortClause *srt = (SortClause *) lfirst(l);
			Node	   *sortexpr;
			Oid			sortcoltype;
			TypeCacheEntry *typentry;

			appendStringInfoString(str, sep);
			sortexpr = get_rule_sortgroupclause(srt, query->targetList,
												force_colno, context);
			sortcoltype = exprType(sortexpr);
			/* See whether operator is default < or > for datatype */
			typentry = lookup_type_cache(sortcoltype,
					TYPECACHE_LT_OPR | TYPECACHE_GT_OPR);
			if (srt->sortop == typentry->lt_opr)
			{
				/* ASC is default, so emit nothing for it */
				if (srt->nulls_first)
					appendStringInfo(str, " NULLS FIRST");
			}
			else if (srt->sortop == typentry->gt_opr)
			{
				appendStringInfo(str, " DESC");
				/* DESC defaults to NULLS FIRST */
				if (!srt->nulls_first)
					appendStringInfo(str, " NULLS LAST");
			}
			else
			{
				appendStringInfo(str, " USING %s",
								 generate_operator_name(srt->sortop,
														sortcoltype,
														sortcoltype));
				/* be specific to eliminate ambiguity */
				if (srt->nulls_first)
					appendStringInfo(str, " NULLS FIRST");
				else
					appendStringInfo(str, " NULLS LAST");
			}
			sep = ", ";
		}
	}

	/* Add the LIMIT clause if given */
	if (query->limitOffset != NULL)
	{
		appendContextKeyword(context, " OFFSET ",
							 -PRETTYINDENT_STD, PRETTYINDENT_STD, 0);
		get_rule_expr(query->limitOffset, context, false);
	}
	if (query->limitCount != NULL)
	{
		appendContextKeyword(context, " LIMIT ",
							 -PRETTYINDENT_STD, PRETTYINDENT_STD, 0);
		if (IsA(query->limitCount, Const) &&
			((Const *) query->limitCount)->constisnull)
			appendStringInfo(str, "ALL");
		else
			get_rule_expr(query->limitCount, context, false);
	}

	/* Add FOR UPDATE/SHARE clauses if present */
	foreach(l, query->rowMarks)
	{
		RowMarkClause *rc = (RowMarkClause *) lfirst(l);
		RangeTblEntry *rte = rt_fetch(rc->rti, query->rtable);

		if (rc->forUpdate)
			appendContextKeyword(context, " FOR UPDATE",
					-PRETTYINDENT_STD, PRETTYINDENT_STD, 0);
		else
			appendContextKeyword(context, " FOR SHARE",
					-PRETTYINDENT_STD, PRETTYINDENT_STD, 0);
		appendStringInfo(str, " OF %s",
				quote_identifier(rte->eref->aliasname));
		if (rc->noWait)
			appendStringInfo(str, " NOWAIT");
	}

	parseAnnotations(((List *) Provinfo(query)->annotations), false, str);

	/* close range */
	if(!inStatic)
		range->end = str->len;
}

/*
 *
 */

static void
get_basic_select_query(Query *query, TupleDesc resultDesc, TRANSPARSE_ARGS)
{
	StringInfo	str = context->buf;
	char	   *sep;
	ListCell   *l;
	TransProvInfo *info;
	TransSubInfo *child;
	TransParseRange *range;
	bool infoBelow;

	info = GET_TRANS_INFO(query);
	infoBelow = !(equal(info->root, parentInfo));


	if (PRETTY_INDENT(context))
	{
		context->indentLevel += PRETTYINDENT_STD;
		appendStringInfoChar(str, ' ');
	}

	/*
	 * If the query looks like SELECT * FROM (VALUES ...), then print just the
	 * VALUES part.  This reverses what transformValuesClause() did at parse
	 * time.  If the jointree contains just a single VALUES RTE, we assume
	 * this case applies (without looking at the targetlist...)
	 */
	if (list_length(query->jointree->fromlist) == 1)
	{
		RangeTblRef *rtr = (RangeTblRef *) linitial(query->jointree->fromlist);

		if (IsA(rtr, RangeTblRef))
		{
			RangeTblEntry *rte = rt_fetch(rtr->rtindex, query->rtable);

			if (rte->rtekind == RTE_VALUES)
			{
				get_values_def(rte->values_lists, context);
				return;
			}
		}
	}

	/*
	 * Build up the query string - first we say SELECT
	 */
	appendStringInfo(str, "SELECT");

	/* Then we tell what to select (the targetlist) */
	get_target_list(query, resultDesc, context, parentInfo, inStatic);

	/* Add the FROM clause if needed */
	get_from_clause(query, " FROM ", context, parentInfo, inStatic);

	/* Add the WHERE clause if given */
	if (query->jointree->quals != NULL)
	{
		bool needWhere;

		child = getSpecificInfo((TransSubInfo *) info->root, SUBOP_Selection);

		if ((needWhere = (!inStatic && !equal(child, info->root))))
			MAKE_RANGE(range, child);

		appendContextKeyword(context, " WHERE ",
							 -PRETTYINDENT_STD, PRETTYINDENT_STD, 1);
		get_rule_expr(query->jointree->quals, context, false);

		if (needWhere)
			range->end = str->len;
	}

	/* Add the GROUP BY clause if given */
	if (query->groupClause != NULL)
	{
		bool needAgg;

		child = getSpecificInfo ((TransSubInfo *) info->root,
				SUBOP_Aggregation);

		if ((needAgg = (!inStatic && !equal(info->root, child))))
			MAKE_RANGE(range, child);

		appendContextKeyword(context, " GROUP BY ",
							 -PRETTYINDENT_STD, PRETTYINDENT_STD, 1);
		sep = "";
		//TODO
		foreach(l, query->groupClause)
		{
			GroupClause *grp = (GroupClause *) lfirst(l);

			appendStringInfoString(str, sep);
			get_rule_sortgroupclause(grp, query->targetList,
									 false, context);
			sep = ", ";
		}

		if (needAgg)
			range->end = str->len;
	}

	/* Add the HAVING clause if given */
	if (query->havingQual != NULL)
	{
		bool needHaving;

		child = getSpecificInfo ((TransSubInfo *) info->root, SUBOP_Having);

		if ((needHaving = (!inStatic && !equal(info->root, child))))
			MAKE_RANGE(range, child);

		appendContextKeyword(context, " HAVING ",
							 -PRETTYINDENT_STD, PRETTYINDENT_STD, 0);
		get_rule_expr(query->havingQual, context, false);

		if (needHaving)
			range->end = str->len;
	}
}



/* ----------
 * get_target_list			- Parse back a SELECT target list
 *
 * This is also used for RETURNING lists in INSERT/UPDATE/DELETE.
 * ----------
 */
static void
get_target_list(Query *query, TupleDesc resultDesc, TRANSPARSE_ARGS)
{
	StringInfo	str = context->buf;
	char	   *sep;
	int			colno;
	ListCell   *l;
	List *targetList = query->targetList;
	bool isDummy;
	bool underHaving = false;
	TransProjType type;
	TransSubInfo *curSub;
	TransParseRange *range = NULL;

	isDummy = inStatic || IsA(GET_TRANS_INFO(query)->root, TransProvInfo);

	if (!isDummy)
	{
		type = getProjectionType (query, &underHaving);

		curSub = (TransSubInfo *) GET_TRANS_INFO(query)->root;

		if (underHaving)
			curSub = TSET_LARG(curSub);

		switch(type)
		{
		case ProjUnderAgg:
		case ProjOverAgg:
			push(context->infoStack, TSET_LARG(curSub));
		/* fall through */
		case ProjBothAgg:
			push(context->infoStack, TSET_LARG(TSET_LARG(curSub)));
			break;
		default:
			break;
		}
	}

	if (!isDummy && underHaving)
		MAKE_RANGE(range, curSub);

	/* Add the DISTINCT clause if given */
	if (query->distinctClause != NIL)
	{
		if (has_distinct_on_clause(query))
		{
			appendStringInfo(str, " DISTINCT ON (");
			sep = "";
			foreach(l, query->distinctClause)
			{
				SortClause *srt = (SortClause *) lfirst(l);

				appendStringInfoString(str, sep);
				get_rule_sortgroupclause(srt, query->targetList,
										 false, context);
				sep = ", ";
			}
			appendStringInfo(str, ")");
		}
		else
			appendStringInfo(str, " DISTINCT");
	}

	sep = " ";
	colno = 0;
	foreach(l, targetList)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(l);
		char	   *colname;
		char	   *attname;

		if (tle->resjunk)
			continue;			/* ignore junk entries */

		appendStringInfoString(str, sep);
		sep = ", ";
		colno++;

		/*
		 * We special-case Var nodes rather than using get_rule_expr. This is
		 * needed because get_rule_expr will display a whole-row Var as
		 * "foo.*", which is the preferred notation in most contexts, but at
		 * the top level of a SELECT list it's not right (the parser will
		 * expand that notation into multiple columns, yielding behavior
		 * different from a whole-row Var).  We want just "foo", instead.
		 */
		if (tle->expr && IsA(tle->expr, Var))
		{
			attname = get_variable((Var *) tle->expr, 0, false, context);
		}
		else
		{
			get_rule_expr((Node *) tle->expr, context, true);
			/* We'll show the AS name unless it's this: */
			attname = "?column?";
		}

		/*
		 * Figure out what the result column should be called.	In the context
		 * of a view, use the view's tuple descriptor (so as to pick up the
		 * effects of any column RENAME that's been done on the view).
		 * Otherwise, just use what we can find in the TLE.
		 */
		if (resultDesc && colno <= resultDesc->natts)
			colname = NameStr(resultDesc->attrs[colno - 1]->attname);
		else
			colname = tle->resname;

		/* Show AS unless the column's name is correct as-is */
		if (colname)			/* resname could be NULL */
		{
			if (attname == NULL || strcmp(attname, colname) != 0)
				appendStringInfo(str, " AS %s", quote_identifier(colname));
		}
	}

	if (range)
		range->end = str->len;
}

/*
 *
 */

static void
get_setop_query(Node *setOp, Query *query, TupleDesc resultDesc,
		TRANSPARSE_ARGS, bool isRoot)
{
	StringInfo	str = context->buf;
	bool		need_paren;
	TransParseRange *range;
	Node *child;
	bool newStatic;

	if (parentInfo)
		newStatic = inStatic || TRANS_GET_STATIC(parentInfo);
	else
		newStatic = inStatic;

	if (!inStatic && !isRoot && !IsA(parentInfo, TransProvInfo))
		MAKE_RANGE(range, parentInfo);

	if (IsA(setOp, RangeTblRef))
	{
		RangeTblRef *rtr = (RangeTblRef *) setOp;
		RangeTblEntry *rte = rt_fetch(rtr->rtindex, query->rtable);
		Query	   *subquery = rte->subquery;

		Assert(subquery != NULL);
		Assert(subquery->setOperations == NULL);
		/* Need parens if ORDER BY, FOR UPDATE, or LIMIT; see gram.y */
		need_paren = (subquery->sortClause ||
					  subquery->rowMarks ||
					  subquery->limitOffset ||
					  subquery->limitCount);
		if (need_paren)
			appendStringInfoChar(str, '(');
		get_query_def(subquery, str, context->namespaces, resultDesc,
					  context->prettyFlags, context->indentLevel, context,
					  NULL, inStatic);
		if (need_paren)
			appendStringInfoChar(str, ')');
	}
	else if (IsA(setOp, SetOperationStmt))
	{
		SetOperationStmt *op = (SetOperationStmt *) setOp;

		/*
		 * We force parens whenever nesting two SetOperationStmts. There are
		 * some cases in which parens are needed around a leaf query too, but
		 * those are more easily handled at the next level down (see code
		 * above).
		 */
		need_paren = !IsA(op->larg, RangeTblRef);

		if (need_paren)
			appendStringInfoChar(str, '(');
		if (parentInfo)
			child = (Node *) TSET_LARG(parentInfo);
		get_setop_query(op->larg, query, resultDesc, context, child,
				newStatic, false);
		if (need_paren)
			appendStringInfoChar(str, ')');

		if (!PRETTY_INDENT(context))
			appendStringInfoChar(str, ' ');
		switch (op->op)
		{
			case SETOP_UNION:
				appendContextKeyword(context, " UNION ",
						-PRETTYINDENT_STD, PRETTYINDENT_STD, 0);
				break;
			case SETOP_INTERSECT:
				appendContextKeyword(context, " INTERSECT ",
						-PRETTYINDENT_STD, PRETTYINDENT_STD, 0);
				break;
			case SETOP_EXCEPT:
				appendContextKeyword(context, " EXCEPT ",
						-PRETTYINDENT_STD, PRETTYINDENT_STD, 0);
				break;
			default:
				elog(ERROR, "unrecognized set op: %d",
					 (int) op->op);
		}
		if (op->all)
			appendStringInfo(str, "ALL ");

		if (PRETTY_INDENT(context))
			appendContextKeyword(context, "", 0, 0, 0);

		need_paren = !IsA(op->rarg, RangeTblRef);

		if (op->op == SETOP_EXCEPT && newStatic)
			appendStringInfoString(str, "<NOT>");
		if (need_paren)
			appendStringInfoChar(str, '(');
		if (parentInfo)
			child = (Node *) TSET_RARG(parentInfo);
		get_setop_query(op->rarg, query, resultDesc, context, child, newStatic,
				false);
		if (need_paren)
			appendStringInfoChar(str, ')');
		if (op->op == SETOP_EXCEPT && newStatic)
			appendStringInfoString(str, "</NOT>");
	}
	else
	{
		elog(ERROR, "unrecognized node type: %d",
			 (int) nodeTag(setOp));
	}

	if (!inStatic && !isRoot && !IsA(parentInfo, TransProvInfo))
		range->end = str->len;
}

/*
 * Display a sort/group clause.
 *
 * Also returns the expression tree, so caller need not find it again.
 */
static Node *
get_rule_sortgroupclause(SortClause *srt, List *tlist, bool force_colno,
						 TransParseContext *context)
{
	StringInfo	str = context->buf;
	TargetEntry *tle;
	Node	   *expr;

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
		get_const_expr((Const *) expr, context, 1);
	else
		get_rule_expr(expr, context, true);

	return expr;
}



/*
 * push_plan: set up deparse_namespace to recurse into the tlist of a subplan
 *
 * When expanding an OUTER or INNER reference, we must push new outer/inner
 * subplans in case the referenced expression itself uses OUTER/INNER.	We
 * modify the top stack entry in-place to avoid affecting levelsup issues
 * (although in a Plan tree there really shouldn't be any).
 *
 * Caller must save and restore outer_plan and inner_plan around this.
 */
static void
push_plan(deparse_namespace *dpns, Plan *subplan)
{
	/*
	 * We special-case Append to pretend that the first child plan is the
	 * OUTER referent; otherwise normal.
	 */
	if (IsA(subplan, Append))
		dpns->outer_plan =
				(Plan *) linitial(((Append *) subplan)->appendplans);
	else
		dpns->outer_plan = outerPlan(subplan);

	/*
	 * For a SubqueryScan, pretend the subplan is INNER referent.  (We don't
	 * use OUTER because that could someday conflict with the normal meaning.)
	 */
	if (IsA(subplan, SubqueryScan))
		dpns->inner_plan = ((SubqueryScan *) subplan)->subplan;
	else
		dpns->inner_plan = innerPlan(subplan);
}


/*
 * Display a Var appropriately.
 *
 * In some cases (currently only when recursing into an unnamed join)
 * the Var's varlevelsup has to be interpreted with respect to a context
 * above the current one; levelsup indicates the offset.
 *
 * If showstar is TRUE, whole-row Vars are displayed as "foo.*";
 * if FALSE, merely as "foo".
 *
 * Returns the attname of the Var, or NULL if not determinable.
 */
static char *
get_variable(Var *var, int levelsup, bool showstar, TransParseContext *context)
{
	StringInfo	str = context->buf;
	RangeTblEntry *rte;
	AttrNumber	attnum;
	int			netlevelsup;
	deparse_namespace *dpns;
	char	   *schemaname;
	char	   *refname;
	char	   *attname;

	/* Find appropriate nesting depth */
	netlevelsup = var->varlevelsup + levelsup;
	if (netlevelsup >= list_length(context->namespaces))
		elog(ERROR, "bogus varlevelsup: %d offset %d",
			 var->varlevelsup, levelsup);
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
	else if (var->varno == OUTER && dpns->outer_plan)
	{
		TargetEntry *tle;
		Plan	   *save_outer;
		Plan	   *save_inner;

		tle = get_tle_by_resno(dpns->outer_plan->targetlist, var->varattno);
		if (!tle)
			elog(ERROR, "bogus varattno for OUTER var: %d", var->varattno);

		Assert(netlevelsup == 0);
		save_outer = dpns->outer_plan;
		save_inner = dpns->inner_plan;
		push_plan(dpns, dpns->outer_plan);

		/*
		 * Force parentheses because our caller probably assumed a Var is a
		 * simple expression.
		 */
		if (!IsA(tle->expr, Var))
			appendStringInfoChar(str, '(');
		get_rule_expr((Node *) tle->expr, context, true);
		if (!IsA(tle->expr, Var))
			appendStringInfoChar(str, ')');

		dpns->outer_plan = save_outer;
		dpns->inner_plan = save_inner;
		return NULL;
	}
	else if (var->varno == INNER && dpns->inner_plan)
	{
		TargetEntry *tle;
		Plan	   *save_outer;
		Plan	   *save_inner;

		tle = get_tle_by_resno(dpns->inner_plan->targetlist, var->varattno);
		if (!tle)
			elog(ERROR, "bogus varattno for INNER var: %d", var->varattno);

		Assert(netlevelsup == 0);
		save_outer = dpns->outer_plan;
		save_inner = dpns->inner_plan;
		push_plan(dpns, dpns->inner_plan);

		/*
		 * Force parentheses because our caller probably assumed a Var is a
		 * simple expression.
		 */
		if (!IsA(tle->expr, Var))
			appendStringInfoChar(str, '(');
		get_rule_expr((Node *) tle->expr, context, true);
		if (!IsA(tle->expr, Var))
			appendStringInfoChar(str, ')');

		dpns->outer_plan = save_outer;
		dpns->inner_plan = save_inner;
		return NULL;
	}
	else
	{
		elog(ERROR, "bogus varno: %d", var->varno);
		return NULL;			/* keep compiler quiet */
	}

	/* Identify names to use */
	schemaname = NULL;			/* default assumptions */
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
				schemaname = get_namespace_name(get_rel_namespace(rte->relid));
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
			 *
			 * This wouldn't work in decompiling plan trees, because we don't
			 * store joinaliasvars lists after planning; but a plan tree
			 * should never contain a join alias variable.
			 */
			if (rte->joinaliasvars == NIL)
				elog(ERROR, "cannot decompile join alias var in plan tree");
			if (attnum > 0)
			{
				Var		   *aliasvar;

				aliasvar = (Var *) list_nth(rte->joinaliasvars, attnum - 1);
				if (IsA(aliasvar, Var))
				{
					return get_variable(aliasvar, var->varlevelsup + levelsup,
										showstar, context);
				}
			}
			/* Unnamed join has neither schemaname nor refname */
			refname = NULL;
		}
	}

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

	return attname;
}


/*
 * Get the name of a field of an expression of composite type.
 *
 * This is fairly straightforward except for the case of a Var of type RECORD.
 * Since no actual table or view column is allowed to have type RECORD, such
 * a Var must refer to a JOIN or FUNCTION RTE or to a subquery output.	We
 * drill down to find the ultimate defining expression and attempt to infer
 * the field name from it.	We ereport if we can't determine the name.
 *
 * levelsup is an extra offset to interpret the Var's varlevelsup correctly.
 */
static const char *
get_name_for_var_field(Var *var, int fieldno,
					   int levelsup, TransParseContext *context)
{
	RangeTblEntry *rte;
	AttrNumber	attnum;
	int			netlevelsup;
	deparse_namespace *dpns;
	TupleDesc	tupleDesc;
	Node	   *expr;

	/*
	 * If it's a Var of type RECORD, we have to find what the Var refers to;
	 * if not, we can use get_expr_result_type. If that fails, we try
	 * lookup_rowtype_tupdesc, which will probably fail too, but will ereport
	 * an acceptable message.
	 */
	if (!IsA(var, Var) ||
		var->vartype != RECORDOID)
	{
		if (get_expr_result_type((Node *) var, NULL, &tupleDesc) != TYPEFUNC_COMPOSITE)
			tupleDesc = lookup_rowtype_tupdesc_copy(exprType((Node *) var),
													exprTypmod((Node *) var));
		Assert(tupleDesc);
		/* Got the tupdesc, so we can extract the field name */
		Assert(fieldno >= 1 && fieldno <= tupleDesc->natts);
		return NameStr(tupleDesc->attrs[fieldno - 1]->attname);
	}

	/* Find appropriate nesting depth */
	netlevelsup = var->varlevelsup + levelsup;
	if (netlevelsup >= list_length(context->namespaces))
		elog(ERROR, "bogus varlevelsup: %d offset %d",
			 var->varlevelsup, levelsup);
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
	else if (var->varno == OUTER && dpns->outer_plan)
	{
		TargetEntry *tle;
		Plan	   *save_outer;
		Plan	   *save_inner;
		const char *result;

		tle = get_tle_by_resno(dpns->outer_plan->targetlist, var->varattno);
		if (!tle)
			elog(ERROR, "bogus varattno for OUTER var: %d", var->varattno);

		Assert(netlevelsup == 0);
		save_outer = dpns->outer_plan;
		save_inner = dpns->inner_plan;
		push_plan(dpns, dpns->outer_plan);

		result = get_name_for_var_field((Var *) tle->expr, fieldno,
										levelsup, context);

		dpns->outer_plan = save_outer;
		dpns->inner_plan = save_inner;
		return result;
	}
	else if (var->varno == INNER && dpns->inner_plan)
	{
		TargetEntry *tle;
		Plan	   *save_outer;
		Plan	   *save_inner;
		const char *result;

		tle = get_tle_by_resno(dpns->inner_plan->targetlist, var->varattno);
		if (!tle)
			elog(ERROR, "bogus varattno for INNER var: %d", var->varattno);

		Assert(netlevelsup == 0);
		save_outer = dpns->outer_plan;
		save_inner = dpns->inner_plan;
		push_plan(dpns, dpns->inner_plan);

		result = get_name_for_var_field((Var *) tle->expr, fieldno,
										levelsup, context);

		dpns->outer_plan = save_outer;
		dpns->inner_plan = save_inner;
		return result;
	}
	else
	{
		elog(ERROR, "bogus varno: %d", var->varno);
		return NULL;			/* keep compiler quiet */
	}

	if (attnum == InvalidAttrNumber)
	{
		/* Var is whole-row reference to RTE, so select the right field */
		return get_rte_attribute_name(rte, fieldno);
	}

	/*
	 * This part has essentially the same logic as the parser's
	 * expandRecordVariable() function, but we are dealing with a different
	 * representation of the input context, and we only need one field name
	 * not a TupleDesc.  Also, we need a special case for deparsing Plan
	 * trees, because the subquery field has been removed from SUBQUERY RTEs.
	 */
	expr = (Node *) var;		/* default if we can't drill down */

	switch (rte->rtekind)
	{
		case RTE_RELATION:
		case RTE_SPECIAL:
		case RTE_VALUES:

			/*
			 * This case should not occur: a column of a table or values list
			 * shouldn't have type RECORD.  Fall through and fail (most
			 * likely) at the bottom.
			 */
			break;
		case RTE_SUBQUERY:
			{
				if (rte->subquery)
				{
					/* Subselect-in-FROM: examine sub-select's output expr */
					TargetEntry *ste = get_tle_by_resno(rte->subquery->targetList,
														attnum);

					if (ste == NULL || ste->resjunk)
						elog(ERROR, "subquery %s does not have attribute %d",
							 rte->eref->aliasname, attnum);
					expr = (Node *) ste->expr;
					if (IsA(expr, Var))
					{
						/*
						 * Recurse into the sub-select to see what its Var
						 * refers to. We have to build an additional level of
						 * namespace to keep in step with varlevelsup in the
						 * subselect.
						 */
						deparse_namespace mydpns;
						const char *result;

						mydpns.rtable = rte->subquery->rtable;
						mydpns.outer_plan = mydpns.inner_plan = NULL;

						context->namespaces = lcons(&mydpns,
													context->namespaces);

						result = get_name_for_var_field((Var *) expr, fieldno,
														0, context);

						context->namespaces =
							list_delete_first(context->namespaces);

						return result;
					}
					/* else fall through to inspect the expression */
				}
				else
				{
					/*
					 * We're deparsing a Plan tree so we don't have complete
					 * RTE entries.  But the only place we'd see a Var
					 * directly referencing a SUBQUERY RTE is in a
					 * SubqueryScan plan node, and we can look into the child
					 * plan's tlist instead.
					 */
					TargetEntry *tle;
					Plan	   *save_outer;
					Plan	   *save_inner;
					const char *result;

					if (!dpns->inner_plan)
						elog(ERROR, "failed to find plan for subquery %s",
							 rte->eref->aliasname);
					tle = get_tle_by_resno(dpns->inner_plan->targetlist,
										   attnum);
					if (!tle)
						elog(ERROR, "bogus varattno for subquery var: %d",
							 attnum);
					Assert(netlevelsup == 0);
					save_outer = dpns->outer_plan;
					save_inner = dpns->inner_plan;
					push_plan(dpns, dpns->inner_plan);

					result = get_name_for_var_field((Var *) tle->expr, fieldno,
													levelsup, context);

					dpns->outer_plan = save_outer;
					dpns->inner_plan = save_inner;
					return result;
				}
			}
			break;
		case RTE_JOIN:
			/* Join RTE --- recursively inspect the alias variable */
			if (rte->joinaliasvars == NIL)
				elog(ERROR, "cannot decompile join alias var in plan tree");
			Assert(attnum > 0 && attnum <= list_length(rte->joinaliasvars));
			expr = (Node *) list_nth(rte->joinaliasvars, attnum - 1);
			if (IsA(expr, Var))
				return get_name_for_var_field((Var *) expr, fieldno,
											  var->varlevelsup + levelsup,
											  context);
			/* else fall through to inspect the expression */
			break;
		case RTE_FUNCTION:

			/*
			 * We couldn't get here unless a function is declared with one of
			 * its result columns as RECORD, which is not allowed.
			 */
			break;
	}

	/*
	 * We now have an expression we can't expand any more, so see if
	 * get_expr_result_type() can do anything with it.	If not, pass to
	 * lookup_rowtype_tupdesc() which will probably fail, but will give an
	 * appropriate error message while failing.
	 */
	if (get_expr_result_type(expr, NULL, &tupleDesc) != TYPEFUNC_COMPOSITE)
		tupleDesc = lookup_rowtype_tupdesc_copy(exprType(expr),
												exprTypmod(expr));
	Assert(tupleDesc);
	/* Got the tupdesc, so we can extract the field name */
	Assert(fieldno >= 1 && fieldno <= tupleDesc->natts);
	return NameStr(tupleDesc->attrs[fieldno - 1]->attname);
}





/*
 * appendContextKeyword - append a keyword to buffer
 *
 * If prettyPrint is enabled, perform a line break, and adjust indentation.
 * Otherwise, just append the keyword.
 */
static void
appendContextKeyword(TransParseContext *context, const char *str,
					 int indentBefore, int indentAfter, int indentPlus)
{
	if (PRETTY_INDENT(context))
	{
		context->indentLevel += indentBefore;

//		appendStringInfoChar(context->buf, '\n');
//		appendStringInfoSpaces(context->buf,
//							   Max(context->indentLevel, 0) + indentPlus);
		appendStringInfoString(context->buf, str);

		context->indentLevel += indentAfter;
		if (context->indentLevel < 0)
			context->indentLevel = 0;
	}
	else
		appendStringInfoString(context->buf, str);
}

/*
 * get_rule_expr_paren	- deparse expr using get_rule_expr,
 * embracing the string with parentheses if necessary for prettyPrint.
 *
 * Never embrace if prettyFlags=0, because it's done in the calling node.
 *
 * Any node that does *not* embrace its argument node by sql syntax (with
 * parentheses, non-operator keywords like CASE/WHEN/ON, or comma etc) should
 * use get_rule_expr_paren instead of get_rule_expr so parentheses can be
 * added.
 */
static void
get_rule_expr_paren(Node *node, TransParseContext *context,
					bool showimplicit, Node *parentNode)
{
	bool		need_paren;

	need_paren = PRETTY_PAREN(context) &&
		!isSimpleNode(node, parentNode, context->prettyFlags);

	if (need_paren)
		appendStringInfoChar(context->buf, '(');

	get_rule_expr(node, context, showimplicit);

	if (need_paren)
		appendStringInfoChar(context->buf, ')');
}


/* ----------
 * get_rule_expr			- Parse back an expression
 *
 * Note: showimplicit determines whether we display any implicit cast that
 * is present at the top of the expression tree.  It is a passed argument,
 * not a field of the context struct, because we change the value as we
 * recurse down into the expression.  In general we suppress implicit casts
 * when the result type is known with certainty (eg, the arguments of an
 * OR must be boolean).  We display implicit casts for arguments of functions
 * and operators, since this is needed to be certain that the same function
 * or operator will be chosen when the expression is re-parsed.
 * ----------
 */
static void
get_rule_expr(Node *node, TransParseContext *context,
			  bool showimplicit)
{
	StringInfo	str = context->buf;

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
			(void) get_variable((Var *) node, 0, true, context);
			break;

		case T_Const:
			get_const_expr((Const *) node, context, 0);
			break;

		case T_Param:
			appendStringInfo(str, "$%d", ((Param *) node)->paramid);
			break;

		case T_Aggref:
			get_agg_expr((Aggref *) node, context);
			break;

		case T_ArrayRef:
			{
				ArrayRef   *aref = (ArrayRef *) node;
				bool		need_parens;

				/*
				 * Parenthesize the argument unless it's a simple Var or a
				 * FieldSelect.  (In particular, if it's another ArrayRef, we
				 * *must* parenthesize to avoid confusion.)
				 */
				need_parens = !IsA(aref->refexpr, Var) &&
					!IsA(aref->refexpr, FieldSelect);
				if (need_parens)
					appendStringInfoChar(str, '(');
				get_rule_expr((Node *) aref->refexpr, context, showimplicit);
				if (need_parens)
					appendStringInfoChar(str, ')');
				printSubscripts(aref, context);

				/*
				 * Array assignment nodes should have been handled in
				 * processIndirection().
				 */
				if (aref->refassgnexpr)
					elog(ERROR, "unexpected refassgnexpr");
			}
			break;

		case T_FuncExpr:
			get_func_expr((FuncExpr *) node, context, showimplicit);
			break;

		case T_OpExpr:
			get_oper_expr((OpExpr *) node, context);
			break;

		case T_DistinctExpr:
			{
				DistinctExpr *expr = (DistinctExpr *) node;
				List	   *args = expr->args;
				Node	   *arg1 = (Node *) linitial(args);
				Node	   *arg2 = (Node *) lsecond(args);

				if (!PRETTY_PAREN(context))
					appendStringInfoChar(str, '(');
				get_rule_expr_paren(arg1, context, true, node);
				appendStringInfo(str, " IS DISTINCT FROM ");
				get_rule_expr_paren(arg2, context, true, node);
				if (!PRETTY_PAREN(context))
					appendStringInfoChar(str, ')');
			}
			break;

		case T_ScalarArrayOpExpr:
			{
				ScalarArrayOpExpr *expr = (ScalarArrayOpExpr *) node;
				List	   *args = expr->args;
				Node	   *arg1 = (Node *) linitial(args);
				Node	   *arg2 = (Node *) lsecond(args);

				if (!PRETTY_PAREN(context))
					appendStringInfoChar(str, '(');
				get_rule_expr_paren(arg1, context, true, node);
				appendStringInfo(str, " %s %s (",
								 generate_operator_name(expr->opno,
														exprType(arg1),
										   get_element_type(exprType(arg2))),
								 expr->useOr ? "ANY" : "ALL");
				get_rule_expr_paren(arg2, context, true, node);
				appendStringInfoChar(str, ')');
				if (!PRETTY_PAREN(context))
					appendStringInfoChar(str, ')');
			}
			break;

		case T_BoolExpr:
			{
				BoolExpr   *expr = (BoolExpr *) node;
				Node	   *first_arg = linitial(expr->args);
				ListCell   *arg = lnext(list_head(expr->args));

				switch (expr->boolop)
				{
					case AND_EXPR:
						if (!PRETTY_PAREN(context))
							appendStringInfoChar(str, '(');
						get_rule_expr_paren(first_arg, context,
											false, node);
						while (arg)
						{
							appendStringInfo(str, " AND ");
							get_rule_expr_paren((Node *) lfirst(arg), context,
												false, node);
							arg = lnext(arg);
						}
						if (!PRETTY_PAREN(context))
							appendStringInfoChar(str, ')');
						break;

					case OR_EXPR:
						if (!PRETTY_PAREN(context))
							appendStringInfoChar(str, '(');
						get_rule_expr_paren(first_arg, context,
											false, node);
						while (arg)
						{
							appendStringInfo(str, " OR ");
							get_rule_expr_paren((Node *) lfirst(arg), context,
												false, node);
							arg = lnext(arg);
						}
						if (!PRETTY_PAREN(context))
							appendStringInfoChar(str, ')');
						break;

					case NOT_EXPR:
						if (!PRETTY_PAREN(context))
							appendStringInfoChar(str, '(');
						appendStringInfo(str, "NOT ");
						get_rule_expr_paren(first_arg, context,
											false, node);
						if (!PRETTY_PAREN(context))
							appendStringInfoChar(str, ')');
						break;

					default:
						elog(ERROR, "unrecognized boolop: %d",
							 (int) expr->boolop);
				}
			}
			break;

		case T_SubLink:
			get_sublink_expr((SubLink *) node, context, NULL, false);
			break;

		case T_SubPlan:
			{
				/*
				 * We cannot see an already-planned subplan in rule deparsing,
				 * only while EXPLAINing a query plan. For now, just punt.
				 */
				if (((SubPlan *) node)->useHashTable)
					appendStringInfo(str, "(hashed subplan)");
				else
					appendStringInfo(str, "(subplan)");
			}
			break;

		case T_FieldSelect:
			{
				FieldSelect *fselect = (FieldSelect *) node;
				Node	   *arg = (Node *) fselect->arg;
				int			fno = fselect->fieldnum;
				const char *fieldname;
				bool		need_parens;

				/*
				 * Parenthesize the argument unless it's an ArrayRef or
				 * another FieldSelect.  Note in particular that it would be
				 * WRONG to not parenthesize a Var argument; simplicity is not
				 * the issue here, having the right number of names is.
				 */
				need_parens = !IsA(arg, ArrayRef) &&!IsA(arg, FieldSelect);
				if (need_parens)
					appendStringInfoChar(str, '(');
				get_rule_expr(arg, context, true);
				if (need_parens)
					appendStringInfoChar(str, ')');

				/*
				 * Get and print the field name.
				 */
				fieldname = get_name_for_var_field((Var *) arg, fno,
												   0, context);
				appendStringInfo(str, ".%s", quote_identifier(fieldname));
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
				Node	   *arg = (Node *) relabel->arg;

				if (relabel->relabelformat == COERCE_IMPLICIT_CAST &&
					!showimplicit)
				{
					/* don't show the implicit cast */
					get_rule_expr_paren(arg, context, false, node);
				}
				else
				{
					get_coercion_expr(arg, context,
									  relabel->resulttype,
									  relabel->resulttypmod,
									  node);
				}
			}
			break;

		case T_CoerceViaIO:
			{
				CoerceViaIO *iocoerce = (CoerceViaIO *) node;
				Node	   *arg = (Node *) iocoerce->arg;

				if (iocoerce->coerceformat == COERCE_IMPLICIT_CAST &&
					!showimplicit)
				{
					/* don't show the implicit cast */
					get_rule_expr_paren(arg, context, false, node);
				}
				else
				{
					get_coercion_expr(arg, context,
									  iocoerce->resulttype,
									  -1,
									  node);
				}
			}
			break;

		case T_ArrayCoerceExpr:
			{
				ArrayCoerceExpr *acoerce = (ArrayCoerceExpr *) node;
				Node	   *arg = (Node *) acoerce->arg;

				if (acoerce->coerceformat == COERCE_IMPLICIT_CAST &&
					!showimplicit)
				{
					/* don't show the implicit cast */
					get_rule_expr_paren(arg, context, false, node);
				}
				else
				{
					get_coercion_expr(arg, context,
									  acoerce->resulttype,
									  acoerce->resulttypmod,
									  node);
				}
			}
			break;

		case T_ConvertRowtypeExpr:
			{
				ConvertRowtypeExpr *convert = (ConvertRowtypeExpr *) node;
				Node	   *arg = (Node *) convert->arg;

				if (convert->convertformat == COERCE_IMPLICIT_CAST &&
					!showimplicit)
				{
					/* don't show the implicit cast */
					get_rule_expr_paren(arg, context, false, node);
				}
				else
				{
					get_coercion_expr(arg, context,
									  convert->resulttype, -1,
									  node);
				}
			}
			break;

		case T_CaseExpr:
			{
				CaseExpr   *caseexpr = (CaseExpr *) node;
				ListCell   *temp;

				appendContextKeyword(context, "CASE",
									 0, PRETTYINDENT_VAR, 0);
				if (caseexpr->arg)
				{
					appendStringInfoChar(str, ' ');
					get_rule_expr((Node *) caseexpr->arg, context, true);
				}
				foreach(temp, caseexpr->args)
				{
					CaseWhen   *when = (CaseWhen *) lfirst(temp);
					Node	   *w = (Node *) when->expr;

					if (!PRETTY_INDENT(context))
						appendStringInfoChar(str, ' ');
					appendContextKeyword(context, "WHEN ",
										 0, 0, 0);
					if (caseexpr->arg)
					{
						/*
						 * The parser should have produced WHEN clauses of the
						 * form "CaseTestExpr = RHS"; we want to show just the
						 * RHS.  If the user wrote something silly like "CASE
						 * boolexpr WHEN TRUE THEN ...", then the optimizer's
						 * simplify_boolean_equality() may have reduced this
						 * to just "CaseTestExpr" or "NOT CaseTestExpr", for
						 * which we have to show "TRUE" or "FALSE".  Also,
						 * depending on context the original CaseTestExpr
						 * might have been reduced to a Const (but we won't
						 * see "WHEN Const").
						 */
						if (IsA(w, OpExpr))
						{
							Node	   *rhs;

							Assert(IsA(linitial(((OpExpr *) w)->args),
									   CaseTestExpr) ||
								   IsA(linitial(((OpExpr *) w)->args),
									   Const));
							rhs = (Node *) lsecond(((OpExpr *) w)->args);
							get_rule_expr(rhs, context, false);
						}
						else if (IsA(w, CaseTestExpr))
							appendStringInfo(str, "TRUE");
						else if (not_clause(w))
						{
							Assert(IsA(get_notclausearg((Expr *) w),
									   CaseTestExpr));
							appendStringInfo(str, "FALSE");
						}
						else
							elog(ERROR, "unexpected CASE WHEN clause: %d",
								 (int) nodeTag(w));
					}
					else
						get_rule_expr(w, context, false);
					appendStringInfo(str, " THEN ");
					get_rule_expr((Node *) when->result, context, true);
				}
				if (!PRETTY_INDENT(context))
					appendStringInfoChar(str, ' ');
				appendContextKeyword(context, "ELSE ",
									 0, 0, 0);
				get_rule_expr((Node *) caseexpr->defresult, context, true);
				if (!PRETTY_INDENT(context))
					appendStringInfoChar(str, ' ');
				appendContextKeyword(context, "END",
									 -PRETTYINDENT_VAR, 0, 0);
			}
			break;

		case T_ArrayExpr:
			{
				ArrayExpr  *arrayexpr = (ArrayExpr *) node;

				appendStringInfo(str, "ARRAY[");
				get_rule_expr((Node *) arrayexpr->elements, context, true);
				appendStringInfoChar(str, ']');
			}
			break;

		case T_RowExpr:
			{
				RowExpr    *rowexpr = (RowExpr *) node;
				TupleDesc	tupdesc = NULL;
				ListCell   *arg;
				int			i;
				char	   *sep;

				/*
				 * If it's a named type and not RECORD, we may have to skip
				 * dropped columns and/or claim there are NULLs for added
				 * columns.
				 */
				if (rowexpr->row_typeid != RECORDOID)
				{
					tupdesc = lookup_rowtype_tupdesc(rowexpr->row_typeid, -1);
					Assert(list_length(rowexpr->args) <= tupdesc->natts);
				}

				/*
				 * SQL99 allows "ROW" to be omitted when there is more than
				 * one column, but for simplicity we always print it.
				 */
				appendStringInfo(str, "ROW(");
				sep = "";
				i = 0;
				foreach(arg, rowexpr->args)
				{
					Node	   *e = (Node *) lfirst(arg);

					if (tupdesc == NULL ||
						!tupdesc->attrs[i]->attisdropped)
					{
						appendStringInfoString(str, sep);
						get_rule_expr(e, context, true);
						sep = ", ";
					}
					i++;
				}
				if (tupdesc != NULL)
				{
					while (i < tupdesc->natts)
					{
						if (!tupdesc->attrs[i]->attisdropped)
						{
							appendStringInfoString(str, sep);
							appendStringInfo(str, "NULL");
							sep = ", ";
						}
						i++;
					}

					ReleaseTupleDesc(tupdesc);
				}
				appendStringInfo(str, ")");
				if (rowexpr->row_format == COERCE_EXPLICIT_CAST)
					appendStringInfo(str, "::%s",
						  format_type_with_typemod(rowexpr->row_typeid, -1));
			}
			break;

		case T_RowCompareExpr:
			{
				RowCompareExpr *rcexpr = (RowCompareExpr *) node;
				ListCell   *arg;
				char	   *sep;

				/*
				 * SQL99 allows "ROW" to be omitted when there is more than
				 * one column, but for simplicity we always print it.
				 */
				appendStringInfo(str, "(ROW(");
				sep = "";
				foreach(arg, rcexpr->largs)
				{
					Node	   *e = (Node *) lfirst(arg);

					appendStringInfoString(str, sep);
					get_rule_expr(e, context, true);
					sep = ", ";
				}

				/*
				 * We assume that the name of the first-column operator will
				 * do for all the rest too.  This is definitely open to
				 * failure, eg if some but not all operators were renamed
				 * since the construct was parsed, but there seems no way to
				 * be perfect.
				 */
				appendStringInfo(str, ") %s ROW(",
						  generate_operator_name(linitial_oid(rcexpr->opnos),
										   exprType(linitial(rcexpr->largs)),
										 exprType(linitial(rcexpr->rargs))));
				sep = "";
				foreach(arg, rcexpr->rargs)
				{
					Node	   *e = (Node *) lfirst(arg);

					appendStringInfoString(str, sep);
					get_rule_expr(e, context, true);
					sep = ", ";
				}
				appendStringInfo(str, "))");
			}
			break;

		case T_CoalesceExpr:
			{
				CoalesceExpr *coalesceexpr = (CoalesceExpr *) node;

				appendStringInfo(str, "COALESCE(");
				get_rule_expr((Node *) coalesceexpr->args, context, true);
				appendStringInfoChar(str, ')');
			}
			break;

		case T_MinMaxExpr:
			{
				MinMaxExpr *minmaxexpr = (MinMaxExpr *) node;

				switch (minmaxexpr->op)
				{
					case IS_GREATEST:
						appendStringInfo(str, "GREATEST(");
						break;
					case IS_LEAST:
						appendStringInfo(str, "LEAST(");
						break;
				}
				get_rule_expr((Node *) minmaxexpr->args, context, true);
				appendStringInfoChar(str, ')');
			}
			break;

		case T_XmlExpr:
			{
				XmlExpr    *xexpr = (XmlExpr *) node;
				bool		needcomma = false;
				ListCell   *arg;
				ListCell   *narg;
				Const	   *con;

				switch (xexpr->op)
				{
					case IS_XMLCONCAT:
						appendStringInfoString(str, "XMLCONCAT(");
						break;
					case IS_XMLELEMENT:
						appendStringInfoString(str, "XMLELEMENT(");
						break;
					case IS_XMLFOREST:
						appendStringInfoString(str, "XMLFOREST(");
						break;
					case IS_XMLPARSE:
						appendStringInfoString(str, "XMLPARSE(");
						break;
					case IS_XMLPI:
						appendStringInfoString(str, "XMLPI(");
						break;
					case IS_XMLROOT:
						appendStringInfoString(str, "XMLROOT(");
						break;
					case IS_XMLSERIALIZE:
						appendStringInfoString(str, "XMLSERIALIZE(");
						break;
					case IS_DOCUMENT:
						break;
				}
				if (xexpr->op == IS_XMLPARSE || xexpr->op == IS_XMLSERIALIZE)
				{
					if (xexpr->xmloption == XMLOPTION_DOCUMENT)
						appendStringInfoString(str, "DOCUMENT ");
					else
						appendStringInfoString(str, "CONTENT ");
				}
				if (xexpr->name)
				{
					appendStringInfo(str, "NAME %s",
									 quote_identifier(map_xml_name_to_sql_identifier(xexpr->name)));
					needcomma = true;
				}
				if (xexpr->named_args)
				{
					if (xexpr->op != IS_XMLFOREST)
					{
						if (needcomma)
							appendStringInfoString(str, ", ");
						appendStringInfoString(str, "XMLATTRIBUTES(");
						needcomma = false;
					}
					forboth(arg, xexpr->named_args, narg, xexpr->arg_names)
					{
						Node	   *e = (Node *) lfirst(arg);
						char	   *argname = strVal(lfirst(narg));

						if (needcomma)
							appendStringInfoString(str, ", ");
						get_rule_expr((Node *) e, context, true);
						appendStringInfo(str, " AS %s",
										 quote_identifier(map_xml_name_to_sql_identifier(argname)));
						needcomma = true;
					}
					if (xexpr->op != IS_XMLFOREST)
						appendStringInfoChar(str, ')');
				}
				if (xexpr->args)
				{
					if (needcomma)
						appendStringInfoString(str, ", ");
					switch (xexpr->op)
					{
						case IS_XMLCONCAT:
						case IS_XMLELEMENT:
						case IS_XMLFOREST:
						case IS_XMLPI:
						case IS_XMLSERIALIZE:
							/* no extra decoration needed */
							get_rule_expr((Node *) xexpr->args, context, true);
							break;
						case IS_XMLPARSE:
							Assert(list_length(xexpr->args) == 2);

							get_rule_expr((Node *) linitial(xexpr->args),
										  context, true);

							con = (Const *) lsecond(xexpr->args);
							Assert(IsA(con, Const));
							Assert(!con->constisnull);
							if (DatumGetBool(con->constvalue))
								appendStringInfoString(str,
													 " PRESERVE WHITESPACE");
							else
								appendStringInfoString(str,
													   " STRIP WHITESPACE");
							break;
						case IS_XMLROOT:
							Assert(list_length(xexpr->args) == 3);

							get_rule_expr((Node *) linitial(xexpr->args),
										  context, true);

							appendStringInfoString(str, ", VERSION ");
							con = (Const *) lsecond(xexpr->args);
							if (IsA(con, Const) &&
								con->constisnull)
								appendStringInfoString(str, "NO VALUE");
							else
								get_rule_expr((Node *) con, context, false);

							con = (Const *) lthird(xexpr->args);
							Assert(IsA(con, Const));
							if (con->constisnull)
								 /* suppress STANDALONE NO VALUE */ ;
							else
							{
								switch (DatumGetInt32(con->constvalue))
								{
									case XML_STANDALONE_YES:
										appendStringInfoString(str,
														 ", STANDALONE YES");
										break;
									case XML_STANDALONE_NO:
										appendStringInfoString(str,
														  ", STANDALONE NO");
										break;
									case XML_STANDALONE_NO_VALUE:
										appendStringInfoString(str,
													", STANDALONE NO VALUE");
										break;
									default:
										break;
								}
							}
							break;
						case IS_DOCUMENT:
							get_rule_expr_paren((Node *) xexpr->args, context, false, node);
							break;
					}

				}
				if (xexpr->op == IS_XMLSERIALIZE)
					appendStringInfo(str, " AS %s", format_type_with_typemod(xexpr->type,
															 xexpr->typmod));
				if (xexpr->op == IS_DOCUMENT)
					appendStringInfoString(str, " IS DOCUMENT");
				else
					appendStringInfoChar(str, ')');
			}
			break;

		case T_NullIfExpr:
			{
				NullIfExpr *nullifexpr = (NullIfExpr *) node;

				appendStringInfo(str, "NULLIF(");
				get_rule_expr((Node *) nullifexpr->args, context, true);
				appendStringInfoChar(str, ')');
			}
			break;

		case T_NullTest:
			{
				NullTest   *ntest = (NullTest *) node;

				if (!PRETTY_PAREN(context))
					appendStringInfoChar(str, '(');
				get_rule_expr_paren((Node *) ntest->arg, context, true, node);
				switch (ntest->nulltesttype)
				{
					case IS_NULL:
						appendStringInfo(str, " IS NULL");
						break;
					case IS_NOT_NULL:
						appendStringInfo(str, " IS NOT NULL");
						break;
					default:
						elog(ERROR, "unrecognized nulltesttype: %d",
							 (int) ntest->nulltesttype);
				}
				if (!PRETTY_PAREN(context))
					appendStringInfoChar(str, ')');
			}
			break;

		case T_BooleanTest:
			{
				BooleanTest *btest = (BooleanTest *) node;

				if (!PRETTY_PAREN(context))
					appendStringInfoChar(str, '(');
				get_rule_expr_paren((Node *) btest->arg, context, false, node);
				switch (btest->booltesttype)
				{
					case IS_TRUE:
						appendStringInfo(str, " IS TRUE");
						break;
					case IS_NOT_TRUE:
						appendStringInfo(str, " IS NOT TRUE");
						break;
					case IS_FALSE:
						appendStringInfo(str, " IS FALSE");
						break;
					case IS_NOT_FALSE:
						appendStringInfo(str, " IS NOT FALSE");
						break;
					case IS_UNKNOWN:
						appendStringInfo(str, " IS UNKNOWN");
						break;
					case IS_NOT_UNKNOWN:
						appendStringInfo(str, " IS NOT UNKNOWN");
						break;
					default:
						elog(ERROR, "unrecognized booltesttype: %d",
							 (int) btest->booltesttype);
				}
				if (!PRETTY_PAREN(context))
					appendStringInfoChar(str, ')');
			}
			break;

		case T_CoerceToDomain:
			{
				CoerceToDomain *ctest = (CoerceToDomain *) node;
				Node	   *arg = (Node *) ctest->arg;

				if (ctest->coercionformat == COERCE_IMPLICIT_CAST &&
					!showimplicit)
				{
					/* don't show the implicit cast */
					get_rule_expr(arg, context, false);
				}
				else
				{
					get_coercion_expr(arg, context,
									  ctest->resulttype,
									  ctest->resulttypmod,
									  node);
				}
			}
			break;

		case T_CoerceToDomainValue:
			appendStringInfo(str, "VALUE");
			break;

		case T_SetToDefault:
			appendStringInfo(str, "DEFAULT");
			break;

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
				char	   *sep;
				ListCell   *l;

				sep = "";
				foreach(l, (List *) node)
				{
					appendStringInfoString(str, sep);
					get_rule_expr((Node *) lfirst(l), context, showimplicit);
					sep = ", ";
				}
			}
			break;

		default:
			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(node));
			break;
	}
}


/*
 * get_oper_expr			- Parse back an OpExpr node
 */
static void
get_oper_expr(OpExpr *expr, TransParseContext *context)
{
	StringInfo	str = context->buf;
	Oid			opno = expr->opno;
	List	   *args = expr->args;

	if (!PRETTY_PAREN(context))
		appendStringInfoChar(str, '(');
	if (list_length(args) == 2)
	{
		/* binary operator */
		Node	   *arg1 = (Node *) linitial(args);
		Node	   *arg2 = (Node *) lsecond(args);

		get_rule_expr_paren(arg1, context, true, (Node *) expr);
		appendStringInfo(str, " %s ",
						 generate_operator_name(opno,
												exprType(arg1),
												exprType(arg2)));
		get_rule_expr_paren(arg2, context, true, (Node *) expr);
	}
	else
	{
		/* unary operator --- but which side? */
		Node	   *arg = (Node *) linitial(args);
		HeapTuple	tp;
		Form_pg_operator optup;

		tp = SearchSysCache(OPEROID,
							ObjectIdGetDatum(opno),
							0, 0, 0);
		if (!HeapTupleIsValid(tp))
			elog(ERROR, "cache lookup failed for operator %u", opno);
		optup = (Form_pg_operator) GETSTRUCT(tp);
		switch (optup->oprkind)
		{
			case 'l':
				appendStringInfo(str, "%s ",
								 generate_operator_name(opno,
														InvalidOid,
														exprType(arg)));
				get_rule_expr_paren(arg, context, true, (Node *) expr);
				break;
			case 'r':
				get_rule_expr_paren(arg, context, true, (Node *) expr);
				appendStringInfo(str, " %s",
								 generate_operator_name(opno,
														exprType(arg),
														InvalidOid));
				break;
			default:
				elog(ERROR, "bogus oprkind: %d", optup->oprkind);
		}
		ReleaseSysCache(tp);
	}
	if (!PRETTY_PAREN(context))
		appendStringInfoChar(str, ')');
}

/*
 * get_func_expr			- Parse back a FuncExpr node
 */
static void
get_func_expr(FuncExpr *expr, TransParseContext *context,
			  bool showimplicit)
{
	StringInfo	str = context->buf;
	Oid			funcoid = expr->funcid;
	Oid			argtypes[FUNC_MAX_ARGS];
	int			nargs;
	ListCell   *l;

	/*
	 * If the function call came from an implicit coercion, then just show the
	 * first argument --- unless caller wants to see implicit coercions.
	 */
	if (expr->funcformat == COERCE_IMPLICIT_CAST && !showimplicit)
	{
		get_rule_expr_paren((Node *) linitial(expr->args), context,
							false, (Node *) expr);
		return;
	}

	/*
	 * If the function call came from a cast, then show the first argument
	 * plus an explicit cast operation.
	 */
	if (expr->funcformat == COERCE_EXPLICIT_CAST ||
		expr->funcformat == COERCE_IMPLICIT_CAST)
	{
		Node	   *arg = linitial(expr->args);
		Oid			rettype = expr->funcresulttype;
		int32		coercedTypmod;

		/* Get the typmod if this is a length-coercion function */
		(void) exprIsLengthCoercion((Node *) expr, &coercedTypmod);

		get_coercion_expr(arg, context,
						  rettype, coercedTypmod,
						  (Node *) expr);

		return;
	}

	/*
	 * Normal function: display as proname(args).  First we need to extract
	 * the argument datatypes.
	 */
	nargs = 0;
	foreach(l, expr->args)
	{
		if (nargs >= FUNC_MAX_ARGS)
			ereport(ERROR,
					(errcode(ERRCODE_TOO_MANY_ARGUMENTS),
					 errmsg("too many arguments")));
		argtypes[nargs] = exprType((Node *) lfirst(l));
		nargs++;
	}

	appendStringInfo(str, "%s(",
					 generate_function_name(funcoid, nargs, argtypes));
	get_rule_expr((Node *) expr->args, context, true);
	appendStringInfoChar(str, ')');
}

/*
 * get_agg_expr			- Parse back an Aggref node
 */
static void
get_agg_expr(Aggref *aggref, TransParseContext *context)
{
	StringInfo	str = context->buf;
	Oid			argtypes[FUNC_MAX_ARGS];
	int			nargs;
	ListCell   *l;
	TransSubInfo *aggInfo;
	TransSubInfo *projInfo;
	TransParseRange *range;
	TransParseRange *projRange;

	aggInfo = NULL;
	projInfo = NULL;
	if (context->curProjType == ProjOverAgg)
		aggInfo = (TransSubInfo *) list_nth(*context->infoStack, 0);
	else if (context->curProjType == ProjUnderAgg)
		projInfo = (TransSubInfo *) list_nth(*context->infoStack, 0);
	else if (context->curProjType == ProjBothAgg)
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
	foreach(l, aggref->args)
	{
		if (nargs >= FUNC_MAX_ARGS)
			ereport(ERROR,
					(errcode(ERRCODE_TOO_MANY_ARGUMENTS),
					 errmsg("too many arguments")));
		argtypes[nargs] = exprType((Node *) lfirst(l));
		nargs++;
	}

	appendStringInfo(str, "%s(%s",
				   generate_function_name(aggref->aggfnoid, nargs, argtypes),
					 aggref->aggdistinct ? "DISTINCT " : "");

	if (projInfo)
		MAKE_RANGE(projRange, projInfo);

	/* aggstar can be set only in zero-argument aggregates */
	if (aggref->aggstar)
		appendStringInfoChar(str, '*');
	else
		get_rule_expr((Node *) aggref->args, context, true);

	if (projInfo)
		projRange->end = str->len;

	appendStringInfoChar(str, ')');

	if (aggInfo)
		range->end = str->len;
}

/* ----------
 * get_coercion_expr
 *
 *	Make a string representation of a value coerced to a specific type
 * ----------
 */
static void
get_coercion_expr(Node *arg, TransParseContext *context,
				  Oid resulttype, int32 resulttypmod,
				  Node *parentNode)
{
	StringInfo	str = context->buf;

	/*
	 * Since parse_coerce.c doesn't immediately collapse application of
	 * length-coercion functions to constants, what we'll typically see in
	 * such cases is a Const with typmod -1 and a length-coercion function
	 * right above it.	Avoid generating redundant output. However, beware of
	 * suppressing casts when the user actually wrote something like
	 * 'foo'::text::char(3).
	 */
	if (arg && IsA(arg, Const) &&
		((Const *) arg)->consttype == resulttype &&
		((Const *) arg)->consttypmod == -1)
	{
		/* Show the constant without normal ::typename decoration */
		get_const_expr((Const *) arg, context, -1);
	}
	else
	{
		if (!PRETTY_PAREN(context))
			appendStringInfoChar(str, '(');
		get_rule_expr_paren(arg, context, false, parentNode);
		if (!PRETTY_PAREN(context))
			appendStringInfoChar(str, ')');
	}
	appendStringInfo(str, "::%s",
					 format_type_with_typemod(resulttype, resulttypmod));
}

/* ----------
 * get_const_expr
 *
 *	Make a string representation of a Const
 *
 * showtype can be -1 to never show "::typename" decoration, or +1 to always
 * show it, or 0 to show it only if the constant wouldn't be assumed to be
 * the right type by default.
 * ----------
 */
static void
get_const_expr(Const *constval, TransParseContext *context, int showtype)
{
	StringInfo	str = context->buf;
	Oid			typoutput;
	bool		typIsVarlena;
	char	   *extval;
	char	   *valptr;
	bool		isfloat = false;
	bool		needlabel;

	if (constval->constisnull)
	{
		/*
		 * Always label the type of a NULL constant to prevent misdecisions
		 * about type when reparsing.
		 */
		appendStringInfo(str, "NULL");
		if (showtype >= 0)
			appendStringInfo(str, "::%s",
							 format_type_with_typemod(constval->consttype,
													  constval->consttypmod));
		return;
	}

	getTypeOutputInfo(constval->consttype,
					  &typoutput, &typIsVarlena);

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

			/*
			 * We form the string literal according to the prevailing setting
			 * of standard_conforming_strings; we never use E''. User is
			 * responsible for making sure result is used correctly.
			 */
			appendStringInfoChar(str, '\'');
			for (valptr = extval; *valptr; valptr++)
			{
				char		ch = *valptr;

				if (SQL_STR_DOUBLE(ch, !standard_conforming_strings))
					appendStringInfoChar(str, ch);
				appendStringInfoChar(str, ch);
			}
			appendStringInfoChar(str, '\'');
			break;
	}

	pfree(extval);

	if (showtype < 0)
		return;

	/*
	 * For showtype == 0, append ::typename unless the constant will be
	 * implicitly typed as the right type when it is read in.
	 *
	 * XXX this code has to be kept in sync with the behavior of the parser,
	 * especially make_const.
	 */
	switch (constval->consttype)
	{
		case BOOLOID:
		case INT4OID:
		case UNKNOWNOID:
			/* These types can be left unlabeled */
			needlabel = false;
			break;
		case NUMERICOID:

			/*
			 * Float-looking constants will be typed as numeric, but if
			 * there's a specific typmod we need to show it.
			 */
			needlabel = !isfloat || (constval->consttypmod >= 0);
			break;
		default:
			needlabel = true;
			break;
	}
	if (needlabel || showtype > 0)
		appendStringInfo(str, "::%s",
						 format_type_with_typemod(constval->consttype,
												  constval->consttypmod));
}


/* ----------
 * get_sublink_expr			- Parse back a sublink
 * ----------
 */
static void
get_sublink_expr(SubLink *sublink, TRANSPARSE_ARGS)
{
	StringInfo	str = context->buf;
	Query	   *query = (Query *) (sublink->subselect);
	char	   *opname = NULL;
	bool		need_paren;

	if (sublink->subLinkType == ARRAY_SUBLINK)
		appendStringInfo(str, "ARRAY(");
	else
		appendStringInfoChar(str, '(');

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
			OpExpr	   *opexpr = (OpExpr *) sublink->testexpr;

			get_rule_expr(linitial(opexpr->args), context, true);
			opname = generate_operator_name(opexpr->opno,
											exprType(linitial(opexpr->args)),
											exprType(lsecond(opexpr->args)));
		}
		else if (IsA(sublink->testexpr, BoolExpr))
		{
			/* multiple combining operators, = or <> cases */
			char	   *sep;
			ListCell   *l;

			appendStringInfoChar(str, '(');
			sep = "";
			foreach(l, ((BoolExpr *) sublink->testexpr)->args)
			{
				OpExpr	   *opexpr = (OpExpr *) lfirst(l);

				Assert(IsA(opexpr, OpExpr));
				appendStringInfoString(str, sep);
				get_rule_expr(linitial(opexpr->args), context, true);
				if (!opname)
					opname = generate_operator_name(opexpr->opno,
											exprType(linitial(opexpr->args)),
											exprType(lsecond(opexpr->args)));
				sep = ", ";
			}
			appendStringInfoChar(str, ')');
		}
		else if (IsA(sublink->testexpr, RowCompareExpr))
		{
			/* multiple combining operators, < <= > >= cases */
			RowCompareExpr *rcexpr = (RowCompareExpr *) sublink->testexpr;

			appendStringInfoChar(str, '(');
			get_rule_expr((Node *) rcexpr->largs, context, true);
			opname = generate_operator_name(linitial_oid(rcexpr->opnos),
											exprType(linitial(rcexpr->largs)),
										  exprType(linitial(rcexpr->rargs)));
			appendStringInfoChar(str, ')');
		}
		else
			elog(ERROR, "unrecognized testexpr type: %d",
				 (int) nodeTag(sublink->testexpr));
	}

	need_paren = true;

	switch (sublink->subLinkType)
	{
		case EXISTS_SUBLINK:
			appendStringInfo(str, "EXISTS ");
			break;

		case ANY_SUBLINK:
			if (strcmp(opname, "=") == 0)		/* Represent = ANY as IN */
				appendStringInfo(str, " IN ");
			else
				appendStringInfo(str, " %s ANY ", opname);
			break;

		case ALL_SUBLINK:
			appendStringInfo(str, " %s ALL ", opname);
			break;

		case ROWCOMPARE_SUBLINK:
			appendStringInfo(str, " %s ", opname);
			break;

		case EXPR_SUBLINK:
		case ARRAY_SUBLINK:
			need_paren = false;
			break;

		default:
			elog(ERROR, "unrecognized sublink type: %d",
				 (int) sublink->subLinkType);
			break;
	}

	if (need_paren)
		appendStringInfoChar(str, '(');

	get_query_def(query, str, context->namespaces, NULL,
				  context->prettyFlags, context->indentLevel, context, NULL, false);

	if (need_paren)
		appendStringInfo(str, "))");
	else
		appendStringInfoChar(str, ')');
}


/* ----------
 * get_from_clause			- Parse back a FROM clause
 *
 * "prefix" is the keyword that denotes the start of the list of FROM
 * elements. It is FROM when used to parse back SELECT and UPDATE, but
 * is USING when parsing back DELETE.
 * ----------
 */
static void
get_from_clause(Query *query, const char *prefix, TRANSPARSE_ARGS)
{
	StringInfo	str = context->buf;
	bool		first = true;
	ListCell   *l, *infoLc;
	TransProvInfo *info;
	TransSubInfo *subInfo;
	TransParseRange *range;
	bool inDummy;
	bool isRoot = false;
	bool cross = false;
	bool newStatic;

	info = GET_TRANS_INFO(query);
	inDummy = (IsA(info->root, TransProvInfo)) || inStatic;
	subInfo = NULL;
	newStatic = inStatic;

	if (!inStatic)
	{
		cross = list_length(query->jointree->fromlist) > 1;
		subInfo = getTopJoinInfo(query, cross);
		isRoot = !inDummy && equal(subInfo, info->root);
	}

	if (subInfo)
		newStatic = inStatic || TRANS_GET_STATIC(subInfo);

	if (subInfo && !isRoot && !inDummy)
		MAKE_RANGE(range, subInfo);

	/*
	 * We use the query's jointree as a guide to what to print.  However, we
	 * must ignore auto-added RTEs that are marked not inFromCl. (These can
	 * only appear at the top level of the jointree, so it's sufficient to
	 * check here.)  This check also ensures we ignore the rule pseudo-RTEs
	 * for NEW and OLD.
	 */
	if (cross)
		infoLc = list_head(subInfo->children);

	foreach(l, query->jointree->fromlist)
	{
		Node	   *jtnode = (Node *) lfirst(l);

		if (IsA(jtnode, RangeTblRef))
		{
			int			varno = ((RangeTblRef *) jtnode)->rtindex;
			RangeTblEntry *rte = rt_fetch(varno, query->rtable);

			if (!rte->inFromCl)
				continue;
		}

		if (first)
		{
			appendContextKeyword(context, prefix,
								 -PRETTYINDENT_STD, PRETTYINDENT_STD, 2);
			first = false;
		}
		else
			appendStringInfoString(str, ", ");

		if (cross)
		{
			subInfo = (TransSubInfo *) lfirst(infoLc);
			infoLc = infoLc->next;
		}

		get_from_clause_item(jtnode, query, context, (Node *) subInfo, newStatic, !cross);
	}

	if (subInfo && !isRoot && !inDummy)
		range->end = str->len;
}

static void
get_from_clause_item(Node *jtnode, Query *query, TRANSPARSE_ARGS, bool isRoot)
{
	StringInfo	str = context->buf;
	Node *curSub;
	TransParseRange *range;
	bool newStatic;

	curSub = NULL;

	if (parentInfo && !inStatic)
		newStatic = TRANS_GET_STATIC(parentInfo);
	else
		newStatic = inStatic;

	if (parentInfo && !inStatic && !isRoot && !IsA(parentInfo, TransProvInfo))
		MAKE_RANGE(range, parentInfo);

	if (IsA(jtnode, RangeTblRef))
	{
		int			varno = ((RangeTblRef *) jtnode)->rtindex;
		RangeTblEntry *rte = rt_fetch(varno, query->rtable);
		bool		gavealias = false;

		parseAnnotations(rte->annotations, true, str);

		switch (rte->rtekind)
		{
			case RTE_RELATION:
				/* Normal relation RTE */
				appendStringInfo(str, "%s%s",
								 only_marker(rte),
								 generate_relation_name(rte->relid));
				break;
			case RTE_SUBQUERY:
				/* Subquery RTE */
				appendStringInfoChar(str, '(');
				get_query_def(rte->subquery, str, context->namespaces, NULL, context->prettyFlags,
							context->indentLevel, context, NULL, inStatic);
				appendStringInfoChar(str, ')');
				break;
			case RTE_FUNCTION:
				/* Function RTE */
				get_rule_expr(rte->funcexpr, context, true);
				break;
			case RTE_VALUES:
				/* Values list RTE */
				get_values_def(rte->values_lists, context);
				break;
			default:
				elog(ERROR, "unrecognized RTE kind: %d", (int) rte->rtekind);
				break;
		}

		parseAnnotations(rte->annotations, false, str);

		if (rte->alias != NULL)
		{
			appendStringInfo(str, " %s",
							 quote_identifier(rte->alias->aliasname));
			gavealias = true;
		}
		else if (rte->rtekind == RTE_RELATION &&
				 strcmp(rte->eref->aliasname, get_rel_name(rte->relid)) != 0)
		{
			/*
			 * Apparently the rel has been renamed since the rule was made.
			 * Emit a fake alias clause so that variable references will still
			 * work.  This is not a 100% solution but should work in most
			 * reasonable situations.
			 */
			appendStringInfo(str, " %s",
							 quote_identifier(rte->eref->aliasname));
			gavealias = true;
		}
		else if (rte->rtekind == RTE_FUNCTION)
		{
			/*
			 * For a function RTE, always give an alias. This covers possible
			 * renaming of the function and/or instability of the
			 * FigureColname rules for things that aren't simple functions.
			 */
			appendStringInfo(str, " %s",
							 quote_identifier(rte->eref->aliasname));
			gavealias = true;
		}

		if (rte->rtekind == RTE_FUNCTION)
		{
			if (rte->funccoltypes != NIL)
			{
				/* Function returning RECORD, reconstruct the columndefs */
				if (!gavealias)
					appendStringInfo(str, " AS ");
				get_from_clause_coldeflist(rte->eref->colnames,
										   rte->funccoltypes,
										   rte->funccoltypmods,
										   context);
			}
			else
			{
				/*
				 * For a function RTE, always emit a complete column alias
				 * list; this is to protect against possible instability of
				 * the default column names (eg, from altering parameter
				 * names).
				 */
				get_from_clause_alias(rte->eref, rte, context);
			}
		}
		else
		{
			/*
			 * For non-function RTEs, just report whatever the user originally
			 * gave as column aliases.
			 */
			get_from_clause_alias(rte->alias, rte, context);
		}
	}
	else if (IsA(jtnode, JoinExpr))
	{
		JoinExpr   *j = (JoinExpr *) jtnode;
		bool		need_paren_on_right;

		need_paren_on_right = PRETTY_PAREN(context) &&
			!IsA(j->rarg, RangeTblRef) &&
			!(IsA(j->rarg, JoinExpr) &&((JoinExpr *) j->rarg)->alias != NULL);

		if (!PRETTY_PAREN(context) || j->alias != NULL)
			appendStringInfoChar(str, '(');

		if (parentInfo)
			curSub = (Node *) TSET_LARG(parentInfo);
		get_from_clause_item(j->larg, query, context, curSub, newStatic, false);

		if (j->isNatural)
		{
			if (!PRETTY_INDENT(context))
				appendStringInfoChar(str, ' ');
			switch (j->jointype)
			{
				case JOIN_INNER:
					appendContextKeyword(context, "NATURAL JOIN ",
										 -PRETTYINDENT_JOIN,
										 PRETTYINDENT_JOIN, 0);
					break;
				case JOIN_LEFT:
					appendContextKeyword(context, "NATURAL LEFT JOIN ",
										 -PRETTYINDENT_JOIN,
										 PRETTYINDENT_JOIN, 0);
					break;
				case JOIN_FULL:
					appendContextKeyword(context, "NATURAL FULL JOIN ",
										 -PRETTYINDENT_JOIN,
										 PRETTYINDENT_JOIN, 0);
					break;
				case JOIN_RIGHT:
					appendContextKeyword(context, "NATURAL RIGHT JOIN ",
										 -PRETTYINDENT_JOIN,
										 PRETTYINDENT_JOIN, 0);
					break;
				default:
					elog(ERROR, "unrecognized join type: %d",
						 (int) j->jointype);
			}
		}
		else
		{
			switch (j->jointype)
			{
				case JOIN_INNER:
					if (j->quals)
						appendContextKeyword(context, " JOIN ",
											 -PRETTYINDENT_JOIN,
											 PRETTYINDENT_JOIN, 2);
					else
						appendContextKeyword(context, " CROSS JOIN ",
											 -PRETTYINDENT_JOIN,
											 PRETTYINDENT_JOIN, 1);
					break;
				case JOIN_LEFT:
					appendContextKeyword(context, " LEFT JOIN ",
										 -PRETTYINDENT_JOIN,
										 PRETTYINDENT_JOIN, 2);
					break;
				case JOIN_FULL:
					appendContextKeyword(context, " FULL JOIN ",
										 -PRETTYINDENT_JOIN,
										 PRETTYINDENT_JOIN, 2);
					break;
				case JOIN_RIGHT:
					appendContextKeyword(context, " RIGHT JOIN ",
										 -PRETTYINDENT_JOIN,
										 PRETTYINDENT_JOIN, 2);
					break;
				default:
					elog(ERROR, "unrecognized join type: %d",
						 (int) j->jointype);
			}
		}

		if (need_paren_on_right)
			appendStringInfoChar(str, '(');
		if (parentInfo)
			curSub = (Node *) TSET_RARG(parentInfo);
		get_from_clause_item(j->rarg, query, context, curSub, newStatic, false);
		if (need_paren_on_right)
			appendStringInfoChar(str, ')');

		context->indentLevel -= PRETTYINDENT_JOIN_ON;

		if (!j->isNatural)
		{
			if (j->using)
			{
				ListCell   *col;

				appendStringInfo(str, " USING (");
				foreach(col, j->using)
				{
					if (col != list_head(j->using))
						appendStringInfo(str, ", ");
					appendStringInfoString(str,
									  quote_identifier(strVal(lfirst(col))));
				}
				appendStringInfoChar(str, ')');
			}
			else if (j->quals)
			{
				appendStringInfo(str, " ON ");
				if (!PRETTY_PAREN(context))
					appendStringInfoChar(str, '(');
				get_rule_expr(j->quals, context, false);
				if (!PRETTY_PAREN(context))
					appendStringInfoChar(str, ')');
			}
		}
		if (!PRETTY_PAREN(context) || j->alias != NULL)
			appendStringInfoChar(str, ')');

		/* Yes, it's correct to put alias after the right paren ... */
		if (j->alias != NULL)
		{
			appendStringInfo(str, " %s",
							 quote_identifier(j->alias->aliasname));
			get_from_clause_alias(j->alias,
								  rt_fetch(j->rtindex, query->rtable),
								  context);
		}
	}
	else
		elog(ERROR, "unrecognized node type: %d",
			 (int) nodeTag(jtnode));

	if (parentInfo && !inStatic && !isRoot && !IsA(parentInfo, TransProvInfo))
		range->end = str->len;
}

/*
 * get_from_clause_alias - reproduce column alias list
 *
 * This is tricky because we must ignore dropped columns.
 */
static void
get_from_clause_alias(Alias *alias, RangeTblEntry *rte,
					  TransParseContext *context)
{
	StringInfo	str = context->buf;
	ListCell   *col;
	AttrNumber	attnum;
	bool		first = true;

	if (alias == NULL || alias->colnames == NIL)
		return;					/* definitely nothing to do */

	attnum = 0;
	foreach(col, alias->colnames)
	{
		attnum++;
		if (get_rte_attribute_is_dropped(rte, attnum))
			continue;
		if (first)
		{
			appendStringInfoChar(str, '(');
			first = false;
		}
		else
			appendStringInfo(str, ", ");
		appendStringInfoString(str,
							   quote_identifier(strVal(lfirst(col))));
	}
	if (!first)
		appendStringInfoChar(str, ')');
}

/*
 * get_from_clause_coldeflist - reproduce FROM clause coldeflist
 *
 * The coldeflist is appended immediately (no space) to buf.  Caller is
 * responsible for ensuring that an alias or AS is present before it.
 */
static void
get_from_clause_coldeflist(List *names, List *types, List *typmods,
						   TransParseContext *context)
{
	StringInfo	str = context->buf;
	ListCell   *l1;
	ListCell   *l2;
	ListCell   *l3;
	int			i = 0;

	appendStringInfoChar(str, '(');

	l2 = list_head(types);
	l3 = list_head(typmods);
	foreach(l1, names)
	{
		char	   *attname = strVal(lfirst(l1));
		Oid			atttypid;
		int32		atttypmod;

		atttypid = lfirst_oid(l2);
		l2 = lnext(l2);
		atttypmod = lfirst_int(l3);
		l3 = lnext(l3);

		if (i > 0)
			appendStringInfo(str, ", ");
		appendStringInfo(str, "%s %s",
						 quote_identifier(attname),
						 format_type_with_typemod(atttypid, atttypmod));
		i++;
	}

	appendStringInfoChar(str, ')');
}


/*
 * processIndirection - take care of array and subfield assignment
 *
 * We strip any top-level FieldStore or assignment ArrayRef nodes that
 * appear in the input, and return the subexpression that's to be assigned.
 * If printit is true, we also print out the appropriate decoration for the
 * base column name (that the caller just printed).
 */
static Node *
processIndirection(Node *node, TransParseContext *context, bool printit)
{
	StringInfo	str = context->buf;

	for (;;)
	{
		if (node == NULL)
			break;
		if (IsA(node, FieldStore))
		{
			FieldStore *fstore = (FieldStore *) node;
			Oid			typrelid;
			char	   *fieldname;

			/* lookup tuple type */
			typrelid = get_typ_typrelid(fstore->resulttype);
			if (!OidIsValid(typrelid))
				elog(ERROR, "argument type %s of FieldStore is not a tuple type",
					 format_type_be(fstore->resulttype));

			/*
			 * Print the field name.  Note we assume here that there's only
			 * one field being assigned to.  This is okay in stored rules but
			 * could be wrong in executable target lists.  Presently no
			 * problem since explain.c doesn't print plan targetlists, but
			 * someday may have to think of something ...
			 */
			fieldname = get_relid_attribute_name(typrelid,
											linitial_int(fstore->fieldnums));
			if (printit)
				appendStringInfo(str, ".%s", quote_identifier(fieldname));

			/*
			 * We ignore arg since it should be an uninteresting reference to
			 * the target column or subcolumn.
			 */
			node = (Node *) linitial(fstore->newvals);
		}
		else if (IsA(node, ArrayRef))
		{
			ArrayRef   *aref = (ArrayRef *) node;

			if (aref->refassgnexpr == NULL)
				break;
			if (printit)
				printSubscripts(aref, context);

			/*
			 * We ignore refexpr since it should be an uninteresting reference
			 * to the target column or subcolumn.
			 */
			node = (Node *) aref->refassgnexpr;
		}
		else
			break;
	}

	return node;
}

static void
printSubscripts(ArrayRef *aref, TransParseContext *context)
{
	StringInfo	str = context->buf;
	ListCell   *lowlist_item;
	ListCell   *uplist_item;

	lowlist_item = list_head(aref->reflowerindexpr);	/* could be NULL */
	foreach(uplist_item, aref->refupperindexpr)
	{
		appendStringInfoChar(str, '[');
		if (lowlist_item)
		{
			get_rule_expr((Node *) lfirst(lowlist_item), context, false);
			appendStringInfoChar(str, ':');
			lowlist_item = lnext(lowlist_item);
		}
		get_rule_expr((Node *) lfirst(uplist_item), context, false);
		appendStringInfoChar(str, ']');
	}
}

/*
 *
 */

static void
parseAnnotations (List *annots, bool start, StringInfo buf)
{
	ListCell *lc;
	Value *val;

	foreach(lc, annots)
	{
		val = (Value *) lfirst(lc);

		if (start)
			appendStringInfoString (buf, "<");
		else
			appendStringInfoString (buf, "</");

		appendStringInfoString (buf, strVal(val));

		appendStringInfoString (buf, ">");
	}
}

