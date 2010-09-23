/*-------------------------------------------------------------------------
 *
 * provrewrite.c
 *	  PERM C -  Backend Perm provenance extension module main entry point.
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/provrewrite.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
 *
 * NOTES
 *				This module is called by the Postgres TCop component after analzying a query. The TCop passes the analyzed
 *				This query tree to this module and passes it output (a possibly modified query tree) to the optimizer
 *				that in turn generates a plan for the query which is then executed by the Executor. This module traverses
 *				its input query tree and searches for sub trees that are marked for provenance rewrite. If such a
 *				its subtree is found the appropriate code to rewrite the sub tree is called.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"					// needed for all stuff

#include "access/heapam.h"				// heap access
#include "catalog/pg_type.h"			// system table for data types with initial content
#include "catalog/pg_operator.h"		// pg_operator system table for operator lookup
#include "utils/guc.h"
#include "nodes/print.h"				// pretty print node (trees)
#include "nodes/makefuncs.h"			// needed to create new nodes
#include "optimizer/clauses.h"			// tools for expression clauses
#include "parser/parse_relation.h"		// create Vars for all attr  of a RTE
#include "parser/parse_expr.h"			// expression transformation used for expression type calculation
#include "parser/parsetree.h"			// routines to extract info from parse trees
#include "parser/parse_oper.h"			// for lookup of operators
#include "rewrite/rewriteManip.h"		// Querytree manipulation subroutines for rewrites
#include "utils/builtins.h"
#include "utils/lsyscache.h"			// common system catalog access queries
#include "utils/syscache.h"				// used to release heap tuple references

#include "metaq/parse_metaq.h"

#include "provrewrite/prov_copy_spj.h"
#include "provrewrite/prov_spj.h"
#include "provrewrite/prov_set.h"
#include "provrewrite/prov_aggr.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_adaptsuper.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/provattrname.h"
#include "provrewrite/provlog.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_sublink.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_restr_pushdown.h"
#include "provrewrite/prov_copy_map.h"
#include "provrewrite/prov_copy_agg.h"
#include "provrewrite/prov_copy_set.h"
#include "provrewrite/prov_copy_inclattr.h"
#include "provrewrite/prov_trans_main.h"
#include "provrewrite/prov_trans_bitset.h"

/*
 * Global variables.
 */
// a list of reference counts for base relation. It is used to generate provenance attribute names.
static List *relRefCount;

/* Function declarations */
static Query *traverseQueryTree (RangeTblEntry *rteQuery, Query *query, char *cursorName);

/*
 * Rewrites a list of queries (a single input query might have been rewritten into multiple queries
 * by the postgres rewriter) by calling provenanceRewriteQuery for each query in the list.
 */
List *
provenanceRewriteQueryList (List *queries)
{
	ListCell *l;

	foreach (l, queries)
	{
		Node *command = (Node *) lfirst(l);

		if (IsA (command, Query))
			l->data.ptr_value = provenanceRewriteQuery ((Query *) command);
	}

	return queries;
}

/*
 * This method checks that the query to be rewritten is a SELECT query, expands THIS expressions, resets
 * Transformation provenance representation function data structures. If the query contains at least one
 * sub tree that should be rewritten, we call traverseQueryTree to find and rewrite these parts. Afterwards,
 * we try to apply selection pushdowns not considered by Postgres.
 */

Query *
provenanceRewriteQuery (Query *query)
{
	char *cursorName = NULL;

	/* initialize pStack and baseRelStack*/
	pStack = NIL;
	baseRelStack = NIL;
	relRefCount = NIL;
	rewriteMethodStack = NIL;

	/* if this is a close cursor stmt, release the trans prov function holds (if any) */
	if (query->utilityStmt && IsA(query->utilityStmt,ClosePortalStmt))
		releaseHold(((ClosePortalStmt *) query->utilityStmt)->portalname);

	dropTransProvQueryIndex();

	/* only SELECT commands can be rewritten */
	if (query->commandType != CMD_SELECT)
		return query;//TODO

	/* handle this expressions here */
	query = handleThisExprs(query);

	LOGNODE(query, "complete query tree");

	/* check if we have to rewrite a part of the query */
	if (!hasProvenanceSubquery(query))
		return query;

	/* try to pushdown selections be aware of provenance attrs */
	//query = pushdownSelections(query);

	/* if it is a cursor declaration notify the transformation provenance rewriter */
	if (query->utilityStmt && IsA(query->utilityStmt, DeclareCursorStmt))
		cursorName = ((DeclareCursorStmt *) query->utilityStmt)->portalname;

	/* traverse query tree and search for nodes marked for provenance rewrite */
	query = traverseQueryTree (NULL, query, cursorName);

	/* try to pushdown selections */
	query = pushdownSelections(query);

	LOGNODE(query, "complete rewritten query tree");
	LOGDEBUG(parseBackSafe(copyObject(query))->data);

	/* store the rewrite strategies that were used in the query tree */
	((ProvInfo *) query->provInfo)->rewriteInfo = copyObject(rewriteMethodStack);

	return query;
}

