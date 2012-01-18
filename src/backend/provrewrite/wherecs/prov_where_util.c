/*-------------------------------------------------------------------------
 *
 * prov_where_util.c
 *	  PERM C 
 *
 * Portions Copyright (c) 2010 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $Perm: pgsql//SF_Perm/src/backend/provrewrite/wherecs/prov_where_util.c,v 1.542 Oct 28, 2010 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup.h"
#include "catalog/indexing.h"
#include "catalog/pg_class.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_type.h"
#include "fmgr.h"
#include "nodes/makefuncs.h"
#include "optimizer/clauses.h"
#include "parser/parse_coerce.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"

#include "provrewrite/provattrname.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_sublink_util_mutate.h"
#include "provrewrite/prov_where_util.h"


/* prototypes */
static List *makeAuxWithUnion (Query *rep);
static List *makeAuxQueriesWithUnion (Query *rep, WhereAttrInfo *attr,
		Var *inVar, List *rels);
static List *getAllNormalRels (void);
static List *makeAuxNoUnion (Query *rep);
static Query *makeNoUnionAuxQuery (Query *rep, Query *rewrRep,
		WhereAttrInfo *attr, Var *inVar);
static Query *addBaseRelationAnnotationAttrs (RangeTblEntry *rte, List *attrs);
static Node *getAnnotValueExpr (Oid reloid, char *attrName);

static void makeTeNamesUnique (Query *query);
static void addSubqueriesAsBase(Query *root, Query *sub, int subIndex,
		List *vars, List **subFroms);
static List *fetchAllRTEVars (Query *query, List **vars, int rtIndex);
static bool fetchAllRTEVarsWalker (Node *node, FetchVarsContext *context);
static bool addToVarnoWalker (Node *node, int *context);
static void adaptJoinTreeWithSubTrees (Query *query, List *subIndex,
		List *rtindexMap, List *joinIndex, List *subFroms);
static void adaptFromItem (Node **fromItem, List *subIndex, List *rtindexMap,
		List *joinIndex, List *subFroms, int joinOffset);

/*
 *
 */

List *
generateAuxQueries(List *reps, bool withUnion)
{
	ListCell *lc;
	Query *curRep;
	List *result = NIL;
	List *curAux;

	foreach(lc, reps)
	{
		curRep = (Query *) lfirst(lc);

		curAux = withUnion ? makeAuxWithUnion(curRep) : makeAuxNoUnion(curRep);

		result = list_concat(result, curAux);
	}

	return result;
}


/*
 *
 */

static List *
makeAuxWithUnion (Query *rep)
{
	ListCell *lc, *innerLc;
	List *result = NIL;
	WhereAttrInfo *attr;
	List *newAux;
	Var *inVar;
	List *rels;

	rels = getAllNormalRels ();

	/* generate annotation propagation for representatice and add
	 * representative to aux queries */
	generateAnnotBaseQueries(rep);
	result = lappend(result, rep);

	/* */
	foreach(lc, GET_WHERE_ATTRINFOS(rep))
	{
		attr = (WhereAttrInfo *) lfirst(lc);

		foreach(innerLc, attr->inVars)
		{
			inVar = (Var *) lfirst(innerLc);
			newAux = makeAuxQueriesWithUnion(rep, attr, inVar, rels);
			result = list_concat(result, newAux);
		}
	}

	return result;
}

/*
 *
 */

