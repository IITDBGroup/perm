/*-------------------------------------------------------------------------
 *
 * prov_copy_map.c
 *	  PERM C - Support data structure for copy-contribution semantics.
 *	  Basically a list of attribute equivalences for each base relation
 *	  accessed by a provenance query.
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/prov_copy_map.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "nodes/makefuncs.h"
#include "optimizer/clauses.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"

#include "provrewrite/prov_util.h"
#include "provrewrite/provlog.h"
#include "provrewrite/provattrname.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_sublink_util_analyze.h"
#include "provrewrite/prov_copy_util.h"
#include "provrewrite/prov_copy_map.h"

// data structures
typedef enum StaticType {
	STATIC_TRUE,
	STATIC_FALSE,
	NOT_STATIC
} StaticType;

typedef struct EquiGraphNode {
	Var *nodeVar;
	List *sEdges;
	List *nsEdges;
	int pos;
} EquiGraphNode;

// macros
#define MAKE_EQUINODE(nodeholder,var,graph) \
	do { \
		nodeholder = (EquiGraphNode *) palloc(sizeof(EquiGraphNode)); \
		nodeholder->var = copyObject(var); \
		nodeholder->sEdges = NIL; \
		nodeholder->nsEdges = NIL; \
		nodeholder->pos = 0; \
		*graph = lappend(*graph, nodeholder); \
	} while (0)

#define DELETE_NS_EDGE(left, right) \
	do { \
		left->nsEdges = list_delete_ptr(left->nsEdges, right); \
		right->nsEdges = list_delete_ptr(right->nsEdges, left); \
	} while (0)

// prototypes
static CopyMap *generateCopyMapForQueryNode (Query *query, ContributionType contr,
		Index rtindex);
static CopyMap *generateBaseRelCopyMap (RangeTblEntry *rte, Index rtindex);

static void inferStaticCopy (Query *query, ContributionType contr);
static void inferStaticCopyQueryNode (Query *query, ContributionType contr);
static void analyzeAttrPaths (CopyMapEntry *attr, CopyMapRelEntry *relEntry);
static StaticType getInclusionStaticType (Var *inclVar, Var *baseVar,
		CopyMapRelEntry *rel);

static void handleSetOps (Query *query, List *subMaps);
static CopyMap *handleSetOp (Query *query, List *subMaps, Node *setOp);
static void generateAggCopyMap (Query *query, Index rtindex);
static void handleProjectionCopyMap (Query *query, Index rtindex);
static void mapChildVarsToTargetEntries (Query *query, Index rtindex,
		List *tes);
static CaseInfo *getCaseInfoIfSimple (Node *expr, Query *query);
static AttrInclusions *getInclForVar (CopyMapEntry *entry, Var *var);
static void handleJoins (Query *query);
static void handleJoinTreeItem (Query *query, Node *joinItem,
		List **equiGraph);
static void analyzeConditionForCopy (Query *query, Node *condition,
		List *relEntries, List **equiGraph);
static void addEquiGraphEdge (List *eq, List **equiGraph);
static bool hasEdge (EquiGraphNode *left, EquiGraphNode *right, bool onlyS);
static void addCondForEquiGraph(List *equiGraph, List *rels);
static void searchStaticPath (EquiGraphNode *root, EquiGraphNode *node,
		bool *hasStatic, int numNodes);
static int compareEquiNode (void *left, void *right);
static bool findVarEqualitiesWalker (Node *node, List **context);
static bool addEqualityCondsForVarsWalker (CopyMapRelEntry *entry,
		CopyMapEntry *attr, void *context);
static bool hasEqualityForVar (InclusionCond *cond, void *context);
static bool hasExistsForVar (InclusionCond *cond, void *context);
static void setPropagateFlags (Query *query);
static void setPropagateFlagsForQueryNode (Query *query);

static CopyMap *getCopyMapForRtindex (List *maps, Index rtindex);
static void swtichInAndOutVars (CopyMap *map);
static void mapJoinAttrs (CopyMapEntry *attr, List *joinVars, Index rtindex);
static CopyMap *mergeMaps (CopyMap *left, CopyMap *right);
static void emptyMap (CopyMap *map);
static void setCopyMapRTindices (CopyMap *map, Index rtindex);
static CopyMapRelEntry * getRelEntry (CopyMap *map, Oid rel, int refNum);



/*
 *
 */
List *
getQueryOutAttrs (Query *query)
{
	TargetEntry *te;
	ListCell *lc;
	List *result;

	result = NIL;

	foreach(lc, query->targetList)
	{
		te = (TargetEntry *) lfirst(lc);

		if (!te->resjunk && IsA(te->expr, Var)) //TODO add support for casts
			result = list_append_unique(result, te->expr);
	}

	return result;
}

/*
 * Generate the CopyMap data structure for a query. A CopyMap is produced for
 * each query node in the query and is stored in this query nodes ProvInfo
 * field. The CopyMap of a query node q stores the information about which
 * attributes of the base relations accessed by q are copied to the result of
 * q. This information is needed for copy contribution semantics provenance
 * rewrite to know which parts of a query should be rewritten to propagate
 * provenance. The map generation differs depending on the type of copy
 * contribution semantics (C-CS) used:
 *
 * 	CDC-CS: Only the provenance of base relations for which each attribute is
 * 			directly copied are propagated.
 * 	CTC-CS: Only the provenance of base relations for which each attribute is
 * 			transitively copied are propagated.
 * 			(e.g. by WHERE equality conditions)
 * 	PDC-CS: Only the provenance of base relations for which at least one
 * 			attribute is directly copied are propagated.
 * 	PTC-CS: Only the provenance of base relations for which at least one
 * 			attribute is transitively copied are propagated.
 */

void
generateCopyMaps (Query *query)
{
	generateCopyMapForQueryNode(query, ContributionType(query), 1);

	inferStaticCopy (query, ContributionType(query));
}

/*
 *
 */

