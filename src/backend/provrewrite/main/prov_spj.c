/*-------------------------------------------------------------------------
 *
 * prov_spj.c
 *	  PERM C - I-CS data-data provenance rewrites for SPJ queries.
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/prov_spj.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "nodes/makefuncs.h"			// needed to create new nodes
#include "parser/parsetree.h"
#include "parser/parse_expr.h"
#include "parser/parse_relation.h"
#include "parser/parse_oper.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/prov_spj.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/provlog.h"
#include "provrewrite/provstack.h"
#include "provrewrite/provattrname.h"
#include "provrewrite/prov_sublink.h"

/* Function declarations */
static void rewriteBaseRelation (int rtindex, RangeTblEntry *rte, Query *query);
static void computeJoinPAttrs(List **pAttrs, Node *joinChild, Query *query, List *subList);
static bool checkForNTupleDupConstructs (Query *query);

/*
 * Rewrite a SPJ query node. First, handle the LIMIT clause if its present,
 * then rewrite sublink queries. Finally do the actual rewrite of the SPJ
 * query.
 */

Query *
rewriteSPJQuery (Query *query)
{
	List *pList;
	List *subList;
	Index maxRtindex;

	pList = NIL;
	subList = NIL;
	maxRtindex = list_length(query->rtable);

	/* if the query has a limit clause we have to preserve the original
	 * query and join it with the rewritten query without limit. This is
	 * necessary because we don't know beforehand how many duplicates of
	 * normal result tuples we will need to "fit" the provenance in.
	 */
	if (checkLimit(query))
		return handleLimit(query);

	/*
	 * if query has sublinks we have to rewrite the sublink queries first.
	 */
	if (query->hasSubLinks)
		query = rewriteSublinks(query, &subList);

	/* no sublinks, do normal SPJ rewrite */
	if (!IsSublinkRewritten(query))
		rewriteSPJrestrict (query, &subList, maxRtindex);

	return query;
}

/*
 * Do the actual rewrite of an SPJ query. For an SPJ query the only thing to do
 * is to add the provenance attributes for all subquery and base relation
 * entries in the range table to the target list of the SPJ query (we record
 * for which RT entries we have provenance in subList). First, the RT entries
 * are rewritten. Second, RTE aliases and join RTEs are adapted to consider the
 * provenance RT entries. Finally, the provenance attributes are added to the
 * target list of the SPJ query and are pushed on pStack to be used by callers
 * of this method.
 */

Query *
rewriteSPJrestrict (Query *query, List **subList, Index maxRtindex)
{
	List *pList;

	pList = NIL;

	/* rewrite the RTEs of the query */
	rewriteRTEs(query, subList, maxRtindex);

	/* correct eref of subqueries */
	correctSubQueryAlias (query);

	/* correct join RTEs (add provenance attributes) */
	correctJoinRTEs (query, subList);

	/* add provenance attributes of sub queries to targetlist */
	pList = addProvenanceAttrs (query, *subList, pList);

	/* push list of provenance attributes to pStack */
	push(&pStack, pList);

	/* if a distinct clause is present include provenance attributes
	 * otherwise we would incorrectly omit duplicate result tuples with
	 * different provenance
	 */

	if (query->distinctClause != NIL)
		rewriteDistinctClause (query);

	return query;
}

/*
 *	Rewrites the range table entries from 0 to maxRtindex of a query.
 */

List *
rewriteRTEs (Query *query, List **subList, Index maxRtindex)
{
	ListCell *lc;
	RangeTblEntry *rte;
	Index rtindex;
	int i;

	/* if none of the RTEs should be rewritten, return to caller */
	if (maxRtindex == 0)
		return *subList;

	rtindex = 1;

	/* Walk through range table and rewrite rte's if some new rte's were added during
	 * sublink rewrite we ignore them.
	 */
	for(lc = query->rtable->head, i = 0; lc != NULL && i < maxRtindex; lc = lc->next, i++)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		if (strcmp("*NEW*", rte->eref->aliasname) != 0
					&& strcmp("*OLD*", rte->eref->aliasname) != 0
					&& !ignoreRTE(rte))
		{
			/* rte is a stored provenance query */
			if (rte->provAttrs != NIL)
				rewriteRTEwithProvenance (rtindex, rte);

			/* rte is base relation */
			else if (rte->rtekind == RTE_RELATION || rte->isProvBase)
				rewriteBaseRelation (rtindex, rte, query);

			/* rte is subquery */
			else if (rte->rtekind == RTE_SUBQUERY)
				rte->subquery = rewriteQueryNode (rte->subquery);

			/* rte is not a join RTE so add it to subList*/
			if (rte->rtekind != RTE_JOIN)
				*subList = lappend_int(*subList, rtindex);

		}

		rtindex++;
	}

	return *subList;
}

