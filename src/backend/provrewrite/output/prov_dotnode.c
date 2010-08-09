/*-------------------------------------------------------------------------
 *
 * prov_dotnode.c
 *	  generates a dot graph definition for query nodes.
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/prov_dotnode.c,v 1.322 2008/01/09 08:46:44 bglavic $
 *
 * NOTES
 *
 *-------------------------------------------------------------------------
 */
#include <stdlib.h>
#include "postgres.h"

#include "lib/stringinfo.h"
#include "nodes/parsenodes.h"
#include "parser/parsetree.h"
#include "parser/parse_relation.h"
#include "utils/lsyscache.h"

#include "provrewrite/prov_dotnode.h"
#include "provrewrite/provlog.h"
#include "provrewrite/prov_algmap.h"
#include "provrewrite/prov_sublink_util_search.h"
#include "provrewrite/prov_util.h"

/* string constants for dot command script generation */
#define DOT_SCRIPT_HEADER "digraph G {\nnode [fontsize=24];\n\n"
#define DOT_SCRIPT_FOOTER "\n}\n"

#define DOT_QUERY_HEADER "subgraph cluster%i {\n"
#define DOT_QUERY_FOOTER "}\n"
#define DOT_QUERY_STYLE "node [style=filled];\nstyle=filled;\ncolor=lightgrey;\n"
#define DOT_QUERY_LINK "%s -> %s;\n"

#define DOT_SUBLINK "%s [label=\"M\",color=chocolate,style=filled];"

#define DOT_REL_HEADER "\n{\nrank=same;\noderdir=LR;\n"
#define DOT_REL_FOOTER "}\n\n"
#define DOT_BASEREL "rel%i [label=\"%s\",shape=box,color=yellow,style=filled];\n"
#define DOT_ORDERING_EDGE_HEADER "\n\nedge [style=invis,constraint=true];\n"
#define DOT_ORDERING_EDGE "rel%i->"

#define DOT_SELECTION "%s [label=\"&#963;\",color=orange];\n"
#define DOT_PROJECTION "%s [label=\"&#928;\",color=violet];\n"
#define DOT_AGGREGATION "%s [label=\"&#945;\",color=red];\n"
#define DOT_CROSSPROD "%s [label=\"X\",color=green];\n"
#define DOT_JOIN "%s [label=\"|X|\",color=green];\n"
#define DOT_LEFT_JOIN "%s [label=\"=X\",color=green];\n"
#define DOT_RIGHT_JOIN "%s [label=\"X=\",color=green];\n"
#define DOT_FULL_JOIN "%s [label=\"=X=\",color=green];\n"
#define DOT_UNION "%s [label=\"&#8746;\",color=lightblue2];\n"
#define DOT_INTERSECTION "%s [label=\"&#8745;\",color=lightblue2];\n"
#define DOT_EXCEPT "%s [label=\"-\",color=lightblue2];\n"

#define DOT_LINK "%s -> %s;\n"

#define DOT_UTILITY_DUMMY_GRAPH "digraph G {\nutil [label=\"Utility\",shape=box,color=red,style=filled];\n}\n"

#define SUBQUERY_RELLIST_PLACEHOLDER -1

/* types */

/* enumeration for possible dot graph types */
typedef enum DotQueryType
{
	DOT_TYPE_PROJECTION,
	DOT_TYPE_AGGREGATION,
	DOT_TYPE_PROJAGG,
	DOT_TYPE_HAVAGG,
	DOT_TYPE_SELECTION,
	DOT_TYPE_JOIN,
	DOT_TYPE_SETOP
} DotQueryType;

typedef struct QueryDotContext
{
	List *queryNodes;
	List *parents;
	List *rte;
	List *relPosList;
	List *numSubsList;
} QueryDotContext;

/* context information for dot generation */
typedef struct DotContext
{
	int queryCounter;
	int operatorCounter;
	int baseRelCounter;
	StringInfo queriesString;
	StringInfo baseRelString;
	StringInfo linkString;
	StringInfo sublinkString;
	QueryDotContext *queryCon;
	List *relPosList;
} DotContext;