static CopyMap *
generateCopyMapForQueryNode (Query *query, ContributionType contr, Index rtindex)
{
	ListCell *lc;
	ListCell *innerLc;
	RangeTblEntry *rte;
	List *subMaps;
	CopyMap *currentMap;
	CopyMap *newMap;
	CopyMapRelEntry *newRel;
	CopyMapRelEntry *curRel;
	int i;

	subMaps = NIL;
	//TODO add empty maps for sublinks at first

	// generate empty copy map for query
	newMap = makeCopyMap();
	newMap->rtindex = rtindex;
	SET_COPY_MAP(query, newMap);

	/* call us self to generate Copy maps of subqueries */
	foreachi(lc, i, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);
		currentMap = NULL;

		// generate map for rte
		switch(rte->rtekind)
		{
		case RTE_SUBQUERY:
			currentMap = generateCopyMapForQueryNode(rte->subquery, contr, i + 1);
			setCopyMapRTindices (currentMap, i + 1);
			subMaps = lappend(subMaps, currentMap);
			break;
		case RTE_RELATION:
			currentMap = generateBaseRelCopyMap(rte, i + 1);
			subMaps = lappend(subMaps, currentMap);
			break;
		default:
			//TODO
			break;
		}

		if (currentMap)
		{
			// add new RelMaps for the RelMaps of the rte and link them
			foreach(innerLc, currentMap->entries)
			{
				curRel = (CopyMapRelEntry *) lfirst(innerLc);
				COPY_BASIC_COPYREL(newRel,curRel);
				newRel->rtindex = i + 1;
				newMap->entries = lappend(newMap->entries, newRel);
			}
		}
 	}

	/* create the map for the query based on the submaps */
	/* set operation query */
	if (query->setOperations)
		handleSetOps(query, subMaps);
	/* SPJ or ASPJ */
	else
	{
		handleJoins(query);

		/* use these to generate copy map for the current query */
		if (query->hasAggs)
			generateAggCopyMap(query, rtindex);
		else
			handleProjectionCopyMap(query, rtindex);
	}

	/* link sub CopyRelMaps */

	return GET_COPY_MAP(query);
}



///*
// *
// */
//
//static CopyMap *
//generateCopyMapCDCForQuery (Query *query, Index rtindex)
//{
//	ListCell *lc;
//	RangeTblEntry *rte;
//	List *subMaps;
//	CopyMap *currentMap;
//	int i;
//
//	//TODO add empty maps for sublinks at first
//
//	subMaps = NIL;
//
//	/* call us self to generate Copy maps of subqueries */
//	foreachi(lc, i, query->rtable)
//	{
//		rte = (RangeTblEntry *) lfirst(lc);
//
//		switch(rte->rtekind)
//		{
//		case RTE_SUBQUERY:
//			currentMap = generateCopyMapCDCForQuery(rte->subquery, i + 1);
//			setCopyMapRTindices (currentMap, i + 1);
//			subMaps = lappend(subMaps, currentMap);
//			break;
//		case RTE_RELATION:
//			currentMap = generateBaseRelCopyMap(rte, i + 1);
//			subMaps = lappend(subMaps, currentMap);
//			break;
//		default:
//			//TODO
//			break;
//		}
// 	}
//
//	/* set operation query */
//	if (query->setOperations)
//		handleSetOps(query, subMaps);
//
//	/* SPJ or ASPJ */
//	else
//	{
//		Provinfo(query)->copyInfo = (Node *) handleJoins(query, subMaps);
//
//		/* use these to generate copy map for the current query */
//		if (query->hasAggs)
//			generateAggCopyMap(query, rtindex);
//		else
//			handleProjectionCopyMap(query, rtindex);
//	}
//
//	GET_COPY_MAP(query)->rtindex = rtindex;
//
//	return copyObject(GET_COPY_MAP(query));
//}

/*
 * Create the CopyMap for an set operation query from the CopyMaps of its range table entries.
 */

static void
handleSetOps (Query *query, List *subMaps)
{
//	CopyMap *map;
//	ListCell *lc, *innerLc, *attrLc;
//	CopyMapRelEntry *relEntry;
//	CopyMapEntry *attr;
//	Var *var;

	//map = handleSetOp (query, subMaps, query->setOperations);

//	foreach(lc, map->entries)
//	{
//		relEntry = (CopyMapRelEntry *) lfirst(lc);
//
//		foreach(innerLc, relEntry->attrEntries)
//		{
//			attr = (CopyMapEntry *) lfirst(innerLc);
//
//			attr->inVars = copyObject(attr->outVars);
//
//			foreach(attrLc, attr->outVars)
//			{
//				var = (Var *) lfirst(attrLc);
//
//				var->varno = 1;//CHECK OK?
//			}
//
//		}
//	}

	//Provinfo(query)->copyInfo = (Node *) map;
}


/*
 * Create the CopyMap for a set operation from the CopyMaps of its left and
 * right input.
 */

static CopyMap *
handleSetOp (Query *query, List *subMaps, Node *setOp)
{
//	CopyMap *rMap;
//	CopyMap *lMap;
//	SetOperationStmt *setOper;
//	RangeTblRef *rtRef;

//	if(IsA(setOp, SetOperationStmt))
//	{
//		setOper = (SetOperationStmt *) setOp;
//
//		/* get maps for children */
//		lMap = handleSetOp(query, subMaps, setOper->larg);
//		rMap = handleSetOp(query, subMaps, setOper->rarg);
//
//		/* merge child maps */
//		switch(setOper->op)
//		{
//		case SETOP_EXCEPT:
//			emptyMap(rMap);
//		case SETOP_UNION:
//		case SETOP_INTERSECT:
//			return mergeMaps(lMap,rMap);
//		default:
//			return NULL; //TODO error
//		}
//
//	}
//	else
//	{
//		rtRef = (RangeTblRef *) setOp;
//		lMap = (CopyMap *) getCopyMapForRtindex(subMaps, rtRef->rtindex);
//
//		return lMap;
//	}

	return NULL;
}

/*
 * Create the CopyMap for an aggregation. In an aggregation only the group by
 * attributes are considered to be copied from the input of the aggregation.
 */

static void
generateAggCopyMap (Query *query, Index rtindex)
{
	List *groupBys;

	groupBys = getGroupByTLEs(query);
	mapChildVarsToTargetEntries(query, rtindex, groupBys);
}

/*
 * Adapt a CopyMap according to the projections in the target list of a query.
 */

static void
handleProjectionCopyMap (Query *query, Index rtindex)
{
	mapChildVarsToTargetEntries(query, rtindex, query->targetList);
}


/*
 * Given a list of target list entries of a query adapt the AttrInclusion lists
 * of the CopyMapRelEntries of the query according to which attributes are copied
 * by the target list entries.
 *
 * E.g. for R(a,b,c)
 *
 * 		SELECT a, a + b, c AS dummy, CASE WHEN C THEN d ELSE e END AS x FROM r
 *
 * Here a and c are copied, but b is not because it is only used in the
 * expression (a + b). If d and e are copied depends on the evaluation of
 * condition C. If C is trues then d is copied to x, else e is copied to x.
 */

#define MAKE_PROJ_INCL(result) \
	do { \
		result = makeAttrInclusions(); \
		result->attr = var; \
		attr->outAttrIncls = lappend(attr->outAttrIncls, result); \
	} while (0)