/*
 * Rewrites a RTE representing a base relation. In principle a base relation access is rewritten as a projection
 * that duplicates the attributes of the base relation as provenance attributes. This means a base relation access
 * would be replaced by a subquery that implements this projection. To avoid this unnecessary extra query node we
 * just generate the provenance attribute of the base relation and push them on the pStack. The rewrite code
 * that rewrites the parent query node of the base relation access will then use this provenance attributes to implement
 * the projection. E.g.:
 * 			- SELECT PROVENANCE * FROM r is not rewritten into SELECT * FROM (SELECT r.a, r.a AS prov_r_a FROM r), but
 * 			into SELECT r.a, r.a AS prov_r_a FROM r).
 */

static void
rewriteBaseRelation (int rtindex, RangeTblEntry *rte, Query *query)
{
	List *vars;
	ListCell *var;
	List *names;
	ListCell *name;
	List *pList;
	TargetEntry *te;
	Expr *expr;
	char *namestr;
	AttrNumber curResno;

	pList = NIL;

	/* create Var nodes for all attributes of the relation */
	expandRTEWithParam(rte, rtindex, 0, false, false, &names, &vars);

	/* get number of TE s */
	curResno = 0;

	/* increment reference counter for relation used in provnenace attribute names */
	getQueryRefNum (rte, true);

	/* create a list with a new target entry for each attribute of the base relation */
	forboth (var, vars, name, names)
	{
		curResno++;
		expr = (Expr *) lfirst(var);
		namestr = strVal(lfirst(name));

		te = makeTargetEntry(expr, curResno,
						createProvAttrName  (rte, namestr),
						false);
		te->resorigtbl = rte->relid;
		te->resorigcol = ((Var *) expr)->varoattno;

		pList = lappend(pList, te);
	}

	/* push the provenance attrs on pStack */
	push(&pStack, pList);

	/* if the baseRelStack is activated push a RTE for this base relation on the baseRelStack */
	if (baseRelStackActive)
		push(&baseRelStack, copyObject(rte));
}

/*
 * Rewrites a RTE that includes provenance attributes. To be more precise for
 * which a user has given a list of provenance attributes by appending the
 * PROVENANCE (Attribute List) clause to the from clause item represented by
 * this RTE.
 */

void rewriteRTEwithProvenance (int rtindex, RangeTblEntry *rte)
{
	ListCell *lc;
	List *vars;
	ListCell *var;
	List *names;
	ListCell *name;
	Value *provattr;
	Value *attr;
	bool found;
	List *pList;
	TargetEntry *te;
	Var * attrVar;

	pList = NIL;

	/* create Var nodes for all attributes of the RTE */
	if (rte->rtekind == RTE_SUBQUERY)
	{
		/* if the RTE subquery is a provenance query we rewrite this query and
		 * return */
		if (IsProvRewrite(rte->subquery))
		{
			rte->subquery = rewriteQueryNode(rte->subquery);
			return;
		}

		/* subquery is not marked for provenance rewrite */
		vars = NIL;
		names = NIL;

		foreach(lc, rte->subquery->targetList)
		{
			te = (TargetEntry *) lfirst(lc);

			attr = makeString(te->resname);
			names = lappend(names, attr);

			attrVar = makeVar(rtindex, te->resno, exprType((Node *) te->expr),
					exprTypmod((Node *) te->expr), 0);
			vars = lappend(vars, attrVar);
		}
	}
	else if (rte->rtekind == RTE_RELATION)
		expandRTEWithParam(rte, rtindex, 0, false, false, &names, &vars);
	else
		elog(ERROR,
				"PROVENANCE construct in FROM clause is only allowed"
				"for base relations and subqueries");



	/* walk through provAttrs and find correspoding var */
	foreach(lc, rte->provAttrs)
	{
		provattr = (Value *) lfirst(lc);

		found = false;
		forboth(name, names, var, vars)
		{
			attr = (Value *) lfirst(name);
			attrVar = (Var *) lfirst(var);

			if (strcmp(attr->val.str, provattr->val.str) == 0)
			{
				found = true;
				te = makeTargetEntry((Expr *) attrVar, attrVar->varattno,
						createExternalProvAttrName(attr->val.str), false);
				pList = lappend(pList, te);
			}
		}
		if (!found)
			elog(ERROR,
					"Provenance attribute %s given in the "
					"PROVENANCE attribute list is not found in %s",
					provattr->val.str,
					getAlias(rte));
	}

	/* push pList on stack */
	push (&pStack, pList);
}