/* prototypes */
static StringInfo generateDotQuery (Query *query);
static void generateOrderingEdges (DotContext *context);
static List *generateDotQueryNodeString (Query *query, DotContext *context, RangeTblEntry *rte, char *parent);
static List *processChildren (DotContext *context);
static List *generateRelOrderingList (DotContext *context, QueryDotContext *queryCon, List *childrenRelPosList);

static int generateDotForSublinks (Node *node, DotContext *context, char *parent);

static void generateDotJoinTree (Query *query, DotContext *context, bool top, char *parent);
static void generateDotJoin (Query *query, Node *node, DotContext *context, bool top, char *parent);
static void generateDotSelection (Query *query, DotContext *context, bool top, char *parent, bool having);
static void generateDotProjection (Query *query, DotContext *context, bool top, char *parent);
static void generateDotAggregation (Query *query, DotContext *context, bool top, char *parent);
static void generateDotSetOp (Query *query, Node *setOp, DotContext *context, bool top, char *parent);
static void generateDotRTE(RangeTblEntry *rte, DotContext *context, char *parent);

static bool topCrossProdNeeded (Query *query, bool top);
static DotQueryType getDotQueryType (Query *query);
static QueryDotContext *createQueryDotContext (void);
static DotContext *createDotContext (void);
//static void freeDotContext (DotContext *context);
static char *appendStringId (char *string, int i);


/*
 *
 */

void
showDotQuery (Query *node)
{
	StringInfo str;
	StringInfo commandString;

	str = generateDotQuery (node);

	initStringInfo(commandString);
	appendStringInfoString(commandString, "dot");

	system (commandString->data);
}

/*
 *
 */

StringInfo
dotQuery (Query *query)
{
	StringInfo str;

	str = generateDotQuery (query);

	LOGDEBUG(str->data);

	return str;
}

/*
 *
 */

static StringInfo
generateDotQuery (Query *query) {
	StringInfo str;
	DotContext *context;

	str = makeStringInfo();

	if(query->utilityStmt)
	{
		appendStringInfoString (str, DOT_UTILITY_DUMMY_GRAPH);
		return str;
	}
	/* see stringinfo.h for an explanation of this maneuver */
	context = createDotContext ();

	/* generate header and subheaders */
	appendStringInfoString (str, DOT_SCRIPT_HEADER);
	appendStringInfoString (context->baseRelString, DOT_REL_HEADER);

	/* generate commands for top query node of query */
	context->relPosList = generateDotQueryNodeString (query, context, NULL, NULL);

	/* generate invisible edges between the base relation node to enforce a left-to-right ordering in the layout */
	generateOrderingEdges (context);

	/* generate footers */
	appendStringInfoString (context->baseRelString, DOT_REL_FOOTER);

	/* combine commands for query nodes, base relations and links */
	appendStringInfoString (str, context->queriesString->data);
	appendStringInfo (str, "\n%s\n",context->sublinkString->data);
	appendStringInfoString (str, context->baseRelString->data);
	appendStringInfoString (str, context->linkString->data);

	appendStringInfoString (str, "\n}\n");

	return str;
}

/*
 *
 */

static void
generateOrderingEdges (DotContext *context)
{
	ListCell *lc;

	appendStringInfoString (context->baseRelString, DOT_ORDERING_EDGE_HEADER);

	foreach(lc, context->relPosList)
	{
		if(lc->next != NULL)
			appendStringInfo (context->baseRelString, DOT_ORDERING_EDGE, lfirst_int(lc));
		else
			appendStringInfo (context->baseRelString, "rel%i;\n", lfirst_int(lc));
	}
}

/*
 *
 */