static void
mapChildVarsToTargetEntries (Query *query, Index rtindex, List *tes)
{
	List *simpleVars;
	List *tePos;
	List *oldIncls;
	List *cases;
	ListCell *lc, *innerLc, *attLc, *teLc, *caseLc, *varLc;
	Var *var;
	Var *inVar;
	Var *teVar;
	TargetEntry *te;
	CopyMap *map;
	CopyMapRelEntry *relEntry;
	CopyMapEntry *attr;
	AttrInclusions *incl;
	AttrInclusions *newIncl;
	InclusionCond *newCond;
	CaseInfo *caseInfo;
	Node *caseCond;
	Var *caseVar;
	int pos;

	map = GET_COPY_MAP(query);
	tePos = NIL;
	cases = NIL;
	simpleVars = NIL;

	/* get all simple vars in the target list */
	foreach(lc, tes)
	{
		te = (TargetEntry *) lfirst(lc);

		if (!te->resjunk)
		{
			var = getVarFromTeIfSimple((Node *) te->expr);
			if (var)
				var = resolveToRteVar(var, query);
			caseInfo = getCaseInfoIfSimple((Node *) te->expr, query);

			simpleVars = lappend(simpleVars, var);
			cases = lappend(cases, caseInfo);
			tePos = lappend_int(tePos, te->resno);
		}
	}

	/* loop through entries and add new AttrInclusions for the result
	 * attributes of the query */
	foreach(lc, map->entries)
	{
		relEntry = (CopyMapRelEntry *) lfirst(lc);

		foreach(innerLc, relEntry->attrEntries)
		{
			attr = (CopyMapEntry *) lfirst(innerLc);
			oldIncls = attr->outAttrIncls;
			attr->outAttrIncls = NIL;

			foreach(attLc, oldIncls)
			{
				incl = (AttrInclusions *) lfirst(attLc);
				inVar = (Var *) incl->attr;

				/* is inVar used somewhere in the projection list. If so, add a
				 * new AttrInclusion for the projection attr that uses inVar and
				 * add the current AttrInclusions as the outVarIncls of the new
				 * AttrInclusion */
				foreachi(teLc, pos, simpleVars) {
					teVar = (Var *) lfirst(teLc);

					if (teVar && teVar->varno == inVar->varno
							&& teVar->varattno == inVar->varattno)
					{
						var = makeVar(rtindex, list_nth_int(tePos,pos),
								teVar->vartype, teVar->vartypmod, 0);//TODO exprType anpassen?
						newIncl = getInclForVar(attr,var);

						if (newIncl == NULL)
							MAKE_PROJ_INCL(newIncl);

						MAKE_EXISTS_INCL(newCond, (Node *) incl);
						newIncl->inclConds = lappend(newIncl->inclConds,
								newCond);
					}
				}

				/* For each WHEN X THEN inVar in a bare CaseExpr in the
				 * projection list add a conditional inclusion for the current
				 * AttrInclusions to the new AttrInclusions for the result
				 * attribute. */
				foreachi(teLc, pos, cases)
				{
					caseInfo = (CaseInfo *) lfirst(teLc);

					if(caseInfo)
					{
						forboth(caseLc, caseInfo->conditions, varLc,
								caseInfo->vars)
						{
							caseCond = (Node *) lfirst(caseLc);
							caseVar = (Var *) lfirst(varLc);

							if (caseVar->varno == inVar->varno
									&& caseVar->varattno == inVar->varattno)
							{
								var = makeVar(rtindex,
										list_nth_int(tePos, pos),
										caseVar->vartype,
										caseVar->vartypmod, 0);//TODO exprType anpassen?
								newIncl = getInclForVar(attr,var);

								if (newIncl == NULL)
									MAKE_PROJ_INCL(newIncl);

								MAKE_COND_INCL(newCond, (Node *) incl,
										caseCond);
								newIncl->inclConds =
										lappend(newIncl->inclConds, newCond);
							}
						}
					}
				}
			}
		}
	}
}

/*
 * For an input expression return a CaseInfo if it is a CaseExpr that has an
 * single Var as at least one of its return values. This means if it copies
 * the value of at least one attribute.
 */

static CaseInfo *
getCaseInfoIfSimple (Node *expr, Query *query)
{
	CaseInfo *caseInfo;
	CaseExpr *caseExpr;
	CaseWhen *when;
	ListCell *lc;
	bool returnInfo = false;
	bool buildElse = false;
	bool equalCase;
	Node *whenCond;
	List *elseExpr = NIL;
	Var *var;
	OpExpr *equalOp;

	if (!IsA(expr, CaseExpr))
		return NULL;

	caseExpr = (CaseExpr *) expr;
	CREATE_CASEINFO(caseInfo);

	if (caseExpr->defresult
			&& getVarFromTeIfSimple((Node *) caseExpr->defresult) != NULL)
		returnInfo = buildElse = true;

	equalCase = (caseExpr->arg != NULL);

	foreach(lc, caseExpr->args)
	{
		when = (CaseWhen *) lfirst(lc);
		whenCond = (Node *) when->expr;
		var = getVarFromTeIfSimple((Node *) when->result);

		/* construct the real condition in case of equality CaseExpr */
		if (equalCase)
		{
			whenCond = copyObject(whenCond);
			equalOp = (OpExpr *) whenCond;

			linitial(equalOp->args) = copyObject(caseExpr->arg);
		}

		if (buildElse)
			elseExpr = lappend(elseExpr, copyObject(whenCond));

		// if result is a simple var we add the condition and var to CaseInfo
		if (var != NULL)
		{
			var = resolveToRteVar(var, query);
			returnInfo = true;
			caseInfo->conditions = lappend(caseInfo->conditions,
					copyObject(whenCond));
			caseInfo->vars = lappend(caseInfo->vars, var);
		}
	}

	if (buildElse)
	{
		whenCond = (Node *) makeBoolExpr(NOT_EXPR, list_make1(
				makeBoolExpr(AND_EXPR, elseExpr)));
		caseInfo->conditions = lappend(caseInfo->conditions, whenCond);
		caseInfo->vars = lappend(caseInfo->vars,
				copyObject(caseExpr->defresult));
	}

	// did we need a case info for this expression?
	if (returnInfo)
		return caseInfo;

	free(caseInfo);
	return NULL;

}

/*
 * Search for an attribute inclusion for a specified var node in a
 * CopyMapEntry. Returns NULL if no such AttrInclusions is found.
 */

static AttrInclusions *
getInclForVar (CopyMapEntry *entry, Var *var)
{
	ListCell *lc;
	AttrInclusions *incl;

	foreach(lc, entry->outAttrIncls)
	{
		incl = (AttrInclusions *) lfirst(lc);

		if (equal(incl->attr, var))
			return incl;
	}

	return NULL;
}

/*
 * Process the jointree of a query to create a CopyMap for the top level
 * jointree node from the CopyMaps of the range table entries of the query.
 * These are provided as parameter subMaps.
 */