static List *
makeAuxQueriesWithUnion (Query *rep, WhereAttrInfo *attr, Var *inVar, List *rels)
{
	List *result = NIL;
	ListCell *lc, *innerLc;
	RangeTblEntry *rte, *newRte;
	List *relVars;
	Query *newQuery, *annotQuery;
	Var *relVar, *newVar;
	WhereAttrInfo *newattr;
	RangeTblRef *rtRef;
	Node *eqCond;
	int repCurIndex = list_length(rep->rtable) + 1;

	/* for each relation in the database */
	foreach(lc, rels)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		relVars = NIL;

		expandRTE(rte, repCurIndex, 0, false, NULL, &relVars);

		/* foreach attribute of a relation generate a auxiliary query */
		foreach(innerLc, relVars)
		{
			relVar = (Var *) lfirst(innerLc);

			newQuery = copyObject(rep);
			newattr = list_nth(GET_WHERE_ATTRINFOS(newQuery),
					attr->outVar->varattno - 1);

			newRte = copyObject(rte);
			annotQuery = addBaseRelationAnnotationAttrs(newRte,
					list_make1_int(relVar->varattno));
			addSubqueryToRT(newQuery, annotQuery, "auxjoinpartner");

			MAKE_RTREF(rtRef, list_length(newQuery->rtable));
			newQuery->jointree->fromlist = lappend(newQuery->jointree->fromlist, rtRef);

			newRte = (RangeTblEntry *) llast(newQuery->rtable);
			correctRTEAlias(newRte);

			/* add join condition to query qual */
			eqCond = createEqualityConditionForVars(copyObject(relVar),
					copyObject(inVar));

			if (newQuery->jointree->quals)
				newQuery->jointree->quals = (Node *) makeBoolExpr(AND_EXPR,
						list_make2(newQuery->jointree->quals, eqCond));
			else
				newQuery->jointree->quals = eqCond;

			/* correct WhereAttrInfo */
			newattr->inVars = list_make1(copyObject(relVar));
			newVar = makeVar(list_length(newQuery->rtable),
					list_length(newRte->eref->colnames), TEXTOID, -1, 0);
			newattr->annotVars = list_make1(newVar);

			/* add to result list */
			result = lappend(result, newQuery);
		}
	}

	return result;
}

/*
 *
 */

static const int namesp_key[4] = {
		ObjectIdAttributeNumber,
		0,
		0,
		0
};

static const int class_key[4] = {
	Anum_pg_class_relname,
	Anum_pg_class_relnamespace,
	0,
	0
};

static List *
getAllNormalRels (void)
{
	List *result = NIL;
	RangeTblEntry *rte;
	List *okNamespaces = NIL;
	Relation catRel;
	HeapTuple sysTuple;
	HeapScanDesc scandesc;
	Form_pg_namespace namespc;
	Form_pg_class relation;
	ScanKeyData key;
	int relNum = 0;

	/* scan for schemas (namespace) without pg_ - prefix and that are not
	 * information_schema */
	catRel = heap_open(NamespaceRelationId, AccessShareLock);

	ScanKeyInit(&key,
				Anum_pg_namespace_nspowner,
				BTGreaterStrategyNumber, F_OIDGT,
				ObjectIdGetDatum(InvalidOid));

	scandesc = heap_beginscan(catRel, SnapshotNow, 1, &key);

	while (HeapTupleIsValid(sysTuple =
			heap_getnext(scandesc, ForwardScanDirection)))
	{
		char *name;

		namespc = (Form_pg_namespace) GETSTRUCT(sysTuple);

		name = pstrdup(NameStr(namespc->nspname));
		if (strncmp(name, "pg_", 3) != 0
				&& strncmp(name, "information_schema", 18) != 0)
			okNamespaces = lappend_oid(okNamespaces, HeapTupleGetOid(sysTuple));
	}

	heap_endscan(scandesc);
	heap_close(catRel, AccessShareLock);


	/* scan relations and create RTEs for the ones that are not in one of the
	 * system namespaces.
	 */
	ScanKeyInit(&key,
				Anum_pg_class_relkind,
				BTEqualStrategyNumber, F_CHAREQ,
				CharGetDatum(RELKIND_RELATION));

	catRel = heap_open(RelationRelationId, AccessShareLock);
	scandesc = heap_beginscan(catRel, SnapshotNow, 1, &key);

	while(HeapTupleIsValid(sysTuple
			= heap_getnext(scandesc, ForwardScanDirection)))
	{
		Oid schema;
		Oid rel;

		relation = (Form_pg_class) GETSTRUCT(sysTuple);

		if (relation->relkind == 'r')
		{
			schema = relation->relnamespace;

			/* only relations from non-system schemas */
			if (list_member_oid(okNamespaces, schema))
			{
				Relation heapRel;

				relNum++;
				rel = HeapTupleGetOid(sysTuple);
				heapRel = heap_open(rel, AccessShareLock);

				rte = addRangeTableEntryForRelation(NULL, heapRel,
						makeAlias(appendIdToStringPP("auxrel", &relNum), NIL), false,
						true);

				heap_close(heapRel, NoLock);

				result = lappend(result, rte);
			}
		}
	}

	heap_endscan(scandesc);
	heap_close(catRel, AccessShareLock);

	return result;
}

/*
 *
 */