/*
 * Adds provenance attributes to the join RTEs of a query by fetching them from the RTE's joined
 * by the join.
 */

void
correctJoinRTEs (Query *query, List **subList)
{
	List *fromList;
	ListCell *lc;
	List *pAttrs;
	JoinExpr *joinExpr;

	fromList = query->jointree->fromlist;

	foreach(lc, fromList)
	{
		if (IsA(lfirst(lc),JoinExpr))
		{
			pAttrs = NIL;

			/* adapt range table entries for joins */
			joinExpr = (JoinExpr *) lfirst(lc);
		 	computeJoinPAttrs(&pAttrs, (Node *) joinExpr, query, *subList);
		}
	}
}

/*
 * Walk through a join expr tree recursively and for each join expr adds the provenance
 * attributes of the RTEs referenced by the join to the join's RTE.
 */

static void
computeJoinPAttrs(List **pAttrs, Node *joinChild, Query *query, List *subList)
{
	RangeTblEntry *rte;
	JoinExpr *joinExpr;
	Index rteRef;
	Index counter;
	ListCell *lc;
	List *pList;
	TargetEntry *te;
	TargetEntry *newTe;
	Var *var;
	Var *newVar;

	pList = NIL;

	if (IsA(joinChild, JoinExpr))
	{
		joinExpr = (JoinExpr *) joinChild;

		computeJoinPAttrs(&pList, joinExpr->larg, query, subList);
		computeJoinPAttrs(&pList, joinExpr->rarg, query, subList);

		rteRef = joinExpr->rtindex;
		rte = rt_fetch(rteRef, query->rtable);

		counter = list_length(rte->joinaliasvars) + 1;
		foreach(lc,pList)
		{
			te = (TargetEntry *) lfirst(lc);
			var = (Var *) te->expr;

			/* adapt var */
			var->varattno = te->resno;

			/*
			 * make new Var and TE for this join. TE is just used to transfer the
			 * attribute name.
			 */
			newVar = copyObject(var);
			newVar->varattno = counter;
			newVar->varno = rteRef;

			newTe = makeTargetEntry(((Expr *) newVar), newVar->varattno, te->resname, false);

			/* add provenance attribute Var to joinaliasvars and name to eref->colnames */
			rte->joinaliasvars = lappend(rte->joinaliasvars, var);
			rte->eref->colnames = lappend(rte->eref->colnames, makeString(te->resname));

			counter++;

			*pAttrs = lappend(*pAttrs, newTe);
		}
	}
	else if (IsA(joinChild, RangeTblRef))
	{
		rteRef = ((RangeTblRef *) joinChild)->rtindex;

		counter = 0;

		foreach(lc, subList)
		{
			if (lfirst_int(lc) == rteRef)
				pList = (List *) list_nth(pStack, (list_length(subList) - 1) - counter);
			counter++;
		}

		foreach(lc, pList)
		{
			te = (TargetEntry *) lfirst(lc);

			/* copy te so we can savely change it */
			te = copyObject(te);
			var = (Var *) te->expr;
			var->varno = rteRef;

			*pAttrs = lappend(*pAttrs, te);
		}
	}
	else
		elog(ERROR,
				"Found an unexpected node (type %d) in a join tree!",
				joinChild->type);
}