static void
handleJoins (Query *query)
{
	ListCell *lc;
	Node *joinItem;
	List *joinMaps = NIL;
	CopyMap *map;
	List *equiGraph = NIL;

	map = GET_COPY_MAP(query);

	/* get the copy map for each from list item */
	foreach(lc, query->jointree->fromlist)
	{
		joinItem = (Node *) lfirst(lc);

		handleJoinTreeItem(query, joinItem, &equiGraph);
	}

	if (IS_TRANSC(ContributionType(query)))
		analyzeConditionForCopy(query, query->jointree->quals, map->entries, &equiGraph);

	addCondForEquiGraph(equiGraph, map->entries);
}

/*
 * Create the CopyMap for a node in a join tree from the CopyMaps of its left
 *  and right child or RTE-CopyMap in case of a leaf node.
 */

static void
handleJoinTreeItem (Query *query, Node *joinItem, List **equiGraph)
{
	List *rels;
	ListCell *lc;

	if(IsA(joinItem, JoinExpr))
	{
		JoinExpr *join = (JoinExpr *) joinItem;

		handleJoinTreeItem(query, join->larg, equiGraph);
		handleJoinTreeItem(query, join->rarg, equiGraph);

		rels = getCopyRelsForRtindex(query, join->rtindex);

		// The qual is only analyzed for static
		if (IS_TRANSC(ContributionType(query)))
			analyzeConditionForCopy(query, join->quals, rels, equiGraph);
	}
	/* is a input rte reference. Initialize the AttrIncludes based on
	 * the CopyMapRelEntry's of the RTE. */
	else
	{
		RangeTblRef *rtRef;
		ListCell *newLc, *childLc;
		ListCell *outAttrLc;
		CopyMapRelEntry *rel;
		CopyMapRelEntry *relChild;
		CopyMapEntry *attrEntry;
		CopyMapEntry *childAttrEntry;
		AttrInclusions *newIncl;
		AttrInclusions *childIncl;
		InclusionCond *newCond;

		rtRef = (RangeTblRef *) joinItem;
		rels = getCopyRelsForRtindex(query, rtRef->rtindex);

		foreach(lc, rels)
		{
			rel = (CopyMapRelEntry *) lfirst(lc);
			relChild = rel->child;

			/*for each CopyMapRelEntry generate basic exists include
			 * conditions */
			forboth(newLc, rel->attrEntries, childLc, relChild->attrEntries)
			{
				attrEntry = (CopyMapEntry *) lfirst(newLc);
				childAttrEntry = (CopyMapEntry *) lfirst(childLc);

				/* add all attributes inclusions from the child CopyMapEntry
				 * and for each add a exists condition for the child attribute.
				 */
				foreach(outAttrLc, childAttrEntry->outAttrIncls)
				{
					childIncl = (AttrInclusions *) lfirst(outAttrLc);
					COPY_BASIC_COPYAINLC(newIncl, childIncl);
					MAKE_EXISTS_INCL(newCond, copyObject(newIncl->attr));
					newIncl->inclConds = list_make1(newCond);
					newIncl->attr->varno = rel->rtindex;

					attrEntry->outAttrIncls = lappend (attrEntry->outAttrIncls,
							newIncl);
				}
			}
		}
	}
}

/*
 * Analyses a join or where condition to find equality comparisons that imply
 * transitive copy between two attributes. E.g., in
 *
 * 			SELECT a FROM R WHERE a = b;
 *
 * the values of attribute b are implicitely copied to attribute a.
 */

static void analyzeConditionForCopy(Query *query, Node *condition,
		List *relEntries, List **equiGraph)
{
	Node *baseCondition;
	List *eqConds = NIL;
	List *equal;
	ListCell *lc;
	Var *left, *right;
	VarEqualitiesContext *context;

	/* replace join vars with base RTE vars */
	baseCondition = conditionOnRteVarsMutator(condition, query);

	// find equality conditions
	context = (VarEqualitiesContext *) palloc(sizeof(VarEqualitiesContext));
	context->result = &eqConds;
	context->root = condition;

	findVarEqualitiesWalker(baseCondition, context);

	/* for each equality condition x = y, add an inclusion condition for all
	 * copy map entries using x or y. */
	foreach(lc, eqConds)
	{
		equal = (List *) lfirst(lc);

		addEquiGraphEdge(equal, equiGraph);
//		copyMapWalker(relEntries, NULL, (void *) equal, NULL, NULL,
//				addEqualityCondsForVarsWalker, NULL);
	}
}

/*
 *
 */

static void
addEquiGraphEdge (List *eq, List **equiGraph)
{
	Var *left  = (Var *) linitial((List *) eq);
	Var *right = (Var *) lsecond((List *) eq);
	bool useEqual = (intVal((Value *) lthird((List *) eq)) == 1L);
	EquiGraphNode *leftNode, *rightNode, *cur;
	ListCell *lc;

	leftNode = rightNode = NULL;

	// search for the node for the left and right var
	foreach(lc, *equiGraph)
	{
		cur = (EquiGraphNode *) lfirst(lc);
		if (equal(leftNode, cur->var))
			leftNode = cur;
		else if (equal(rightNode, cur->var))
			rightNode = cur;
	}

	// create nodes for left and right var if not in graph
	if (!leftNode)
		MAKE_EQUINODE(leftNode, left, equiGraph);
	if (!rightNode)
		MAKE_EQUINODE(rightNode, right, equiGraph);

	// add edge between left and right
	if (useEqual)
	{
		// add non static edge if there is no edge between the nodes yet
		if (!hasEdge(leftNode, rightNode, false))
		{
			leftNode->nsEdges = lappend(leftNode->nsEdges, rightNode);
			rightNode->nsEdges = lappend(rightNode->nsEdges, leftNode);
		}
	}
	else
	{
		// add static edge and delete ns-edges if no s-edge is there
		if (!hasEdge(leftNode, rightNode, true))
		{
			leftNode->sEdges = lappend(leftNode->sEdges, rightNode);
			rightNode->sEdges = lappend(rightNode->sEdges, leftNode);
			DELETE_NS_EDGE(leftNode, rightNode);
		}
	}
}

/*
 *
 */

static bool
hasEdge (EquiGraphNode *left, EquiGraphNode *right, bool onlyS)
{
	ListCell *lc;
	EquiGraphNode *cur;

	foreach(lc, left->sEdges)
	{
		cur = (EquiGraphNode *) lfirst(lc);
		if (cur == right)
			return true;
	}

	if (onlyS)
		return false;

	foreach(lc, left->nsEdges)
	{
		cur = (EquiGraphNode *) lfirst(lc);
		if (cur == right)
			return true;
	}

	return false;
}

/*
 *
 */