static List *
makeAuxNoUnion (Query *rep)
{
	List *result = NIL;
	WhereAttrInfo *attr;
	WhereProvInfo *info = GET_WHERE_PROVINFO(rep);
	ListCell *lc, *innerLc;
	Query *newAux;
	Query *reRep;
	Var *inVar;

	/* generate annotation propagation for representative and add
	 * representative to aux queries */
	reRep = copyObject(rep);
	generateAnnotBaseQueries(reRep);
	result = lappend(result, reRep);

	/* foreach out attribute A and each input attribute S.B that is in the list of
	 * input attribute that contribute to A, add a query with an additional join
	 * with S on S.B = S'.B and propagation of annotations from S'.B to A.
	 */
	foreach(lc, info->attrInfos)
	{
		attr = (WhereAttrInfo *) lfirst(lc);

		foreach(innerLc, attr->inVars)
		{
			inVar = (Var *) lfirst(innerLc);
			newAux = makeNoUnionAuxQuery(rep, reRep, attr, inVar);
			result = lappend(result, newAux);
		}
	}

	return result;
}

/*
 *
 */

static Query *
makeNoUnionAuxQuery (Query *rep, Query *rewrRep, WhereAttrInfo *attr, Var *inVar)
{
	Query *result = copyObject(rep);
	Query *baseAnnotQuery, *newAnnotQuery;
	RangeTblEntry *rte, *newRte;
	RangeTblRef *rtRef;
	Var *newVar;
	WhereAttrInfo *newattr;
	Node *eqCond;

	newattr = list_nth(GET_WHERE_ATTRINFOS(result),
			attr->outVar->varattno - 1);

	/* get the base relation we want to join with in the
	 * auxiliary query and generate an annotation propagation
	 * query for it.*/
	rte = rt_fetch(inVar->varno, rewrRep->rtable);
	baseAnnotQuery = rte->subquery;
	rte = copyObject(rt_fetch(1, baseAnnotQuery->rtable));

	newAnnotQuery = addBaseRelationAnnotationAttrs(rte,
			list_make1_int(inVar->varattno));
	addSubqueryToRT(result, newAnnotQuery, "auxjoinpartner");
	MAKE_RTREF(rtRef, list_length(result->rtable));
	result->jointree->fromlist = lappend(result->jointree->fromlist, rtRef);

	newRte = (RangeTblEntry *) llast(result->rtable);
	correctRTEAlias(newRte);

	/* add join condition to query qual */
	newVar = makeVar(list_length(result->rtable), inVar->varattno,
			inVar->vartype, inVar->vartypmod, 0);
	eqCond = createEqualityConditionForVars(newVar, copyObject(inVar));

	if (result->jointree->quals)
		result->jointree->quals = (Node *) makeBoolExpr(AND_EXPR,
				list_make2(result->jointree->quals, eqCond));
	else
		result->jointree->quals = eqCond;

	/* correct WhereAttrInfo */
	newattr->inVars = list_make1(copyObject(newVar));
	newVar = makeVar(list_length(result->rtable),
			list_length(newRte->eref->colnames), TEXTOID, -1, 0);
	newattr->annotVars = list_make1(newVar);

	return result;
}

/*
 * Based on the WhereAttrInfos of the query translate base relation accesses
 * into queries that propagate WHERE-CS annotations for all the base
 * relation attributes mentioned in the inVars of the WhereAttrInfos.
 */

#define GET_REL_ATTNUM(rtindex) \
	list_length(((RangeTblEntry *) rt_fetch(rtindex, query->rtable))->eref->colnames)

