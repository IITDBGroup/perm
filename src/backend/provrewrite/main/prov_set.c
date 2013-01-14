/*-------------------------------------------------------------------------
 *
 * prov_set.c
 *	  PERM C - I-CS provenance rewrites for set queries.
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/prov_set.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
 *
 * NOTES
 *	Set operation rewrite generates a new top query node that joins the original set operation results
 *	with the rewritten inputs of the set operations (see ICDE'09 paper for explanation of the algebraic
 *	rewrite rules that determine this rewrite).
 *
 *-------------------------------------------------------------------------
 */

//TODO the whole join generation should make use of the helper method
 /* instead of reproducing similar functionality here.
 */

#include "postgres.h"
#include "catalog/pg_operator.h"		// pg_operator system table for operator lookup
#include "nodes/makefuncs.h"			// needed to create new nodes
#include "nodes/print.h"				// pretty print node (trees)
#include "optimizer/clauses.h"
#include "parser/parse_expr.h"			// expression transformation used for expression type calculation
#include "parser/parse_oper.h"			// for lookup of operators
#include "parser/parsetree.h"
#include "parser/parse_relation.h"
#include "utils/guc.h"
#include "utils/syscache.h"				// used to release heap tuple references
#include "provrewrite/provlog.h"
#include "provrewrite/provattrname.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_sublink_util_analyze.h"
#include "provrewrite/prov_sublink_util_mutate.h"
#include "provrewrite/prov_sublink_util_search.h"
#include "provrewrite/prov_spj.h"
#include "provrewrite/provrewrite.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_set_util.h"
#include "provrewrite/prov_set.h"
#include "provrewrite/prov_copy_map.h"

/* macro defs */
#define GET_SETOP(query) \
	(((SetOperationStmt *) query->setOperations)->op)

/* Function declarations */
static void resetOldVarNoAndAttrno (List *tlist);
static void addSetSubqueryRTEs (Query *top, Query *orig);
static void createNullSetDiffProvAttrs(Query *newTop, Query *query,
		List **pList);
static void rewriteUnionWithWLCS (Query *query);
static void addDummyProvAttrs (RangeTblEntry *rte, List *subProv, int pos);
static void adaptSetProvenanceAttrs (Query *query);
static void rewriteSetRTEs (Query *newTop);
static void makeRTEVars (RangeTblEntry *rte, RangeTblEntry *newRte,
		Index rtindex);
static void createJoinsForSetOp (Query *query, SetOperation setType);
static void createSetDiffJoin (Query *query);
static void createRTEforJoin (Query *query, JoinExpr *join, Index leftIndex,
		Index rightIndex);

/*
 *	Rewrites a set operation query node . Depending on the type of set
 *	operations this query node represents, it can be rewritten in one pass or
 *	has to be restructured into several query node before applying the
 *	rewrites. E.g., if the set operations are only union no restructuring is
 *	necessary (this is derived from the fact that applying the algebraic
 *	rewrite rules on which derived Perm is based to a list of unions results
 *	in a fixed form). See ICDE'09 paper for details.
 */

Query *
rewriteSetQuery (Query * query)
{
	Query *newTop;
	List *pList;
	int numSubs;
	int *context;
	Query *orig;

	pList = NIL;
	orig = copyObject(query);

	/* remove dummy RTEs produced by postgres view expansion */
	removeDummyRewriterRTEs(query);

	/* restructure set operation tree if necessary */
	replaceSetOperationSubTrees (query, query->setOperations,
			&(query->setOperations),
			GET_SETOP(query));

	/* check if the alternative union semantics is activated and we are
	 * rewriting a union node. If so use all the join stuff falls apart and we
	 * just have to rewrite the original query */
	if (prov_use_wl_union_semantics
			&& GET_SETOP(query) == SETOP_UNION)
	{
		rewriteUnionWithWLCS (query);

		return query;
	}

	/* create new top query node */
	newTop = makeQuery();
	addSetSubqueryRTEs(newTop, query);
	newTop->targetList = copyObject(query->targetList);
	resetOldVarNoAndAttrno(newTop->targetList);
	newTop->intoClause = copyObject(query->intoClause);
	SetSublinkRewritten(newTop, true);

	/* add original query as first range table entry */
	addSubqueryToRTWithParam (newTop, orig, "originalSet", true, ACL_NO_RIGHTS, false);

	/* rewrite the subqueries used in the set operation */
	rewriteSetRTEs (newTop);
	numSubs = list_length(newTop->rtable) - 1;

	LOGNODE(newTop, "replaced different set ops and rewrite sub queries");

	/* correct alias of rewritten subqueries */
	correctSubQueryAlias (newTop);

	/* join the original query with all rewritten subqueries */
	createJoinsForSetOp (newTop, GET_SETOP(query));

	/* add provenance attributes from the subqueries to the top query target list */
	if (!prov_use_wl_union_semantics &&  ((SetOperationStmt *)
			query->setOperations)->op == SETOP_EXCEPT)
		pList = addProvenanceAttrs (newTop, createIntList(2,numSubs,1), pList, false);
	else
		pList = addProvenanceAttrs (newTop, createIntList(2,numSubs,2), pList, false);

	if (prov_use_wl_union_semantics && ((SetOperationStmt *)
			query->setOperations)->op == SETOP_EXCEPT)
		createNullSetDiffProvAttrs(newTop, query, &pList);

	push(&pStack, pList);

	/* increase sublevelsup of vars in case we are rewriting in an sublink query */
	context = (int *) palloc(sizeof(int));
	*context = -1;
	increaseSublevelsUpMutator((Node *) orig, context);
	pfree(context);

	return newTop;
}