static List *
generateDotQueryNodeString (Query *query, DotContext *context, RangeTblEntry *rte, char *parent)
{
	int curId;
	char *name;

	context->queryCon = createQueryDotContext ();
	curId = (context->queryCounter)++;
	name = appendStringId("top", curId);

	/* create header and label */
	appendStringInfo (context->queriesString, DOT_QUERY_HEADER, curId);

	if (rte)
		appendStringInfo (context->queriesString, "label = \"%s\"\n", getAlias(rte));
	else
		appendStringInfo (context->queriesString, "label = \"TopQueryNode\"\n");

	appendStringInfo (context->queriesString, DOT_QUERY_STYLE);

	/* add link from parent to top node */
	if (parent)
		appendStringInfo (context->linkString, DOT_LINK, parent, name);

	/* create nodes for algebra operators of query node and links between them */
	switch(getDotQueryType (query))
	{
		case DOT_TYPE_SELECTION:
			generateDotSelection (query, context, true, name, false);
		break;
		case DOT_TYPE_AGGREGATION:
			generateDotAggregation (query, context, true, name);
		break;
		case DOT_TYPE_PROJECTION:
			generateDotProjection (query, context, true, name);
		break;
		case DOT_TYPE_JOIN:
			generateDotJoinTree (query, context, true, name);
		break;
		case DOT_TYPE_PROJAGG:
			generateDotProjection (query, context, true, name);
		break;
		case DOT_TYPE_HAVAGG:
			generateDotSelection (query, context, true, name, true);
		break;
		case DOT_TYPE_SETOP:
			generateDotSetOp (query, query->setOperations, context, true, name);
		break;
		default:
			break;
	}

	/* add footer */
	appendStringInfoString (context->queriesString, DOT_QUERY_FOOTER);

	/* process child query nodes */
	return processChildren (context);
}

/*
 *
 */

static List *
processChildren (DotContext *context)
{
	QueryDotContext *curCon;
	Query *query;
	RangeTblEntry *rte;
	List *childrenRelPosList;
	char *parent;
	ListCell *qLc, *rLc, *pLc;

	curCon = context->queryCon;
	context->queryCon = NULL;
	childrenRelPosList = NIL;

	qLc = curCon->queryNodes ? curCon->queryNodes->head : NULL;
	rLc = curCon->rte ? curCon->rte->head : NULL;
	pLc = curCon->parents ? curCon->parents->head : NULL;
	for(;qLc != NULL; qLc = qLc->next, rLc = rLc->next, pLc = pLc->next)
	{
		query = (Query *) lfirst(qLc);
		parent = (char *) lfirst(pLc);
		rte = (RangeTblEntry *) lfirst(rLc);

		childrenRelPosList = lappend(childrenRelPosList, generateDotQueryNodeString (query, context, rte, parent));
	}

	if (!childrenRelPosList)
		return curCon->relPosList;

	return generateRelOrderingList (context, curCon, childrenRelPosList);
}

/*
 *
 */

static List *
generateRelOrderingList (DotContext *context, QueryDotContext *queryCon, List *childrenRelPosList)
{
	List *result;
	List *orderedChildrenList;
	List *subList;
	ListCell *lc, *childrenPos, *innerLc;
	int numSub, i, pos;

	orderedChildrenList = NIL;
	childrenPos = childrenRelPosList ? childrenRelPosList->head : NULL;
	result = NIL;

	/* order the children so they appear in the order of processing for each generated node in the query graph and
	 * the children of a child graph node appear before the children of the parent graph node. This will result in a layout where
	 * the ordering of e.g. the children of a join will be the expected one (left child of the join is also left in the graph) and
	 * in addition badly routed edges are omited by placing the children of higher level nodes of a query block to the right.
	 */

	/* generate the mentioned sort order for the subqueries rel pos lists */
	foreach(lc, queryCon->numSubsList)
	{
		numSub = lfirst_int(lc);
		subList = NIL;

		for(i = 0; i < numSub; i++, childrenPos = childrenPos->next)
		{
			subList = lappend(subList, lfirst(childrenPos));
		}

		orderedChildrenList = list_concat(subList, orderedChildrenList);
	}

	subList = NIL;
	for(; childrenPos != NULL; childrenPos = childrenPos->next)
	{
		subList = lappend(subList, lfirst(childrenPos));
	}
	orderedChildrenList = list_concat(subList, orderedChildrenList);

	/* generate final rel pos list from "normal" rel pos list and chilren relpos lists */
	childrenPos = orderedChildrenList ? orderedChildrenList->head : NULL;

	foreach(lc, queryCon->relPosList)
	{
		pos = lfirst_int (lc);

		/* -1 indicates that the next child rel pos list should be inserted at this position */
		if (pos == -1)
		{
			subList = (List *) lfirst(childrenPos);

			foreach(innerLc, subList)
			{
				result = lappend_int(result, lfirst_int(innerLc));
			}

			childrenPos = childrenPos->next;
		}
		else
			result = lappend_int(result, pos);
	}

	/* add unprocessed chilren rel pos lists */
	for(; childrenPos != NULL; childrenPos = childrenPos->next)
	{
		subList = (List *) lfirst(childrenPos);

		foreach(lc, subList)
		{
			result = lappend_int(result, lfirst_int(lc));
		}
	}

	return result;
}

