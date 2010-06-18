/*-------------------------------------------------------------------------
 *
 * prov_dotalgebra.c
 *	  POSTGRES C
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/prov_dotalgebra.c,v 1.542 19.11.2008 08:13:06 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "provrewrite/prov_dotalgebra.h"
#include "provrewrite/prov_util.h"

/* dot output string templates */
#define ALGDOT_DIGRAPH_HEADER "digraph G {\n"
#define ALGDOT_DIGRAPH_FOOTER "}\n"

#define ALGDOT_SEL_NODE_TEMPLATE "\t\tsel%i [label=\"&#963;\",color=blue];\n"
#define ALGDOT_AGG_NODE_TEMPLATE "\t\tagg%i [label=\"&#945;\",color=red];\n"
#define ALGDOT_PROJ_NODE_TEMPLATE "\t\tproj%i [label=\"&#945;\",color=yellow];\n"
#define ALGDOT_JOIN_NODE_TEMPLATE "\t\tjoin%i [label=\"X\",color=green];"
#define ALGDOT_UNION_NODE_TEMPLATE "\t\tset%i [label=\"u\",color=brown];"
#define ALGDOT_INTERSEC_NODE_TEMPLATE "\t\tset%i [label=\"int\",color=brown];"
#define ALGDOT_DIFFNODE_TEMPLATE "\t\tset%i [label=\"-\",color=brown];"

#define ALGDOT_CLUSTER_HEADER "\tsubgraph cluster%i {\n\t\tnode [style=filled];\n\t\tlabel = \"%s\";\n\t\tstyle=filled;\n\t\tcolor=lightgrey;\n"
#define ALGDOT_CLUSTER_FOOTER "\t}\n"

#define ALGDOT_REL_HEADER "\t{\n\t\trank=same;\n"
#define ALGDOT_REL_FOOTER "\t}\n"
#define ALGDOT_REL_TEMPLATE "\t\trel%i [label=\"%s\",shape=box,color=yellow,style=filled];\n"

/* data types */
typedef struct SetWalkerContext {
	Query *query;
	List **relations;
	int *nodeId;
} SetWalkerContext;

/*
 *
 */

void
dotQueryAlgebra (Query *query, StringInfo str)
{
	/* append header */
	appendStringInfo(str,ALGDOT_DIGRAPH_HEADER);

	/* build clusters for query nodes */

	/* build nodes for relations */
	dotRelations (relations, str);

	/* append footer */
	appendStringInfo(str,ALGDOT_DIGRAPH_FOOTER);
}

/*
 *
 */

static void
dotQueryNode (Query *query, List **relations, RangeTblEntry *rte, StringInfo str)
{
	if (query->setOperations)
		dotSetQuery (query,relations,rte);
}

static void
dotSetQuery (Query *query, List **relations, RangeTblEntry *rte, StringInfo str)
{
	/* append header */
	appendStringInfo(query, ALGDOT_CLUSTER_HEADER);

	/* build clusters for query nodes */
	dotSetQueryWalker (query->setOperations, NULL);

	/* append footer */
	appendStringInfo(str,ALGDOT_CLUSTER_FOOTER);
}

static bool
dotSetQueryWalker (Node *node, SetWalkerContext *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, SetOperationStmt))
	{
		SetOperationStmt *set;

		set = (SetOperationStmt *) node;

	}
	else if (IsA(node, RangeTblRef))
	{
		RangeTblRef *ref;

		ref = (RangeTblRef *) node;

		*(context->relations) = lappend(*(context->relations),
				rt_fetch(context->query->rtable, ref->rtindex);
	}

	return expression_tree_walker(node, dotSetQueryWalker, (void *) context);
}

static void
dotJoinTree (Query *query)
{

}

static void
dotJoinTreeWalker (Node *node, void *context)
{
	if (node == null)
		return false;

	if (IsA(node,JoinExpr))
	{
		JoinExpr *join;

		join = (JoinExpr *) node;
	}
	else if (IsA(node, RangeTblRef))
	{
		RangeTblRef *rtRef;

		rtRef = (RangeTblRef *) node;
	}

	return expression_tree_walker(node, dotJoinTreeWalker, (void *) context);
}

static void dotRelations (List *relations, StringInfo str) {
	RangeTblEntry *rte;
	ListCell *lc;
	int i;

	/* append header */
	appendStringInfo(str,ALGDOT_REL_HEADER);

	/* output a node for each relation */
	foreachi (relations, lc, i)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		appendStringInfo(ALGDOT_REL_TEMPLATE, i, getRelationNameUnqualified(rte->relid));//TODO BASERELATION
	}
	/* append footer */
	appendStringInfo(str,ALGDOT_REL_FOOTER);
}