/*
 * Adds the provenance attributes of an query to an existing distinct clause
 */

void
rewriteDistinctClause (Query *query)
{
	List *pList;
	ListCell *lc;
	TargetEntry *te;
	SortClause *sortClause;
	Index curGroupref;

	pList = (List *) linitial(pStack);

	/* find current maximal ressortgroupref value */
	curGroupref = 0;
	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);
		curGroupref = (te->ressortgroupref > curGroupref) ? te->ressortgroupref : curGroupref;
	}

	/* create a new SortClause entry for every provenance attribute of the query */
	foreach(lc, pList)
	{
		te = (TargetEntry *) lfirst(lc);
		sortClause = makeNode(SortClause);
		sortClause->sortop = ordering_oper_opid(exprType((Node *) te->expr));
		sortClause->nulls_first = false;

		if (te->ressortgroupref == 0)
			te->ressortgroupref = ++curGroupref;

		sortClause->tleSortGroupRef = te->ressortgroupref;

		query->distinctClause = lappend(query->distinctClause, sortClause);
	}

}

/*
 * Checks if an query contains an LIMIT clause and if this is the case check if we
 * have to join the rewritten with the original query to preserve the LIMIT semantics.
 */

bool
checkLimit (Query *query)
{
	if(!query->limitCount && !query->limitOffset)
		return false;

	return checkForNTupleDupConstructs(query);
}

/*
 * Checks if an query node contains constructs that can cause result tuple duplication.
 */

static bool
checkForNTupleDupConstructs (Query *query)
{
	ListCell *lc;
	RangeTblEntry *rte;
	bool result;

	if (query->distinctClause)
		return true;

	if (query->hasAggs || query->hasSubLinks)
		return true;

	result = false;

	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		/* subqueries have to be checked too */
		if (rte->rtekind == RTE_SUBQUERY)
			result = result || checkForNTupleDupConstructs(rte->subquery);

		/* We don't know how a table function is defined.
		 * To be safe we assume it does some nasty stuff.
		 */
		if (rte->rtekind == RTE_FUNCTION)
			result = true;
	}

	return result;
}



/*
 * Rewrites an LIMIT clause for an SPJ-query that might have more than one contributing
 * tuple from a base relation for a result tuple. To preserve the limit we have to join
 * the original query with the rewritten query on the original result attributes.
 */

Query *
handleLimit (Query *query)
{
	Query *newTopNode;
	Query *rewrittenQuery;
	ListCell *lc;
	TargetEntry *te;
	TargetEntry *newTe;
	Var *var;
	int i;
	List *attrsPos;

	JoinExpr *join;

	/* copy query and remove limit clause from copy */
	rewrittenQuery = copyObject(query);
	rewrittenQuery->limitCount = NULL;
	rewrittenQuery->limitOffset = NULL;

	/* rewrite query */
	rewrittenQuery = rewriteSPJQuery(rewrittenQuery);

	/* ceate new top node that joins original with rewritten
	 * query on the normal result attributes.
	 */
	newTopNode = makeQuery();

	/* create target list */
	foreachi(lc, i, rewrittenQuery->targetList)
	{
		te = (TargetEntry *) lfirst(lc);

		var = makeVar(2, i + 1, exprType((Node *) te->expr), exprTypmod((Node *) te->expr), 0); //CHECK sublevelsup
		newTe = makeTargetEntry((Expr *) var, i + 1, pstrdup(te->resname), te->resjunk);

		newTopNode->targetList = lappend(newTopNode->targetList, newTe);
	}

	/* create join */
	addSubqueryToRT (newTopNode, query, "originalQuery");
	addSubqueryToRT (newTopNode, rewrittenQuery, "rewrittenQuery");

	correctSubQueryAlias(newTopNode);

	attrsPos = listNthFirstInts(list_length(query->targetList), 0);
	join = createJoinOnAttrs(newTopNode, JOIN_INNER,1,2,attrsPos,attrsPos,true);

	newTopNode->jointree->fromlist = list_make1(join);

	recreateJoinRTEs(newTopNode);

	return newTopNode;
}