/*
 * Checks for the type of algebra operator sequence represented by a query node. This is needed to know
 * which type of algebra operator node should be the top node of a query node subgraph.
 */

static DotQueryType
getDotQueryType (Query *query)
{
	/* query is an aggregation check if we should put an projection or selection
	 * for a having clause above it
	 */
	if (query->hasAggs)
	{
		if (isProjectionOverAgg(query))
			return DOT_TYPE_PROJAGG;

		if (query->havingQual)
			return DOT_TYPE_HAVAGG;

		return DOT_TYPE_AGGREGATION;
	}

	/* query is an set operation */
	if (query->setOperations)
		return DOT_TYPE_SETOP;

	/* no set operation or aggregation */
	if(isProjection(query))
		return DOT_TYPE_PROJECTION;

	/* check if top is a selection */
	if (query->jointree->quals)
		return DOT_TYPE_SELECTION;

	return DOT_TYPE_JOIN;
}


/*
 *
 */

static void
generateDotSetOp (Query *query, Node *setOp, DotContext *context, bool top, char *parent)
{
	SetOperationStmt *op;
	char *name;

	if(IsA(setOp, SetOperationStmt))
	{
		op = (SetOperationStmt *) setOp;

		if (top)
			name = parent;
		else
			name = appendStringId("node", (context->operatorCounter)++);

		switch(op->op)
		{
			case SETOP_UNION:
				appendStringInfo(context->queriesString, DOT_UNION, name);
			break;
			case SETOP_INTERSECT:
				appendStringInfo(context->queriesString, DOT_INTERSECTION, name);
			break;
			case SETOP_EXCEPT:
				appendStringInfo(context->queriesString, DOT_EXCEPT, name);
			break;
			default:
				//TODO
			break;
		}

		if (!top)
			appendStringInfo(context->queriesString, DOT_QUERY_LINK, parent, name);

		generateDotSetOp (query, op->larg, context, false, name);
		generateDotSetOp (query, op->rarg, context, false, name);
	}
	else
	{
		RangeTblRef *rtRef;
		RangeTblEntry *rte;

		rtRef = (RangeTblRef *) setOp;
		rte = rt_fetch(rtRef->rtindex, query->rtable);

		generateDotRTE (rte, context, parent);
	}
}

/*
 *
 */

static void
generateDotAggregation (Query *query, DotContext *context, bool top, char *parent)
{
	char *name;
	int numSublinks;

	/* generate node for projection */
	if (top)
	{
		name = parent;
		appendStringInfo(context->queriesString, DOT_AGGREGATION, name);
	}
	else
	{
		name = appendStringId("node", (context->operatorCounter)++);
		appendStringInfo(context->queriesString, DOT_AGGREGATION, name);
		appendStringInfo(context->queriesString, DOT_QUERY_LINK, parent, name);
	}

	/* generate nodes for sublink queries in group by */
	if (query->groupClause)
	{
		numSublinks = generateDotForSublinks((Node *) getGroupByTLEs(query), context, name);

		if (numSublinks > 0)
			context->queryCon->numSubsList = lappend_int(context->queryCon->numSubsList, numSublinks);
	}
	/* decide which operator should be next */
	if (isProjectionUnderAgg(query))
	{
		generateDotProjection (query, context, false, name);
		return;
	}

	if (query->jointree->quals)
		generateDotSelection (query, context, false, name, false);
	else
		generateDotJoinTree (query, context, false, name);
}

/*
 *
 */