/*
 * Walk through a target list of a set operation (simple Vars) and reset the
 * varnoold and varoattno.
 */

static void
resetOldVarNoAndAttrno (List *tlist)
{
	TargetEntry *te;
	Var *var;
	ListCell *lc;

	foreach(lc, tlist) {
		te = (TargetEntry *) lfirst(lc);
		Assert(IsA(te->expr, Var));
		var = (Var *) te->expr;

		var->varnoold = var->varno;
		var->varoattno = var->varattno;
	}
}

/*
 *
 */

static void
addSetSubqueryRTEs (Query *top, Query *orig)
{
	SetOperationStmt *setOp = (SetOperationStmt *) orig->setOperations;
	List *rte = NIL;

	if (!prov_use_wl_union_semantics || setOp->op != SETOP_EXCEPT)
	{
		top->rtable = copyObject(orig->rtable);
		return;
	}

	findSetOpRTEs(orig->rtable, (Node *) setOp->larg, &rte, NULL);
	top->rtable = copyObject(rte);
}


/*
 *
 */

static void
createNullSetDiffProvAttrs(Query *newTop, Query *query, List **pList)
{
	List *rtes = NIL;
	List *provRtes = NIL;
	List *attrNames;
	List *vars;
	Node *rightSet;
	ListCell *lc;
	ListCell *innerLc;
	ListCell *attrLc;
	ListCell *varLc;
	RangeTblEntry *rte;
	RangeTblEntry *provRte;
	TargetEntry *newTe;
	Node *nullConst;
	Value *colName;
	Var *var;
	char *provName;
	Index curResno = list_length(newTop->targetList);

	/* get all base relations under right input of set difference and
	 * generate NULL provenance attributes for them.
	 */
	rightSet = ((SetOperationStmt *) query->setOperations)->rarg;
	findSetOpRTEs(query->rtable, rightSet, &rtes, NULL);

	/* for each rte accessed by the subtree under the right input get the
	 * base relations accessed by this rte and create null constants as
	 * provenance attributes for them. */
	foreach(lc, rtes)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		provRtes = NIL;

		findBaseRelationsForProvenanceRTE(rte, &provRtes);

		foreach(innerLc, provRtes)
		{
			provRte = (RangeTblEntry *) lfirst(innerLc);
			attrNames = NIL;
			vars = NIL;

			/* check if is an RTE with specified provenance attributes */
			if (provRte->provAttrs != NIL)
			{
				List *tes;

				rewriteRTEwithProvenance(1, provRte);
				tes = pop(&pStack);

				foreach(attrLc, tes)
				{
					newTe = (TargetEntry *) lfirst(attrLc);
					var = (Var *) newTe->expr;

					newTe->resno = ++curResno;
					newTe->expr = (Expr *) makeNullConst(var->vartype,
							var->vartypmod);
					newTop->targetList = lappend(newTop->targetList, newTe);
					*pList = lappend(*pList, newTe);
				}
			}
			else
			{
				/* increment rel ref counter */
				getQueryRefNum(provRte, true);

				/* get RTE attributes */
				expandRTEWithParam(provRte, 1, 0, false, false, &attrNames, &vars);

				/* generate provenance attributes */
				forboth(attrLc, attrNames, varLc, vars)
				{
					colName = (Value *) lfirst(attrLc);
					var = (Var *) lfirst(varLc);

					nullConst = (Node *) makeNullConst(var->vartype,
							var->vartypmod);
					provName = createProvAttrName(provRte, strVal(colName));

					newTe = makeTargetEntry((Expr *) nullConst, ++curResno,
							provName, false);
					newTop->targetList = lappend(newTop->targetList, newTe);
					*pList = lappend(*pList, newTe);
				}
			}
		}
	}
}