/*
 * Recursively traverses a query tree to find query nodes marked for provenance rewrite (provRewrite = true). These nodes
 * are passed to the appropriate provenance rewriter.
 */

static Query *
traverseQueryTree (RangeTblEntry *rteQuery, Query *query, char *cursorName)
{
	RangeTblEntry *rtEntry;
	ListCell *lc;
	IntoClause *into = NULL;
	Node *utility = NULL;

	// is query marked for provenance rewrite?
	if (IsProvRewrite(query))
	{
		LOGNODE(query, "query tree to be rewritten");

		resetRelReferences ();
		resetUniqueNameGens ();

		/* remove dummy target list entry for provenance attributes added during parse analysis */
		removeDummyProvenanceTEs(query);

		/* if query has a into clause we have to remove it and
		 * set it for the new top query returned by the prov rewriter.
		 * The same applies to utilityStmt.
		 */
		if (query->intoClause) {
			into = query->intoClause;
			query->intoClause = NULL;
		}
		if (query->utilityStmt) {
			utility = query->utilityStmt;
			query->utilityStmt = NULL;
		}

		/* rewrite query node according to the requested provenance type */
		switch(ContributionType(query))
		{
			case CONTR_INFLUENCE:
				query = rewriteQueryNode (query);
			break;
			case CONTR_COPY_PARTIAL_TRANSITIVE:
			case CONTR_COPY_PARTIAL_NONTRANSITIVE:
			case CONTR_COPY_COMPLETE_TRANSITIVE:
			case CONTR_COPY_COMPLETE_NONTRANSITIVE:
			{
				int numQAttrs;

				numQAttrs = list_length(query->targetList);
				generateCopyMaps(query);
				query = rewriteQueryNodeCopy (query);
				addTopCopyInclExpr(query, numQAttrs);
			}
			break;
			case CONTR_TRANS_SET:
			case CONTR_TRANS_SQL:
			case CONTR_TRANS_XML:
			case CONTR_TRANS_XML_SIMPLE:
			case CONTR_MAP:
				query = rewriteQueryTransProv (query, cursorName);
			break;
			default:
				elog(ERROR,
						"unkown type of contribution semantics %d",
						ContributionType(query));
			break;
		}

		/* reset into and utiltiy and set rewritten query in RTE if present */
		if (rteQuery != NULL)
			rteQuery->subquery = query;

		if (into)
			query->intoClause = into;

		if (utility)
			query->utilityStmt = utility;

		/* create correct eref for rewritten query */
		if (rteQuery)
			correctRTEAlias(rteQuery);
	}
	// if not, test if one of the subqueries is marked for provenance rewrite
	else
	{
		foreach (lc, query->rtable)
		{
			rtEntry = (RangeTblEntry *) lfirst(lc);
			if (rtEntry->rtekind ==  RTE_SUBQUERY)	// is subquery?
			{
				traverseQueryTree (rtEntry, rtEntry->subquery, cursorName);
			}
		}
	}

	return query;
}



/*
 * Rewrite a query node marked for provenance rewrite using influence contribution semantics (I-CS) data-data provenance.
 */

Query *
rewriteQueryNode (Query * query)
{
	/* is an aggregate query? */
	if (query->hasAggs)
		query = rewriteAggregateQuery (query);

	/* is a set operation? */
	else if (query->setOperations != NULL)
		query = rewriteSetQuery (query);

	/* SPJ */
	else
		query = rewriteSPJQuery (query);

	LOGNODE(query, "rewritten query tree (influence contribution)");

	return query;
}

/*
 * Rewrite a query node using copy contribution semantics (C-CS) data-data provenance.
 */

Query *
rewriteQueryNodeCopy (Query *query)
{
	if (query->hasAggs)
		query = rewriteCopyAggregateQuery (query);
	else if (query->setOperations != NULL)
		query = rewriteCopySetQuery (query);
	else
		query = rewriteSPJQueryCopy (query);

	LOGNODE(query, "rewritten query tree (copy contribution semantics)");

	return query;
}