static void
generateDotProjection (Query *query, DotContext *context, bool top, char *parent)
{
	char *name;
	int numSublinks;

	/* generate node for projection */
	if (top)
	{
		name = parent;
		appendStringInfo(context->queriesString, DOT_PROJECTION, name);
	}
	else
	{
		name = appendStringId("node", (context->operatorCounter)++);
		appendStringInfo(context->queriesString, DOT_PROJECTION, name);
		appendStringInfo(context->queriesString, DOT_QUERY_LINK, parent, name);
	}

	/* generate nodes for sublink queries */
	numSublinks = generateDotForSublinks((Node *) query->targetList, context, name);

	if (numSublinks > 0)
		context->queryCon->numSubsList = lappend_int(context->queryCon->numSubsList, numSublinks);

	/* decide which operator should be next */
	if (query->hasAggs && top)
	{
		if (query->havingQual)
			generateDotSelection (query, context, false, name, true);
		else
			generateDotAggregation (query, context, false, name);
	}
	else if (query->jointree->quals)
		generateDotSelection (query, context, false, name, false);
	else
		generateDotJoinTree (query, context, false, name);
}

/*
 *
 */

static void
generateDotSelection (Query *query, DotContext *context, bool top, char *parent, bool having)
{
	char *name;
	int numSublinks;

	/* check if is top node */
	if(top)
	{
		name = parent;
		appendStringInfo(context->queriesString, DOT_SELECTION, name);
	}
	else
	{
		name = appendStringId("node", (context->operatorCounter)++);
		appendStringInfo(context->queriesString, DOT_SELECTION, name);
		appendStringInfo(context->queriesString, DOT_QUERY_LINK, parent, name);
	}

	/* generate nodes for sublink queries */
	if (having)
		numSublinks = generateDotForSublinks(query->havingQual, context, name);
	else
		numSublinks = generateDotForSublinks(query->jointree->quals, context, name);

	if (numSublinks > 0)
		context->queryCon->numSubsList = lappend_int(context->queryCon->numSubsList, numSublinks);

	/* decide which operator is produced next */
	if (having)
		generateDotAggregation (query, context, false, name);
	else
		generateDotJoinTree (query, context, false, name);
}

/*
 *
 */

static void
generateDotJoinTree (Query *query, DotContext *context, bool top, char *parent)
{
	ListCell *lc;
	Node *fromItem;
	char *name;

	/* generate a cross-product parent node if the fromlist contains more than one
	 * jointree.
	 */
	if (topCrossProdNeeded(query, top))
	{
		if (top)
		{
			name = parent;
			appendStringInfo(context->queriesString, DOT_CROSSPROD, name);
		}
		else
		{
			name = appendStringId("node",(context->operatorCounter)++);
			appendStringInfo(context->queriesString, DOT_CROSSPROD, name);
			appendStringInfo(context->queriesString, DOT_QUERY_LINK, parent, name);
			parent = name;
		}

		top = false;
	}

	/* add each from clause item as a child of the parent node. */
	foreach(lc, query->jointree->fromlist)
	{
		fromItem = (Node *) lfirst(lc);
		generateDotJoin (query, fromItem, context, top, parent);
	}
}

/*
 *
 */

static bool
topCrossProdNeeded (Query *query, bool top)
{
	if (list_length(query->jointree->fromlist) > 1)
		return true;

	if (top)
		return (!IsA(linitial(query->jointree->fromlist), JoinExpr));

	return false;
}

/*
 *
 */