/*
 *
 */

static void
rewriteUnionWithWLCS (Query *query)
{
	List *subProvs = NIL;
	List *pList = NIL;
	RangeTblEntry *rte;
	ListCell *lc;
	int i;

	// rewrite the range table entries and fetch their provenance attrs
	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		rte->subquery = rewriteQueryNode(rte->subquery);
		subProvs = lappend(subProvs, linitial(pStack));
	}

	// add projections on NULL to each subquery to generate
	// the same schema for each subquery.
	foreachi(lc, i, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		addDummyProvAttrs(rte, subProvs, i);
	}

	correctSubQueryAlias(query);

	// push provenance list on stack and add provenance attrs to query
	pList = addProvenanceAttrsForRange(query, 1, list_length(query->rtable) + 1, pList);
	push(&pStack, pList);

	// adapt set operation query provenance attributes
	adaptSetProvenanceAttrs(query);
}

/*
 *
 */

static void
addDummyProvAttrs (RangeTblEntry *rte, List *subProv, int pos)
{
	List *result = NIL;
	Query *query;
	int i;
	ListCell *lc;
	ListCell *innerLc;
	List *curProvList;
	List *ownProv;
	TargetEntry *newTe;
	TargetEntry *te;
	Expr *newExpr;
	int numSelfProvAttrs;
	int curResno;

	query = rte->subquery;

	// get number of provenance attributes for the rte
	// and remove the provenance attributes from target list
	curProvList = (List *) list_nth(subProv, pos);
	numSelfProvAttrs = list_length(curProvList);
	ownProv = list_copy_tail(query->targetList,
							list_length(query->targetList) - numSelfProvAttrs);
	removeAfterPos(query->targetList, list_length(query->targetList) - numSelfProvAttrs);
	result = query->targetList;
	curResno = list_length(result) + 1;

	foreachi(lc,i,subProv)
	{
		curProvList = (List *) lfirst(lc);

		// for own provenance use the original provenance attrs
		if (i == pos)
		{
			foreach(innerLc, ownProv)
			{
				te = (TargetEntry *) lfirst(innerLc);
				te->resno = curResno++;
				result = lappend(result, te);
			}
		}
		// create null constants for provenance of other subqueries
		else
		{
			foreach(innerLc, curProvList)
			{
				te = (TargetEntry *) lfirst(innerLc);

				newExpr = (Expr *) makeNullConst (exprType ((Node *) te->expr),
						exprTypmod ((Node *) te->expr));
				newTe = makeTargetEntry(newExpr, curResno++, te->resname, false);

				result = lappend(result, newTe);
			}
		}
	}

	query->targetList = result;

	list_free(ownProv);
}


/*
 *
 */

static void
adaptSetProvenanceAttrs (Query *query)
{
	TargetEntry *te;
	Var *var;
	ListCell *lc;
	int curAttno = 1;
	List *colTypes = NIL;
	List *colTypmods = NIL;

	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);
		var = (Var *) te->expr;
		var->varno = 1;
		var->varattno = curAttno++;
		colTypes = lappend_oid(colTypes, var->vartype);
		colTypmods = lappend_int(colTypmods, var->vartypmod);
	}

	adaptSetStmtCols((SetOperationStmt *) query->setOperations,
			colTypes, colTypmods);
}


/*
 * Rewrites the range table entries of a set query. Basically loops over each
 * RTE entries leaving out the first one which is the original set operation
 * query.
 */

static void
rewriteSetRTEs (Query *newTop)
{
	ListCell *lc;
	RangeTblEntry *rte;
	Query *query;

	for(lc = newTop->rtable->head->next; lc != NULL; lc = lc->next)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		query = rewriteQueryNode (rte->subquery);
		rte->subquery = query;
		rte->inFromCl = true;
		rte->requiredPerms = ACL_SELECT;
	}
}