void
generateAnnotBaseQueries (Query *query)
{
	List **baseVars;
	List *baseRtIndexMap = NIL;
	int i, numBaseRels, rtepos, attrpos;
	ListCell *lc, *innerLc;
	WhereAttrInfo *attr;
	RangeTblEntry *rte;
	Var *inVar, *annotVar;

	/* get base relation rtes and store their rt indices */
	numBaseRels = 0;
	foreachi(lc, i, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		if (rte->rtekind == RTE_RELATION)
		{
			numBaseRels++;
			baseRtIndexMap = lappend_int(baseRtIndexMap, i + 1);
		}
	}

	/* generate data structure to store for which attributes of each of the
	 * base relations we have to propagate annotations. */
	baseVars = (List **) palloc(sizeof(List *) * numBaseRels);
	for(i = 0; i < numBaseRels; i++)
		baseVars[i] = NIL;

	/* loop through the WhereAttrInfos and gather inVars */
	foreach(lc, GET_WHERE_ATTRINFOS(query))
	{
		attr = (WhereAttrInfo *) lfirst(lc);

		foreach(innerLc, attr->inVars)
		{
			inVar = (Var *) lfirst(innerLc);
			rtepos = listPositionInt(baseRtIndexMap, inVar->varno);
			baseVars[rtepos] = list_append_unique_int(baseVars[rtepos], inVar->varattno);
		}
	}

	for(i = 0; i < numBaseRels; i++)
		baseVars[rtepos] = sortIntList(&baseVars[rtepos], true);

	/* adapt the WhereAttrInfo->annotVars */
	foreach(lc, GET_WHERE_ATTRINFOS(query))
	{
		attr = (WhereAttrInfo *) lfirst(lc);
		attr->annotVars = NIL;

		foreach(innerLc, attr->inVars)
		{
			inVar = (Var *) lfirst(innerLc);

			rtepos = listPositionInt(baseRtIndexMap, inVar->varno);
			attrpos = listPositionInt(baseVars[rtepos], inVar->varattno)
					+ GET_REL_ATTNUM(inVar->varno) + 1;
			annotVar = makeVar(inVar->varno, attrpos, TEXTOID, -1 ,0);
			attr->annotVars = lappend(attr->annotVars, annotVar);
		}
	}

	/* generate the annotation propagating base queries */
	for(i = 0; i < numBaseRels; i++)
		addBaseRelationAnnotationAttrs(rt_fetch(list_nth_int(baseRtIndexMap,i), query->rtable),
				baseVars[i]);

	/* clean up */
	for(i = 0; i < numBaseRels; i++)
		list_free(baseVars[i]);
	pfree(baseVars);
}

/*
 *
 */

static Query *
addBaseRelationAnnotationAttrs (RangeTblEntry *rte, List *attrs)
{
	Query *result;
	List *newTes = NIL;
	TargetEntry *origAttr;
	TargetEntry *annotAttr;
	ListCell *lc;
	Expr *annotExpr;
	int curResno, i;
	char *annotName;

	/* transform into a trivial query */
	result = generateQueryFromBaseRelation(copyObject(rte));
	curResno = list_length(result->targetList);

	/* for each attribute generate an annotation attribute */
	foreachi(lc, i, result->targetList)
	{
		origAttr = (TargetEntry *) lfirst(lc);

		if (listPositionInt(attrs, i + 1) != -1)
		{
			annotName = getWhereAnnotName(origAttr->resname);
			annotExpr = (Expr *) getAnnotValueExpr(rte->relid,
					origAttr->resname);
			annotAttr = makeTargetEntry ((Expr *) annotExpr, ++curResno,
					annotName, false);
			newTes = lappend(newTes, annotAttr);
		}
	}

	result->targetList = list_concat(result->targetList, newTes);

	/* change rte type to subquery */
	rte->rtekind = RTE_SUBQUERY;
	rte->subquery = result;
	correctRTEAlias(rte);

	return result;
}

/*
 * Create the expression that computes the value of an WHERE-CS annotation
 * attribute. This expression concatenates the constant prefix string
 * with the oid attribute value of a tuple. For instance, for the annotation
 * attribute for relation R and its attribute A the following prefix is
 * generated:
 * 		"R#A#"
 */

static Node *
getAnnotValueExpr (Oid reloid, char *attrName)
{
	Const *annotConst;
	Node *result;
	Var *oidVar;
	Node *oidCast;
	char *annotValue;
	char *relName;
	int constLength;

	/* create a var for the oid column */
	oidVar = makeVar(1, -2, OIDOID ,-1, 0);
	oidCast = coerce_to_target_type(NULL, (Node *) oidVar, OIDOID, TEXTOID, -1,
			COERCION_EXPLICIT, COERCE_EXPLICIT_CAST);

	/* create a constant for the annotation prefix
	 * ("Relname#Attrname#" ) */
	relName = getRelationNameUnqualified(reloid);
	constLength = strlen(relName) + strlen(attrName) + 3;
	annotValue = (char *) palloc(constLength);

	annotValue = strcpy(annotValue, relName);
	annotValue = strcat(annotValue, "#");
	annotValue = strcat(annotValue, attrName);
	annotValue = strcat(annotValue, "#");

	pfree(relName);

	annotConst = makeConst(TEXTOID, -1, -1,
			DirectFunctionCall1(textin,CStringGetDatum(annotValue)),
			false, false);

	/* apply text concat to both values */
	result = (Node *) makeFuncExpr(F_TEXTCAT, TEXTOID,
			list_make2(annotConst, oidCast), COERCE_EXPLICIT_CALL);

	return result;
}


/*
 *
 */