#define MARK_STATIC_PATH(lnode, rnode) \
	do { \
		hasStatic[(lnode->pos  * numNodes) + rnode->pos] = true; \
		hasStatic[(rnode->pos  * numNodes) + lnode->pos] = true; \
	} while (0)

#define HAS_STATIC(lnode,rnode) \
	(hasStatic[(lnode->pos  * numNodes) + rnode->pos])

#define HAS_STATIC_POS(lpos,rpos) \
	(hasStatic[(lpos *numNodes) + rpos])

static void
addCondForEquiGraph (List *equiGraph, List *rels)
{
	int numNodes = list_length(equiGraph);
	bool *hasStatic = palloc0(numNodes * numNodes * sizeof(bool));
	bool *haveSeen = NULL;
	int i;
	EquiGraphNode *cur;
	CopyMapRelEntry *rel;
	CopyMapEntry *attr;
	ListCell *lc, *innerLc;
	List *newAttrIncls;
	AttrInclusions *attrIncl;
	AttrInclusions *newAttrIncl;
	InclusionCond *inclCond;
	int nodePos;

	equiGraph = sortList(&equiGraph, compareEquiNode, true);

	// enumerate the nodes in the equi graph
	foreachi(lc, i, equiGraph)
	{
		cur = (EquiGraphNode *) lfirst(lc);
		cur->pos = i;
	}

	// store in hasStatic which nodes are connected through static paths.
	foreach(lc, equiGraph)
	{
		cur = (EquiGraphNode *) lfirst(lc);
		searchStaticPath (cur, cur, hasStatic, numNodes);
	}

	/* check for each attr entry in each rel entry if we have to add new
	 * AttrInclusions to model the equality constraints. */
	foreach(lc, rels)
	{
		rel = (CopyMapRelEntry *) lfirst(lc);

		foreach(innerLc, rel->attrEntries)
		{
			attr = (CopyMapEntry *) lfirst(innerLc);
			newAttrIncls = NIL;

			// check for each attrIncl if the
			foreach(attLc, attr->outAttrIncls)
			{
				attrIncl = (AttrInclusions *) lfirst(attLc);
				nodePos = getNodePos (equiGraph, attrIncl->attr);

				// included attr has no equality constraints
				if (nodePos == -1)
					continue;

				// add attr includes for static reachable nodes
				for (j = 0; j < (nodePos + 1) * nodeNum ; j++)
				{
					if (HAS_STATIC_POS(nodePos,j) && j != nodePos)
					{
						Var *toVar = (Var *) copyObject((EquiGraphNode *)
								list_nth(equiGraph, j))->nodeVar;
						newAttrIncl = makeAttrInclusions();
						newAttrIncl->attr = toVar;
						MAKE_EXISTS_INCL(inclCond, copyObject(attrIncl->attr));
						newAttrIncl->inclConds = list_make1(inclCond);

						newAttrIncls = lappend(newAttrIncls, newAttrIncl);
					}
				}

				/* add equality incl conds for paths to non static reachable
				 * nodes */
			}

			attr->outAttrIncls = list_concat(attr->outAttrIncls, newAttrIncls);
		}
	}
}

/*
 *
 */

static int
getNodePos (List *graph, Var *var)
{
	ListCell *lc;
	EquiGraphNode *node;

	foreach(lc, graph)
	{
		node = (EquiGraphNode *) lfirst(lc);

		if (node->nodeVar->varno == var->varno
				&& node->nodeVar->varattno == var->varattno)
		{
			return node->pos;
		}
	}

	return -1;
}

/*
 * Store in the matrix hasStatic which nodes have static paths between each
 * other.
 */


static void
searchStaticPath (EquiGraphNode *root, EquiGraphNode *node, bool *hasStatic,
		int numNodes)
{
	ListCell *lc;
	EquiGraphNode *cur;

	foreach(lc, node->sEdges)
	{
		cur = (EquiGraphNode *) lfirst(lc);

		if (!HASSTATIC(root, cur))
		{
			MARK_STATIC_PATH(root, cur);
			searchStaticPath (root, cur, hasStatic, numNodes);
		}
	}
}

/*
 * compare two EquiGraphNodes based on first their nodeVar varno's and second
 * on their varattno's.
 */

static int
compareEquiNode (void *left, void *right)
{
	EquiGraphNode *l = (EquiGraphNode *) left;
	EquiGraphNode *r = (EquiGraphNode *) right;

	if (l->nodeVar->varno < r->nodeVar->varno)
		return 1;
	if (l->nodeVar->varno > r->nodeVar->varno)
		return -1;
	if (l->nodeVar->varattno < r->nodeVar->varattno)
		return 1;
	if (l->nodeVar->varattno > r->nodeVar->varattno)
		return -1;
	return 0;
}

/*
 * Searches for equality conditions between simple vars in a qual condition and
 * returns these equalities as a list of lists of equivalent vars.
 */

static bool
findVarEqualitiesWalker (Node *node, VarEqualitiesContext *context)
{
	Var *left, *right;
	OpExpr *op;
	Value *useEqual;

	if (node == NULL)
		return false;

	// we found an equality
	if (IsA(node, OpExpr) && isEqualityOper((OpExpr *) node))
	{
		op = (OpExpr *) node;
		left = getVarFromTeIfSimple(linitial(op->args));
		right = getVarFromTeIfSimple(lsecond(op->args));

		// check that equality compares Vars (possibly casted)
		if (left == NULL && right == NULL)
			return false;

		if (!context->outerJoin && isInAndOrTop(node, context->root))
			useEqual = makeInteger(0L);
		else
			useEqual = makeInteger(1L);

		*context->result = lappend(*context->result, list_make3(left, right, useEqual));

		return false;
	}

	return expression_tree_walker(node, findVarEqualitiesWalker,
			(void *) context);
}

/*
 * Gets all AttrInclusions that reference a var
 */

static bool
addEqualityCondsForVarsWalker (CopyMapRelEntry *entry, CopyMapEntry *attr,
		 void *context)
{
	ListCell *lc;
	Var *left  = (Var *) linitial((List *) context);
	Var *right = (Var *) lsecond((List *) context);
	Value *useEqual = (Value *) lthird((List *) context);
	AttrInclusions *attrIncl;
	InclusionCond *cond;

	// walk through all attribute inclusions and if either left or right is found
	foreach(lc, attr->outAttrIncls)
	{
		attrIncl = (AttrInclusions *) lfirst(lc);
		// is AttrInclusion for left?
		if (equal(incl->attr, left))
		{
			if (intVal(useEqual) == 0)
			{
				if (!includionCondWalker(incl, hasExistsForVar, right))
				{
					MAKE_EQUAL_INCL
				}
			}
			else
			{
			// if the equality condition for left and right does not exist, add it.
				if (!inclusionCondWalker(incl, hasEqualityForVar, right))
				{
					MAKE_EQUAL_INCL(cond, (Node *) left, right);
					incl->inclConds = lappend(incl->inclConds, cond);
				}
			}
		}
		if (equal(incl->attr, right))
		{
			// if the equality condition for left and right does not exist, add it.
			if (!inclusionCondWalker(incl, hasEqualityForVar, left))
			{
				MAKE_EQUAL_INCL(cond, (Node *) right, left);
				incl->inclConds = lappend(incl->inclConds, cond);
			}
		}
	}

	return true;
}