/*
 * Create the joins for a rewritten set operation node. We have to join the
 * original query (first RTE of the new top query node introduced by the
 * rewrite rules) with each rewritten input of the set operation (all the other
 * range table entries of the new top node).
 */

static void
createJoinsForSetOp (Query *query, SetOperation setType)
{
	RangeTblRef *rtRef;
	RangeTblEntry *rte;
	Node *curJoin;
	List *todo;
	ListCell *lc;
	JoinExpr *newJoin;
	JoinType joinType;
	int rtableLength;
//	int i, origRtableLength;

	/* define join type to use in join expression */
	switch (setType)
	{
		case SETOP_UNION:
			joinType = JOIN_LEFT;
		break;
		case SETOP_INTERSECT:
			joinType = JOIN_INNER;
		break;
		/* set difference is a special case with two types of join ops but only
		 * two subqueries */
		case SETOP_EXCEPT:
			createSetDiffJoin (query);
			return;
		default:
			elog(ERROR,
					"Unkown set operation type: %d",
					setType);
		break;
	}

	/* set original rtable to first two item and todo list to the remainder */
	todo = list_copy(query->rtable);
	todo = list_delete_first(list_delete_first(todo));
	query->rtable = list_truncate(query->rtable, 2);

	/* create the leaf join */
	rtableLength = list_length(query->rtable);

	newJoin = (JoinExpr *) makeNode(JoinExpr);
	newJoin->rtindex = rtableLength + 1;
	newJoin->jointype = joinType;

	MAKE_RTREF(rtRef, 1);
	newJoin->larg = (Node *) rtRef;
	MAKE_RTREF(rtRef, 2);
	newJoin->rarg = (Node *) rtRef;

	createRTEforJoin(query, newJoin, 0, 1);
	createSetJoinCondition (query, newJoin, 0, 1, false);
	curJoin = (Node *) newJoin;

	/* create the joins with each rewritten set operation input */
	foreach(lc, todo)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		query->rtable = lappend(query->rtable, rte);
		rtableLength = list_length(query->rtable);

		newJoin = (JoinExpr *) makeNode(JoinExpr);
		newJoin->jointype = joinType;
		newJoin->rtindex = rtableLength + 1;
		newJoin->larg = curJoin;

		MAKE_RTREF(rtRef, rtableLength);
		newJoin->rarg = (Node *) rtRef;

		/* create join range table entry */
		createRTEforJoin(query, newJoin, rtableLength - 2, rtableLength - 1);

		/* create join condition */
		createSetJoinCondition (query, newJoin, 0, rtableLength - 1, false);

		/* create join range table entry */
		curJoin = (Node *) newJoin;
	}

	list_free(todo);

//	/* create the leaf join */
//	origRtableLength = list_length(query->rtable);
//	rtableLength = list_length(query->rtable);
//
//	newJoin = (JoinExpr *) makeNode(JoinExpr);
//	newJoin->rtindex = rtableLength + 1;
//	newJoin->jointype = joinType;
//
//	MAKE_RTREF(rtRef, 1);
//	newJoin->larg = (Node *) rtRef;
//
//	MAKE_RTREF(rtRef, 2);
//	newJoin->rarg = (Node *) rtRef;
//
//	createRTEforJoin(query, newJoin, 0, 1);
//	createSetJoinCondition (query, newJoin, 0, 1, false);
//	curJoin = (Node *) newJoin;
//
//
//	/* add a join for each range table entry */
//	for (i = 2; i < origRtableLength ;i++)
//	{
//		/* create a new join expression and add join expression created in last step
//		 * as right child. Left child is the current range table entry
//		 */
//		newJoin = (JoinExpr *) makeNode(JoinExpr);
//		newJoin->jointype = joinType;
//		newJoin->rtindex = rtableLength + 2;
//
//		newJoin->larg = curJoin;
//
//		MAKE_RTREF(rtRef,i + 1);
//		newJoin->rarg = (Node *) rtRef;
//
//		/* create join range table entry */
//		createRTEforJoin(query, newJoin, rtableLength, i);
//
//		/* create join condition */
//		createSetJoinCondition (query, newJoin, 0, i, false);
//
//		/* create join range table entry */
//		curJoin = (Node *) newJoin;
//		rtableLength++;
//	}

	/* set top level join as jointree of top query */
	query->jointree->fromlist = list_make1(curJoin);
}