Query *
pullUpSubqueries (Query *query)
{
	List *rtindexMap = NIL;
	List *subIndex = NIL;
	List *subqueries = NIL;
	List *relAccess = NIL;
	List *joinAccess = NIL;
	List *joinIndex = NIL;
	List *subFroms = NIL;
	List **subqueryVars;
	ListCell *lc, *innerLc;
	Query *sub;
	RangeTblEntry *rte;
	Var *var;
	int i;

	makeTeNamesUnique(query);

	/* check for a simple wrapper query around a union */
	if (list_length(query->rtable) == 1 && !query->setOperations)
	{
		rte = (RangeTblEntry *) linitial(query->rtable);

		if (rte->rtekind == RTE_SUBQUERY)
		{
			sub = rte->subquery;

			if (sub->setOperations)
				return sub;
		}
	}

	/* in case of a UNION try to pull up the subqueries of inputs to the union */
	if (query->setOperations)
	{
		foreach(lc, query->rtable)
		{
			rte = (RangeTblEntry *) lfirst(lc);

			if (rte->rtekind == RTE_SUBQUERY)
				pullUpSubqueries(rte->subquery);
		}

		return query;
	}

	/* partition query into relation accesses/joins and subqueries */
	foreachi(lc, i, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		Assert(rte->rtekind == RTE_SUBQUERY
				|| rte->rtekind == RTE_RELATION
				|| rte->rtekind == RTE_JOIN);

		if (rte->rtekind == RTE_SUBQUERY)
		{
			subqueries = lappend(subqueries, rte->subquery);
			subIndex = lappend_int(subIndex, i + 1);
		}
		else if (rte->rtekind == RTE_RELATION)
		{
			relAccess = lappend(relAccess, rte);
			rtindexMap = lappend_int(rtindexMap, i + 1);
		}
		else
		{
			joinAccess = lappend(joinAccess, rte);
			joinIndex = lappend_int(joinIndex, i + 1);
		}
	}

	/* preserve the vars of subqueries and basertes */
	subqueryVars = (List **) palloc(sizeof(List *) * list_length(query->rtable));

	for(i = 0; i < list_length(query->rtable); i++)
		subqueryVars[i] = NIL;

	for(i = 1; i <= list_length(query->rtable); i++)
		fetchAllRTEVars(query, &(subqueryVars[i - 1]), i);

	/* map the rtindex for base relation accesses to new order */
	foreachi(lc, i, rtindexMap)
	{
		foreach(innerLc, subqueryVars[lfirst_int(lc) - 1])
		{
			var = (Var *) lfirst(innerLc);

			var->varnoold = i + 1;
			var->varno = i + 1;
		}
	}

	/* start of with only the base relation accesses */
	query->rtable = relAccess;

	/* retrieve all the base relation accesses of each subqueries, possibly
	 * calling ourselves.
	 */

	forboth(lc, subqueries, innerLc, subIndex)
	{
		sub = (Query *) lfirst(lc);
		addSubqueriesAsBase(query, sub, lfirst_int(innerLc),
				subqueryVars[lfirst_int(innerLc) - 1], &subFroms);
	}

	/* map the rtindex for joins to new order */
	foreachi(lc, i, joinIndex)
	{
		foreach(innerLc, subqueryVars[lfirst_int(lc) - 1])
		{
			var = (Var *) lfirst(innerLc);
			var->varno = i + 1 + list_length(query->rtable);
			var->varnoold = var->varno;//TODO check
		}
	}

	/* add join rtes again */
	query->rtable = list_concat(query->rtable, joinAccess);

	/* adapt join tree */
	adaptJoinTreeWithSubTrees (query, subIndex, rtindexMap, joinIndex, subFroms);

	/* recreate join RT entries */
	recreateJoinRTEs(query);

	return query;
}

/*
 *
 */

static void
makeTeNamesUnique (Query *query)
{
	ListCell *lc;
	TargetEntry *te;
	int i = 1;

	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);

		te->resname = appendIdToStringPP("orig_attr_", &i);
		i++;
	}
}

/*
 *
 */