/*
 *
 */

static bool
hasEqualityForVar (InclusionCond *cond, void *context)
{
	if (equal(cond->eqVar, context))
		return true;

	return false;
}

/*
 *
 */

static bool
hasExistsForVar (InclusionCond *cond, void *context)
{
	if (equal(cond->existsAttr, context))
		return true;

	return false;
}

/*
 * From a list of CopyMaps return the one that represents RTE with index "rtindex".
 */

static CopyMap *
getCopyMapForRtindex (List *maps, Index rtindex)
{
	CopyMap *result;
	ListCell *lc;

	foreach(lc, maps)
	{
		result = (CopyMap *) lfirst(lc);

		if (result->rtindex == rtindex)
			return result;
	}

	return NULL;
}

/*
 * Copies the outVars list to the inVars list and deletes the outVars list for each CopyMapRelEntry
 * of a CopyMap.
 */

static void
swtichInAndOutVars (CopyMap *map)
{
//	ListCell *lc, *innerLc;
//	CopyMapEntry *attrEntry;
//	CopyMapRelEntry *entry;
//
//	foreach(lc, map->entries)
//	{
//		entry = (CopyMapRelEntry *) lfirst(lc);
//
//		/* generate the new Var mapping for each attr */
//		foreach(innerLc, entry->attrEntries)
//		{
//			attrEntry = (CopyMapEntry *) lfirst(innerLc);
//			attrEntry->inVars = attrEntry->outVars;
//			attrEntry->outVars = NIL;
//		}
//	}
}

/*
 * Maps each var of a copy map attr entry used in a join to the output var of the join.
 */

static void
mapJoinAttrs (CopyMapEntry *attr, List *joinVars, Index rtindex)
{
//	ListCell *lc, *innerLc;
//	List *newIns;
//	Var *joinVar;
//	Var *newVar;
//	Var *attrVar;
//	int i;
//
//	newIns = copyObject(attr->inVars);
//
//	foreach(lc, attr->inVars)
//	{
//		attrVar = (Var *) lfirst(lc);
//
//		foreachi(innerLc, i, joinVars)
//		{
//			joinVar = (Var *) lfirst(innerLc);
//
//			/* found the in Var, map it to joinVar */
//			if(equal(joinVar, attrVar))
//			{
//				newVar = makeVar(rtindex, i + 1, joinVar->vartype, joinVar->vartypmod, 0);
//				newIns = lappend(newIns, newVar);
//			}
//		}
//	}
//
//	attr->inVars = newIns;
}


/*
 * Top-down traversal to set the propagate flags, such that each query nodes
 * copy map contains the information which subqueries should be rewritten.
 */

static void
setPropagateFlags (Query *query)
{
//	CopyMapRelEntry *entry;
//	ListCell *lc;
//
//	foreach(lc, GetInfoCopyMap(query)->entries)
//	{
//		entry = (CopyMapRelEntry *) lfirst(lc);
//
//		entry->propagate = isPropagating(entry);
//	}
//
//	setPropagateFlagsForQueryNode(query);
}

/*
 *
 */

static void
setPropagateFlagsForQueryNode (Query *query)
{
//	CopyMapRelEntry *superEntry;
//	CopyMapRelEntry *subEntry;
//	RangeTblEntry *rte;
//	CopyMap *subCopyMap;
//	ListCell *lc;
//
//	/* for each base relation entry, check if it is not propagating, and if so
//	 *  push this info down into the child query nodes copy maps. */
//	foreach(lc, GetInfoCopyMap(query)->entries)
//	{
//		superEntry = (CopyMapRelEntry *) lfirst(lc);
//
//		if(!superEntry->propagate)
//		{
//			rte = rt_fetch(superEntry->rtindex, query->rtable);
//
//			if(rte->rtekind == RTE_SUBQUERY)
//			{
//				subCopyMap = GetInfoCopyMap(rte->subquery);
//
//				subEntry = getRelEntry(subCopyMap, superEntry->relation,
//						superEntry->refNum);
//				subEntry->propagate = false;
//			}
//		}
//	}
//
//	/* decend into tree */
//	foreach(lc, query->rtable)
//	{
//		rte = (RangeTblEntry *) lfirst(lc);
//
//		if(rte->rtekind == RTE_SUBQUERY)
//			setPropagateFlagsForQueryNode(rte->subquery);
//	}

}


/*
 * Merges to CopyMap structs by appending their lists of entries. A new allocated CopyMap is
 * returned with all entries is returned.
 */
static CopyMap *
mergeMaps (CopyMap *left, CopyMap *right)
{
	CopyMap *result;

	result = makeCopyMap();
	result->entries = copyObject(left->entries);
	result->entries = list_concat(result->entries, copyObject(right->entries));

	return result;
}

/*
 * Delete the inVars and outVars lists of each CopyMapRelEntry of a CopyMap.
 */

static void
emptyMap (CopyMap *map)
{
//	ListCell *lc, *attrLc;
//	CopyMapRelEntry *relEntry;
//	CopyMapEntry *attr;
//
//	foreach(lc, map->entries)
//	{
//		relEntry = (CopyMapRelEntry *) lfirst(lc);
//
//		foreach(attrLc, relEntry->attrEntries)
//		{
//			attr = (CopyMapEntry *) lfirst(attrLc);
//
//			attr->inVars = NIL;
//			attr->outVars = NIL;
//		}
//	}
}

/*
 * Generates a CopyMap struct for a base relation.
 */

static CopyMap *
generateBaseRelCopyMap (RangeTblEntry *rte, Index rtindex)
{
	CopyMap *result;
	CopyMapRelEntry *relMap;
	List *vars;
	List *names;
	ListCell *varLc, *nameLc;
	Var *var;
	CopyMapEntry *attrEntry;
	AttrInclusions *attrIncl;

	vars = NIL;
	names = NIL;
	result = makeCopyMap();
	result->rtindex = rtindex;
	relMap = makeCopyMapRelEntry ();

	relMap->relation = rte->relid;
	relMap->refNum = getRelationRefNum (rte->relid, true);
	relMap->rtindex = rtindex;
	relMap->isStatic = true;

	/* get Attributes */
	expandRTE(rte, rtindex, 0, false, &names, &vars);

	forboth(varLc, vars, nameLc, names)
	{
		var = (Var *) lfirst(varLc);

		attrIncl = makeAttrInclusions();
		attrIncl->attr = var;
		attrIncl->isStatic = true;

		attrEntry = makeCopyMapEntry();
		attrEntry->baseRelAttr = var;
		attrEntry->provAttrName = createProvAttrName(rte, strVal((Value *) lfirst(nameLc)));
		attrEntry->isStaticTrue = true;
		attrEntry->isStaticFalse = false;
		attrEntry->outAttrIncls = list_make1(attrIncl);

		relMap->attrEntries = lappend(relMap->attrEntries, attrEntry);
	}

	result->entries = list_make1(relMap);

	return result;
}