/*
 * Creates the joins for a rewritten set difference operation. The algebraic
 * rewrite rules dictate that each set diff operation has to be placed on its
 * own in a query node. Therefore, we only have to join the original query
 * (first range table entry) with its two rewritten inputs.
 */

static void
createSetDiffJoin (Query *query)
{
	JoinExpr *innerJoin;
	JoinExpr *outerJoin;
	RangeTblRef *rtRef;
	bool all;
	Query *orig;

	orig = ((RangeTblEntry *) linitial(query->rtable))->subquery;
	all = ((SetOperationStmt *) orig->setOperations)->all;

	/* create inner join node */
	innerJoin = (JoinExpr *) makeNode(JoinExpr);
	innerJoin->rtindex = list_length(query->rtable) + 1;
	innerJoin->jointype = JOIN_INNER;

	MAKE_RTREF(rtRef, 1);
	innerJoin->larg = (Node *) rtRef;

	MAKE_RTREF(rtRef, 2);
	innerJoin->rarg = (Node *) rtRef;

	createRTEforJoin(query, innerJoin, 0, 1);
	createSetJoinCondition (query, innerJoin, 0, 1, false);

	if (prov_use_wl_union_semantics)
	{
		query->jointree->fromlist = list_make1(innerJoin);
		return;
	}

	/* create outer join node */
	outerJoin = (JoinExpr *) makeNode(JoinExpr);
	outerJoin->rtindex = 5;
	outerJoin->jointype = JOIN_LEFT;

	outerJoin->larg = (Node *) innerJoin;

	MAKE_RTREF(rtRef, 3);
	outerJoin->rarg = (Node *) rtRef;

	createRTEforJoin(query, outerJoin, 3, 2);
	if (all)
		createSetJoinCondition (query, outerJoin, 0, 2, true);
	else
		outerJoin->quals =  (Node *) makeBoolConst(true, false);

	/* set top level join as jointree of top query */
	query->jointree->fromlist = list_make1(outerJoin);
}




/*
 * Creates an range table for a new join introduced by the set rewrite rules.
 */

static void
createRTEforJoin (Query *query, JoinExpr *join, Index leftIndex, Index rightIndex)
{
	RangeTblEntry *newRte;
	RangeTblEntry *rte;

	/* create new RTE */
	newRte = makeRte (RTE_JOIN);
	newRte->alias = NULL;
	newRte->eref->aliasname = pstrdup("unnamed_join");
	newRte->jointype = join->jointype;
	newRte->requiredPerms = ACL_NO_RIGHTS;
	/* add join var aliases for left */
	rte = (RangeTblEntry *) list_nth(query->rtable, leftIndex);
	makeRTEVars (rte, newRte, leftIndex);

	/* add join var aliases for right */
	rte = (RangeTblEntry *) list_nth(query->rtable, rightIndex);
	makeRTEVars (rte, newRte, rightIndex);

	/* append join rte to range table */
	query->rtable = lappend(query->rtable, newRte);
}

/*
 * Create a list of vars for a RTE. Helper method used by createRTEforJoin to
 * generate the joinaliasvars.
 */

static void
makeRTEVars (RangeTblEntry *rte, RangeTblEntry *newRte, Index rtindex)
{
	ListCell *lc;
	ListCell *nameLc;
	TargetEntry *te;
	Var *var;
	Value *name;
	Alias *alias;
	Index curAttno;
	int counter;

	alias = newRte->eref;
	/* is subquery RTE */
	if (rte->rtekind == RTE_SUBQUERY) {
		counter = 1;
		foreach (lc, rte->subquery->targetList)
		{
			te = (TargetEntry *) lfirst(lc);
			var = makeVar (rtindex + 1,
					te->resno,
					exprType ((Node *) te->expr),
					exprTypmod ((Node *) te->expr),
					0);
			newRte->joinaliasvars = lappend(newRte->joinaliasvars, var);
			alias->colnames =  lappend(alias->colnames, makeString(te->resname));
			counter++;
		}
	}
	/* is join RTE */
	else {
		curAttno = 1;
		forboth(lc, rte->joinaliasvars, nameLc, rte->eref->colnames)
		{
			var = (Var *) lfirst(lc);
			name = (Value *) lfirst(nameLc);
			var = makeVar (rtindex + 1,
						curAttno,
						var->vartype,
						var->vartypmod,
						0);
			newRte->joinaliasvars = lappend(newRte->joinaliasvars, var);
			alias->colnames =  lappend(alias->colnames, copyObject(name));
			curAttno++;
		}
	}
}