static void
addSubqueriesAsBase(Query *root, Query *sub, int subIndex, List *vars, List **subFroms)
{
	ListCell *lc;
	TargetEntry *te;
	Var *var;
	Var *innerVar;
	int curRtindex = list_length(root->rtable);
	FromExpr *subJoinTree;

	joinQueryRTEs(sub);
	pullUpSubqueries(sub);

	/* set varnos to new varno for the rte for the vars for sub in the target
	 * list of the super query */
	foreach(lc, vars)
	{
		var = (Var *) lfirst(lc);
		te = list_nth(sub->targetList, var->varattno - 1);
		innerVar = getVarFromTeIfSimple((Node *) te->expr);
		Assert(innerVar != NULL);

		var->varno = curRtindex + innerVar->varno;
		var->varnoold = var->varno;
		var->varattno = innerVar->varoattno;
		var->vartype = innerVar->vartype;
		var->vartypmod = innerVar->vartypmod;
	}

	root->rtable = list_concat(root->rtable, sub->rtable);

	/* get subquery join tree and adapt varnos */
	subJoinTree = sub->jointree;
	addToVarnoWalker((Node *) subJoinTree, &curRtindex);
	*subFroms = lappend(*subFroms, subJoinTree);
}


/*
 * Search for vars from a range table entry in a query node and append them to
 * list vars.
 */

static List *
fetchAllRTEVars (Query *query, List **vars, int rtIndex)
{
	FetchVarsContext *context;

	context = (FetchVarsContext *) palloc(sizeof(FetchVarsContext));
	context->rtindex = rtIndex;
	context->result = vars;

	query_tree_walker(query, fetchAllRTEVarsWalker, (void *) context, QTW_IGNORE_RT_SUBQUERIES);

	return *vars;
}

/*
 *
 */

static bool
fetchAllRTEVarsWalker (Node *node, FetchVarsContext *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, Var))
	{
		Var *var = (Var *) node;

		if (var->varno == context->rtindex)
			*(context->result) = lappend(*(context->result), var);

		return false;
	}

	return expression_tree_walker(node, fetchAllRTEVarsWalker, (void *) context);
}

/*
 *
 */

static bool
addToVarnoWalker (Node *node, int *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, JoinExpr))
	{
		JoinExpr *join = (JoinExpr *) node;

		join->rtindex += *context;
	}
	else if (IsA(node, RangeTblRef))
	{
		RangeTblRef *rtRef = (RangeTblRef *) node;

		rtRef->rtindex += *context;

		return false;
	}

	return expression_tree_walker (node, addToVarnoWalker, context);
}

/*
 *
 */

static void
adaptJoinTreeWithSubTrees (Query *query, List *subIndex, List *rtindexMap,
		List *joinIndex, List *subFroms)
{
	ListCell *lc;
	FromExpr *subFrom;
	Node **fromItem;
	int joinOffset = list_length(query->rtable) - list_length(joinIndex) + 1;

	foreach(lc, subFroms)
	{
		subFrom = (FromExpr *) lfirst(lc);

		if (subFrom->quals != NULL) {
			if (query->jointree->quals == NULL)
				query->jointree->quals = subFrom->quals;
			else
				query->jointree->quals = (Node *) makeBoolExpr(AND_EXPR,
						list_make2(query->jointree->quals, subFrom->quals));
		}
	}


	foreach(lc, query->jointree->fromlist)
	{
		fromItem = (Node **) &(lc->data.ptr_value);
		adaptFromItem(fromItem, subIndex, rtindexMap, joinIndex, subFroms,
				joinOffset);
	}
}

/*
 *
 */

static void
adaptFromItem (Node **fromItem, List *subIndex, List *rtindexMap,
		List *joinIndex, List *subFroms, int joinOffset)
{
	int pos;
	FromExpr *subFrom;

	if (IsA(*fromItem, JoinExpr))
	{
		JoinExpr *join = (JoinExpr *) *fromItem;

		pos = listPositionInt(joinIndex, join->rtindex);
		join->rtindex = pos + joinOffset;

		adaptFromItem(&(join->larg), subIndex, rtindexMap, joinIndex, subFroms,
				joinOffset);
		adaptFromItem(&(join->rarg), subIndex, rtindexMap, joinIndex, subFroms,
				joinOffset);
	}
	else
	{
		RangeTblRef *rtRef = (RangeTblRef *) *fromItem;

		pos = listPositionInt(subIndex, rtRef->rtindex);

		/* is a subquery */
		if (pos != -1)
		{
			subFrom = (FromExpr *) list_nth(subFroms, pos);

			/* sub query from list should have only one item */
			Assert (list_length(subFrom->fromlist) == 1);

			*fromItem = (Node *) linitial(subFrom->fromlist);
		}
		/* is a base relation rte */
		else
		{
			pos = listPositionInt(rtindexMap, rtRef->rtindex);
			rtRef->rtindex = pos + 1;
		}
	}
}