static void
generateDotJoin (Query *query, Node *node, DotContext *context, bool top, char *parent)
{
	char *name;

	/* we are processing a join node */
	if (IsA(node, JoinExpr))
	{
		JoinExpr *join;

		join = (JoinExpr *) node;

		/* generate joinNode and link from parent */
		if (top)
			name = parent;
		else
			name = appendStringId("node", (context->operatorCounter)++);

		switch(join->jointype)
		{
		case JOIN_INNER:
			appendStringInfo(context->queriesString, DOT_JOIN, name);
		break;
		case JOIN_LEFT:
			appendStringInfo(context->queriesString, DOT_LEFT_JOIN, name);
			break;
		case JOIN_RIGHT:
			appendStringInfo(context->queriesString, DOT_RIGHT_JOIN, name);
			break;
		case JOIN_FULL:
			appendStringInfo(context->queriesString, DOT_FULL_JOIN, name);
			break;
		default:
			//TODO
			break;
		}

		if (!top)
			appendStringInfo(context->queriesString, DOT_QUERY_LINK, parent, name);

		/* process sublinks in join expr qual */
		generateDotForSublinks((Node *) join->quals, context, name);

		/* process children */
		generateDotJoin (query, join->larg, context, false, name);
		generateDotJoin (query, join->rarg, context, false, name);
	}
	/* we are processing a range table reference */
	else
	{
		RangeTblRef *rtRef;
		RangeTblEntry *rte;

		rtRef = (RangeTblRef *) node;
		rte = rt_fetch(rtRef->rtindex, query->rtable);

		generateDotRTE (rte, context, parent);
	}

}

/*
 *
 */

static void
generateDotRTE (RangeTblEntry *rte, DotContext *context, char *parent)
{
	int relId;
	char *relname;

	switch(rte->rtekind)
	{
		case RTE_RELATION:
			relname = get_rel_name(rte->relid);
			relId = (context->baseRelCounter)++;
			appendStringInfo(context->baseRelString, DOT_BASEREL, relId, relname);
			appendStringInfo(context->linkString, DOT_LINK, parent, appendStringId("rel", relId));

			/* add base relation node id to the left to right ordering list */
			context->queryCon->relPosList = lappend_int(context->queryCon->relPosList, relId);
		break;
		case RTE_SUBQUERY:
			context->queryCon->parents = lappend(context->queryCon->parents, parent);
			context->queryCon->rte = lappend(context->queryCon->rte, rte);
			context->queryCon->queryNodes = lappend(context->queryCon->queryNodes, rte->subquery);

			/* add a -1 to the left to right ordering list to later know that we have to insert rel ids for the subquery node */
			context->queryCon->relPosList = lappend_int
					(context->queryCon->relPosList, SUBQUERY_RELLIST_PLACEHOLDER);
		break;
		default:
		//TODO error
		break;
	}
}

/*
 *
 */

static int
generateDotForSublinks (Node *node, DotContext *context, char *parent)
{
	List *sublinks;
	char *mapname;
	ListCell *lc;
	SubLink *sublink;
	QueryDotContext *con;

	con = context->queryCon;
	sublinks = NIL;
	findExprSublinkWalker(node, &sublinks);

	if (list_length(sublinks) > 0)
	{
		mapname = appendStringId("map",(context->operatorCounter)++);
		appendStringInfo(context->sublinkString, DOT_SUBLINK, mapname);
		appendStringInfo(context->linkString, DOT_LINK, parent, mapname);

		foreach(lc, sublinks)
		{
			sublink = (SubLink *) lfirst(lc);

			con->parents = lappend(con->parents, mapname);
			con->queryNodes = lappend(con->queryNodes, sublink->subselect);
			con->rte = lappend(con->rte, NULL);
		}
	}

	return list_length(sublinks);
}

/*
 *
 */

static QueryDotContext *
createQueryDotContext (void)
{
	QueryDotContext *result;

	result = (QueryDotContext *) palloc(sizeof(QueryDotContext));

	result->parents = NIL;
	result->queryNodes = NIL;
	result->rte = NIL;
	result->relPosList = NIL;
	result->numSubsList = NIL;

	return result;
}

/*
 *
 */

static DotContext *
createDotContext (void)
{
	DotContext *result;

	result = (DotContext *) palloc(sizeof(DotContext));

	result->queriesString = makeStringInfo ();
	result->baseRelString = makeStringInfo ();
	result->linkString = makeStringInfo ();
	result->sublinkString = makeStringInfo ();
	result->queryCounter = 0;
	result->operatorCounter = 0;
	result->baseRelCounter = 0;
	result->queryCon = NULL;
	result->relPosList = NIL;

	return result;
}

//static void
//freeDotContext (DotContext *context)
//{
//
//}

static char *
appendStringId (char *string, int i)
{
	StringInfo result;

	result = makeStringInfo ();
	appendStringInfo(result, "%s%i", string, i);

	return result->data;
}