/*
 * Set the rtindex attribute of each CopyMapRelEntry in a CopyMap to "rtindex". This function is used if the CopyMap of a RTE is
 * derived from the CopyMap of the subquery of the RTE.
 */

static void
setCopyMapRTindices (CopyMap *map, Index rtindex)
{
	ListCell *lc;
	CopyMapRelEntry *entry;

	foreach(lc, map->entries)
	{
		entry = (CopyMapRelEntry *) lfirst(lc);

		entry->rtindex = rtindex;
	}
}

/*
 * Analyze each base relation entry of the top query node's copy map to infer
 * if (1) we have to rewrite the range tbl entry that rel refers too and (2) if
 * this rel entry is static. I.e., if the provenance from this base relation
 * will always be included unconditionally in the provenance of the whole
 * query.
 */

static void
inferStaticCopy (Query *query, ContributionType contr)
{
	ListCell *lc;
	ListCell *attrLc;
	CopyMap *map = GET_COPY_MAP(query);
	CopyMapRelEntry *relEntry;
	CopyMapEntry *attrEntry;
	bool noRewrite, isStaticTrue;

	/* infer if CopyMapEntries are static for the query node and its children
	 */
	inferStaticCopyQueryNode(query, contr);

	// check for each rel entry if it is static
	foreach(lc, map->entries)
	{
		relEntry= (CopyMapRelEntry *) lfirst(lc);

		if(relEntry->noRewrite || relEntry->isStatic)
			continue;

		/* for partial copy CS types we have to find one static attr entry to
		 * infer that the entry is static.
		 */
		if (IS_PARTIALC(contr))
		{
			noRewrite = !(isStaticTrue = false);

			foreach(attrLc, relEntry->attrEntries)
			{
				attrEntry = (CopyMapEntry *) lfirst(attrLc);

				/* if one attr entry is always static and has at most one entry
				 * the rel entry is static and we have to rewrite the part of
				 * the query where this entry originates from. */
				if (attrEntry->isStaticTrue)
					noRewrite = !(isStaticTrue = true);
				/* if there is at least on non static entry we have to rewrite
				 * too */
				else if (!attrEntry->isStaticFalse)
					noRewrite = false;
			}
		}
		/* for complete copy CS types a rel entry is static if all attr entries
		 * are static */
		else
		{
			noRewrite = !(isStaticTrue = true);

			foreach(attrLc, relEntry->attrEntries)
			{
				attrEntry = (CopyMapEntry *) lfirst(attrLc);

				/* if at least one entry is static false we do not have to
				 * rewrite the part of the query this rel entry originates
				 * from (if all other entries from that part of the query are
				 * also static false) */
				if (attrEntry->isStaticFalse)
					noRewrite = !(isStaticTrue = false);
				/* if there is at least one non static entry the rel entry is
				 * not static */
				else if (!attrEntry->isStaticTrue)
					isStaticTrue = false;
			}
		}

		relEntry->isStatic = isStaticTrue;
		relEntry->noRewrite = noRewrite;
	}
}

/*
 *
 */

static void
inferStaticCopyQueryNode (Query *query, ContributionType contr)
{
	ListCell *lc, *attrLc;
	CopyMap *map = GET_COPY_MAP(query);
	CopyMapRelEntry *relEntry;
	CopyMapEntry *attrEntry;
	RangeTblEntry *rte;

	// analyze subquery copy maps first
	foreach(lc, query->rtable)
	{
		rte= (RangeTblEntry *) lfirst(lc);

		if (rte->rtekind == RTE_SUBQUERY)
			inferStaticCopyQueryNode(rte->subquery, contr);
	}

	// check static for each attr entry is it is static
	foreach(lc, map->entries)
	{
		relEntry = (CopyMapRelEntry *) lfirst(lc);

		foreach(attrLc, relEntry->attrEntries)
		{
			attrEntry = (CopyMapEntry *) lfirst(attrLc);

			analyzeAttrPaths(attrEntry, relEntry);
		}
	}
}

/*
 *
 */

static void
analyzeAttrPaths (CopyMapEntry *attr, CopyMapRelEntry *relEntry)
{
	ListCell *lc, *innerLc, *condLc;
	AttrInclusions *attrIncl;
	AttrInclusions *innerIncl;
	InclusionCond *inclCond;
	InclusionCond *innerCond;
	StaticType staticType;

	attr->isStaticTrue = false;
	attr->isStaticFalse = true;

	foreach(lc, attr->outAttrIncls)
	{
		attrIncl = (AttrInclusions *) lfirst(lc);
		attrIncl->isStatic = false;

		foreach(condLc, attrIncl->inclConds)
		{
			inclCond = (InclusionCond *) lfirst(condLc);

			// does the condition use a nested AttrInclusions?
			if(IsA(inclCond->existsAttr, AttrInclusions))
			{
				innerIncl = (AttrInclusions *) inclCond->existsAttr;
				innerIncl->isStatic = false;

				/* InclusionsConds with nested AttrInclusions are either EXISTS
				 *  or IF inclusions */
				if (inclCond->inclType == INCL_EXISTS)
				{
					// check each inner inclusion condition
					foreach(innerLc, innerIncl->inclConds)
					{
						innerCond = (InclusionCond *) lfirst(innerLc);

						staticType = getInclusionStaticType(
								(Var *) innerCond->existsAttr,
								attr->baseRelAttr, relEntry->child);

						// is EXISTS and guaranteed to be true
						if (innerCond->inclType == INCL_EXISTS &&
								staticType == STATIC_TRUE)
						{
								attrIncl->isStatic = true;
								innerIncl->isStatic = true;
								attr->isStaticTrue = true;
						}

						/* is not static false then attr entry is not static false */
						if (staticType != STATIC_FALSE)
							attr->isStaticFalse = false;
					}
				}
			}
			// is a simple inclusion condition without nesting
			else
			{
				staticType = getInclusionStaticType(
						(Var *) inclCond->existsAttr,
						attr->baseRelAttr, relEntry->child);

				/* is an EXISTS and guaranteed to be true */
				if (inclCond->inclType == INCL_EXISTS
						&& staticType == STATIC_TRUE)
				{
						attrIncl->isStatic = true;
						attr->isStaticTrue = true;
				}

				/* is not static false then attr entry is not static false */
				if (staticType != STATIC_FALSE)
					attr->isStaticFalse = false;
			}
		}
	}
}

