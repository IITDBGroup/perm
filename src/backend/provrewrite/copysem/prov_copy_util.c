/*-------------------------------------------------------------------------
 *
 * copy/prov_copy_util.c
 *	  PERM C - Utility functions for copy contribution semantics
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/provrewrite.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "nodes/makefuncs.h"
#include "parser/parsetree.h"
#include "parser/parse_expr.h"
#include "optimizer/clauses.h"

#include "provrewrite/prov_util.h"
#include "provrewrite/provlog.h"
#include "provrewrite/provattrname.h"
#include "provrewrite/prov_copy_util.h"
#include "provrewrite/prov_copy_map.h"
#include "provrewrite/prov_copy_inclattr.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/provstack.h"

static List *getRTindicesForJoin (Node *joinTreeNode);
static void addProvAttrsForRelEntry (Query *query, CopyMapRelEntry *rel,
		CopyMapRelEntry *child);


/*
 * Replace all Var nodes in a condition with base RTE vars. This means all
 * references to join Vars are replaced by the base RTE var they represent.
 */

Node *
conditionOnRteVarsMutator (Node *node, Query *context)
{
	if (node == NULL)
		return NULL;

	if (IsA(node, Var))
	{
		Var *in;

		in = (Var *) node;

		return copyObject(resolveToRteVar(in, context));
	}

	return expression_tree_mutator(node, conditionOnRteVarsMutator,
			(void *) context);
}

/*
 * Resolve the join alias jars to the RTE vars they are derived from.
 */

List *
getRteVarsForJoin (Query *query, JoinExpr *join)
{
	List *result;
	ListCell *lc;
	RangeTblEntry *rte;
	Var *in;

	rte = rt_fetch(join->rtindex, query->rtable);

	foreach(lc, rte->joinaliasvars)
	{
		in = (Var *) lfirst(lc);
		result = lappend(result, copyObject(resolveToRteVar(in, query)));
	}

	return result;
}

/*
 * Return a list of all CopyRelEntries that are accessed in the RTE with index
 * "rtindex".
 */

List *
getCopyRelsForRtindex (Query *query, Index rtindex)
{
	CopyMap *map = GET_COPY_MAP(query);
	List *result = NIL;
	ListCell *lc;
	CopyMapRelEntry *rel;
	RangeTblEntry *rte;

	// get RTE
	rte = rt_fetch(rtindex, query->rtable);

	/* if it's a join get the indicies of the base rte accessed by the join and
	 * try for each CopyMapRelEntry if it belongs to one of these indices. */
	if (rte->rtekind == RTE_JOIN)
	{
		List *rtIndices;
		ListCell *innerLc;

		rtIndices = getRTindicesForJoin(getJoinTreeNode(query, rtindex));

		foreach(lc, map->entries)
		{
			rel = (CopyMapRelEntry *) lfirst(lc);

			foreach(innerLc, rtIndices)
			{
				if (rel->rtindex == lfirst_int(innerLc))
				{
					result = lappend(result, rel);
					break;
				}
			}
		}
	}

	foreach(lc, map->entries)
	{
		rel = (CopyMapRelEntry *) lfirst(lc);

		if (rel->rtindex == rtindex)
			result = lappend(result, rel);
	}

	return result;
}

/*
 * Get the range table indices for all range table entries accessed by a join.
 */

static List *
getRTindicesForJoin (Node *joinTreeNode)
{
	if (IsA(joinTreeNode, JoinExpr))
	{
		JoinExpr *join = (JoinExpr *) joinTreeNode;

		return list_concat(getRTindicesForJoin(join->larg),
				getRTindicesForJoin(join->rarg));
	}
	else
	{
		RangeTblRef *rtRef = (RangeTblRef *) joinTreeNode;

		return list_make1_int(rtRef->rtindex);
	}
}

/*
 * Create fake provenance attributes for a not rewritten query.
 */

List *
copyAddProvAttrForNonRewritten (Query *query)
{
	ListCell *lc, *innerLc;
	List *pList;
	List *targetList;
	CopyMapRelEntry *entry;
	CopyMapEntry *attr;
	TargetEntry *newTe;
	Expr *expr;
	Index curResno;

	pList = NIL;
	curResno = list_length(query->targetList) + 1;

	targetList = query->targetList;

	foreach(lc, GET_COPY_MAP(query)->entries)
	{
		entry = (CopyMapRelEntry *) lfirst(lc);

		/* create proveance attr for each base rel attr */
		foreach(innerLc, entry->attrEntries)
		{
			attr = (CopyMapEntry *) lfirst(innerLc);

			expr = (Expr *) makeNullConst(attr->baseRelAttr->vartype,
					attr->baseRelAttr->vartypmod);

			newTe = makeTargetEntry(expr, curResno, strdup(attr->provAttrName),
					false);

			/* append to targetList and pList */
			targetList = lappend (targetList, newTe);
			pList = lappend (pList, newTe);

			/* increase current resno */
			curResno++;
		}
	}

	query->targetList = targetList;

	return pList;
}


/*
 * Add provenance attributes to a query. The query should have been rewritten
 * beforehand. The provenance attributes are added for each range table entry.
 * If a RTE is rewritten than just add its provenance attributes like for
 * influence contribution semantics. Else add null-constants as Dummy
 * provenance attributes to generate a consistent output schema.
 */

void
copyAddProvAttrs (Query *query, List *subList)
{
	ListCell *lc;
	int origAttrNum = list_length(query->targetList);
	CopyMap *map = GET_COPY_MAP(query);
	CopyMapRelEntry *rel, *relChild;

	// for each rel entry get the provenance attributes from the child
	foreach(lc, map->entries)
	{
		rel = (CopyMapRelEntry *) lfirst(lc);
		relChild = rel->child;

		if (!rel->noRewrite)
			addProvAttrsForRelEntry(query, rel, relChild);
	}

	/* add copy map attributes */
	generateCopyMapAttributs(query, origAttrNum);
}

/*
 * Add provenance attributes for one CopyMapRelEntry.
 */

static void
addProvAttrsForRelEntry (Query *query, CopyMapRelEntry *rel,
		CopyMapRelEntry *child)
{
	ListCell *lc;
	Index curResno = list_length(query->targetList);
	TargetEntry *newTe, *te;
	Expr *expr;

	foreach(lc, child->provAttrs)
	{
		te = (TargetEntry *) lfirst(lc);

		/* create new TE */
		expr = (Expr *) makeVar (rel->child->rtindex,
					te->resno,
					exprType ((Node *) te->expr),
					exprTypmod ((Node *) te->expr),
					0);
		newTe = makeTargetEntry(expr, ++curResno, te->resname, false);

		/* adapt varno und varattno if referenced rte is used in a join-RTE */
		getRTindexForProvTE (query, (Var *) expr);

		query->targetList = lappend(query->targetList, newTe);
		rel->provAttrs = lappend(rel->provAttrs, newTe);
	}
}