/*
 *
 */

static StaticType
getInclusionStaticType (Var *inclVar, Var *baseVar, CopyMapRelEntry *rel)
{
	ListCell *lc;
	CopyMapEntry *attr;
	AttrInclusions *incl;

	// find CopyMapEntry for the base relation attribute
	foreach(lc, rel->attrEntries)
	{
		attr = (CopyMapEntry *) lfirst(lc);

		if (attr->baseRelAttr->varattno == baseVar->varattno)
			break;
	}

	if (attr->isStaticTrue)
		return STATIC_TRUE;
	if (attr->isStaticFalse)
		return STATIC_FALSE;

	// search for the attr inclusion for inclVar
	foreach(lc, attr->outAttrIncls)
	{
		incl = (AttrInclusions *) lfirst(lc);

		// attr inclusion is static, return STATIC_TRUE
		if (incl->attr->varattno == inclVar->varattno && incl->isStatic)
			return STATIC_TRUE;
	}

	return NOT_STATIC;
}

/*
 *
 */

//static void
//inferStaticAttrEntry (CopyMapEntry *attr, CopyMapRelEntry *relEntry)
//{
//	AttrInclusions *attrIncl;
//
//	// no attr inclusions means that this attr will never in the copy map
//	if (list_length(attr->outAttrIncls) == 0)
//		attr->staticValue = !(attr->isStatic = attr->minOneStatic = true);
//
//
//}



/*
 * Checks if the provenance of a baserelation represented by a CopyMapRelEntry
 * should be propagated further up in the query. For CCT-CS it should be
 * propagated if for each base relation attribute there is at least one
 * attribute in the query that is directly copied from this base relation
 * attribute.
 */

bool
isPropagating (CopyMapRelEntry *entry)
{
//	ListCell *lc;
//	CopyMapEntry *attr;
//
//	foreach(lc, entry->attrEntries)
//	{
//		attr = (CopyMapEntry *) lfirst(lc);
//
//		if(!attr->outVars)
//			return false;
//	}

	return true;
}

/*
 * Checks if a query node should be rewritten.
 */

bool
shouldRewriteQuery (Query *query)
{
	CopyMap *map;
	ListCell *lc;
	CopyMapRelEntry *entry;

	map = GET_COPY_MAP(query);

	foreach(lc, map->entries)
	{
		entry = (CopyMapRelEntry *) lfirst(lc);

		if (!entry->noRewrite)
			return true;
	}

	return false;
}

/*
 * Checks if a range table entry of a query should be rewritten according to
 * copy contribution semantics. This information is stored in the provided copy
 * map.
 */

bool
shouldRewriteRTEforMap (CopyMap *map, Index rtindex)
{
	ListCell *lc;
	CopyMapRelEntry *entry;

	if(!map)
		return false;

	foreach(lc, map->entries)
	{
		entry = (CopyMapRelEntry *) lfirst(lc);

		if ((rtindex == -1 || entry->rtindex == rtindex) && !entry->noRewrite)
			return true;
	}

	return false;
}

/*
 *
 */

List *
getAllEntriesForRTE (CopyMap *map, Index rtindex)
{
	List *result;
	ListCell *lc;
	CopyMapRelEntry *entry;

	result = NIL;

	foreach(lc, map->entries)
	{
		entry = (CopyMapRelEntry *) lfirst(lc);

		if (entry->rtindex == rtindex)
			result = lappend(result, entry);
	}

	return result;
}

/*
 * Extract the CopyMapRelEntry for the RTE with index = rtindex from a CopyMap.
 */

CopyMapRelEntry *
getEntryForBaseRel (CopyMap *map, Index rtindex)
{
	CopyMapRelEntry *cur;
	ListCell *lc;

	foreach(lc, map->entries)
	{
		cur = (CopyMapRelEntry *) lfirst(lc);

		if (cur->rtindex == rtindex)
			return cur;
	}

	return NULL;
}

/*
 * Get the CopyMapRelEntry for the "refNum"th reference to the relation with
 * Oid "rel".
 */

static CopyMapRelEntry *
getRelEntry (CopyMap *map, Oid rel, int refNum)
{
	CopyMapRelEntry *entry;
	ListCell *lc;

	foreach(lc, map->entries)
	{
		entry = (CopyMapRelEntry *) lfirst(lc);

		if (entry->relation == rel && entry->refNum == refNum)
			return entry;
	}

	return NULL;
}

/*
 * A generic walker that applies a function to each of the relentries and attr
 * entries of a copy map.
 */

void
copyMapWalker (List *entries, void *context, void *attrContext, void *inclContext,
				bool (*relWalker) (CopyMapRelEntry *entry, void *context),
				bool (*attrWalker) (CopyMapRelEntry *entry, CopyMapEntry *attr,
						void *context),
				bool (*inclWalker) (CopyMapRelEntry *entry, CopyMapEntry *attr,
						AttrInclusions *incl, void *context))
{
	CopyMapRelEntry *entry;
	CopyMapEntry *attr;
	AttrInclusions *incl;
	ListCell *lc;
	ListCell *attrLc;
	ListCell *inclLc;

	foreach(lc, entries)
	{
		entry = (CopyMapRelEntry *) lfirst(lc);

		/* if a relWalker is given apply it an skip the processing of its
		 * entries if it returns false.
		 */
		if (relWalker)
			if (!relWalker (entry, context))
				continue;

		if (attrWalker)
		{
			foreach(attrLc, entry->attrEntries)
			{
				attr = (CopyMapEntry *) lfirst(attrLc);

				if (attrWalker(entry, attr, attrContext))
				{
					if (inclWalker)
					{
						foreach(inclLc, attr->outAttrIncls)
						{
							incl = (AttrInclusions *) lfirst(inclLc);
							inclWalker(entry, attr, incl, inclContext);
						}
					}
				}
			}
		}
	}
}

/*
 * Applies function to all InclusionCond's of an AttrInclusion.
 */

bool
inclusionCondWalker (AttrInclusions *incl,
		bool (*condWalker) (InclusionCond *cond, void *context), void *context)
{
	ListCell *lc;
	InclusionCond *cond;

	foreach(lc, incl->inclConds)
	{
		cond = (InclusionCond *) lfirst(lc);

		if (condWalker(cond, context))
			return true;
	}

	return false;
}

/*
 * Dummy walkers to allow processing of all subelements
 */

bool
dummyAttrWalker (CopyMapRelEntry *entry, CopyMapEntry *attr, void *context)
{
	return true;
}
