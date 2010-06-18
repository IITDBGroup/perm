/*-------------------------------------------------------------------------
 *
 * prov_copy_map.c
 *	  Support data structure for copy-contribution semantics. Basically a list of attribute equivalences for
 *	  each base relation accessed by a provenance query.
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
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/provlog.h"
#include "provrewrite/provattrname.h"
#include "provrewrite/prov_copy_map.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/provstack.h"


//static void generateCopyMapForQuery (Query *query);
static CopyMap *generateCopyMapCDCForQuery (Query *query, Index rtindex);
static CopyMap *generateBaseRelCopyMap (RangeTblEntry *rte, Index rtindex);

static void setPropagateFlags (Query *query);
static void setPropagateFlagsForQueryNode (Query *query);

static void handleSetOps (Query *query, List *subMaps);
static CopyMap *handleSetOp (Query *query, List *subMaps, Node *setOp);
static void generateAggCopyMap (Query *query, Index rtindex);
static void handleProjectionCopyMap (Query *query, Index rtindex);
static void mapChildVarsToTargetEntries (Query *query, Index rtindex, List *tes);
static CopyMap *handleJoins (Query *query, List *subMaps);
static CopyMap *handleJoinTreeItem (Query *query, List *subMaps, Node *joinItem);

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
 * Generate the CopyMap data structure for a query. A CopyMap is produced for each query node in the query and is stored in this query nodes
 * ProvInfo field. The CopyMap of a query node q stores the information about which attributes of the base relations accessed by q are
 * copied to the result of q. This information is needed for copy contribution semantics provenance rewrite to know which parts of a query
 * should be rewritten to propagate provenance. The map generation differs depending on the type of copy contribution semantics (C-CS) used:
 *
 * 	CDC-CS: Only the provenance of base relations for which each attribute is directly copied are propagated.
 * 	CTC-CS: Only the provenance of base relations for which each attribute is transitively copied are propagated.
 * 			(e.g. by WHERE equality conditions)
 * 	PDC-CS: Only the provenance of base relations for which at least one attribute is directly copied are propagated.
 * 	PTC-CS: Only the provenance of base relations for which at least one attribute is transitively copied are propagated.
 */

void
generateCopyMaps (Query *query)
{
	switch(ContributionType(query))
	{
	case CONTR_COPY_COMPLETE_TRANSITIVE:
		break;
	case CONTR_COPY_COMPLETE_NONTRANSITIVE:
		generateCopyMapCDCForQuery(query, 1);
		setPropagateFlags(query);
		break;
	case CONTR_COPY_PARTIAL_TRANSITIVE:
		break;
	case CONTR_COPY_PARTIAL_NONTRANSITIVE:
		break;
	default:
		//TODO error
		break;
	}
}

/*
 * Top-down traversal to set the propagate flags, such that each query nodes copy map contains the information which subqueries should be
 * rewritten.
 */

static void
setPropagateFlags (Query *query)
{
	CopyMapRelEntry *entry;
	ListCell *lc;

	foreach(lc, GetInfoCopyMap(query)->entries)
	{
		entry = (CopyMapRelEntry *) lfirst(lc);

		entry->propagate = isPropagating(entry);
	}

	setPropagateFlagsForQueryNode(query);
}

/*
 *
 */

static void
setPropagateFlagsForQueryNode (Query *query)
{
	CopyMapRelEntry *superEntry;
	CopyMapRelEntry *subEntry;
	RangeTblEntry *rte;
	CopyMap *subCopyMap;
	ListCell *lc;

	/* for each base relation entry, check if it is not propagating, and if so push this info down into the
	 * child query nodes copy maps.
	 */
	foreach(lc, GetInfoCopyMap(query)->entries)
	{
		superEntry = (CopyMapRelEntry *) lfirst(lc);

		if(!superEntry->propagate)
		{
			rte = rt_fetch(superEntry->rtindex, query->rtable);

			if(rte->rtekind == RTE_SUBQUERY)
			{
				subCopyMap = GetInfoCopyMap(rte->subquery);

				subEntry = getRelEntry(subCopyMap, superEntry->relation, superEntry->refNum);
				subEntry->propagate = false;
			}
		}
	}

	/* decend into tree */
	foreach(lc, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		if(rte->rtekind == RTE_SUBQUERY)
			setPropagateFlagsForQueryNode(rte->subquery);
	}

}

/*
 *
 */

static CopyMap *
generateCopyMapCDCForQuery (Query *query, Index rtindex)
{
	ListCell *lc;
	RangeTblEntry *rte;
	List *subMaps;
	CopyMap *currentMap;
	int i;

	//TODO add empty maps for sublinks at first

	subMaps = NIL;

	/* call us self to generate Copy maps of subqueries */
	foreachi(lc, i, query->rtable)
	{
		rte = (RangeTblEntry *) lfirst(lc);

		switch(rte->rtekind)
		{
		case RTE_SUBQUERY:
			currentMap = generateCopyMapCDCForQuery(rte->subquery, i + 1);
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
 	}

	/* set operation query */
	if (query->setOperations)
		handleSetOps(query, subMaps);

	/* SPJ or ASPJ */
	else
	{
		Provinfo(query)->copyInfo = (Node *) handleJoins(query, subMaps);

		/* use these to generate copy map for the current query */
		if (query->hasAggs)
			generateAggCopyMap(query, rtindex);
		else
			handleProjectionCopyMap(query, rtindex);
	}

	GetInfoCopyMap(query)->rtindex = rtindex;

	return copyObject(GetInfoCopyMap(query));
}

/*
 * Create the CopyMap for an set operation query from the CopyMaps of its range table entries.
 */

static void
handleSetOps (Query *query, List *subMaps)
{
	CopyMap *map;
	ListCell *lc, *innerLc, *attrLc;
	CopyMapRelEntry *relEntry;
	CopyMapEntry *attr;
	Var *var;

	map = handleSetOp (query, subMaps, query->setOperations);

	foreach(lc, map->entries)
	{
		relEntry = (CopyMapRelEntry *) lfirst(lc);

		foreach(innerLc, relEntry->attrEntries)
		{
			attr = (CopyMapEntry *) lfirst(innerLc);

			attr->inVars = copyObject(attr->outVars);

			foreach(attrLc, attr->outVars)
			{
				var = (Var *) lfirst(attrLc);

				var->varno = 1;//CHECK OK?
			}

		}
	}

	Provinfo(query)->copyInfo = (Node *) map;
}


/*
 * Create the CopyMap for a set operation from the CopyMaps of its left and right input.
 */

static CopyMap *
handleSetOp (Query *query, List *subMaps, Node *setOp)
{
	CopyMap *rMap;
	CopyMap *lMap;
	SetOperationStmt *setOper;
	RangeTblRef *rtRef;

	if(IsA(setOp, SetOperationStmt))
	{
		setOper = (SetOperationStmt *) setOp;

		/* get maps for children */
		lMap = handleSetOp(query, subMaps, setOper->larg);
		rMap = handleSetOp(query, subMaps, setOper->rarg);

		/* merge child maps */
		switch(setOper->op)
		{
		case SETOP_EXCEPT:
			emptyMap(rMap);
		case SETOP_UNION:
		case SETOP_INTERSECT:
			return mergeMaps(lMap,rMap);
		default:
			return NULL; //TODO error
		}

	}
	else
	{
		rtRef = (RangeTblRef *) setOp;
		lMap = (CopyMap *) getCopyMapForRtindex(subMaps, rtRef->rtindex);

		return lMap;
	}
}

/*
 * Create the CopyMap for an aggregation. In an aggregation only the group by attributes are considered to be copied
 * from the input of the aggregation.
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
 * Given a list of target list entries of a query adapt the outVars lists of the CopyMapRelEntries of
 * the query accoding to which attributes are copied by the target list entries.
 *
 * E.g. for R(a,b,c)
 *
 * 		SELECT a, a + b, c AS dummy FROM r
 *
 * Here a and c are copied, but b is not because it is only used in the expression (a + b).
 */

static void
mapChildVarsToTargetEntries (Query *query, Index rtindex, List *tes)
{
	List *simpleVars;
	List *tePos;
	ListCell *lc, *innerLc, *attLc, *teLc;
	Var *var;
	Var *inVar;
	Var *teVar;
	TargetEntry *te;
	CopyMap *map;
	CopyMapRelEntry *relEntry;
	CopyMapEntry *attr;
	int pos;

	map = GetInfoCopyMap(query);
	tePos = NIL;
	simpleVars = NIL;

	/* get all simple vars in the target list */
	foreach(lc, tes)
	{
		te = (TargetEntry *) lfirst(lc);

		if (!te->resjunk)
		{
			var = getVarFromTeIfSimple((Node *) te->expr);

			simpleVars = lappend(simpleVars, var);
			tePos = lappend_int(tePos, te->resno);
		}
	}

	/* loop through entries and adapt out var lists */
	foreach(lc, map->entries)
	{
		relEntry = (CopyMapRelEntry *) lfirst(lc);

		foreach(innerLc, relEntry->attrEntries)
		{
			attr = (CopyMapEntry *) lfirst(innerLc);

			foreach(attLc, attr->inVars)
			{
				inVar = (Var *) lfirst(attLc);

				foreachi(teLc, pos, simpleVars) {
					teVar = (Var *) lfirst(teLc);

					if (teVar && teVar->varno == inVar->varno && teVar->varattno == inVar->varattno)
					{
						var = makeVar(rtindex, list_nth_int(tePos,pos), teVar->vartype, teVar->vartypmod, 0);//TODO exprType anpassen?
						attr->outVars = lappend(attr->outVars, var);
					}
				}
			}
		}
	}
}

/*
 * Process the jointree of a query to create a CopyMap for the top level jointree node from the
 * CopyMaps of the range table entries of the query. These are provided as parameter subMaps.
 */

static CopyMap *
handleJoins (Query *query, List *subMaps)
{
	ListCell *lc;
	Node *joinItem;
	List *joinMaps;
	CopyMap *map;

	joinMaps = NIL;

	/* get the copy map for each from list item */
	foreach(lc, query->jointree->fromlist)
	{
		joinItem = (Node *) lfirst(lc);

		map = handleJoinTreeItem(query, subMaps, joinItem);
		joinMaps = lappend(joinMaps, map);
	}

	/* compose the individual lists */
	lc = list_head(joinMaps);
	map = (CopyMap *) lfirst(lc);

	for(lc = lc->next; lc != NULL; lc = lc->next)
		map = mergeMaps(map, lfirst(lc));

	return map;
}

/*
 * Create the CopyMap for a node in a join tree from the CopyMaps of its left and right child or
 * RTE-CopyMap in case of a leaf node.
 */

static CopyMap *
handleJoinTreeItem (Query *query, List *subMaps, Node *joinItem)
{
	JoinExpr *join;
	RangeTblRef *rtRef;
	CopyMap *lMap, *rMap, *result;
	CopyMapRelEntry *entry;
	CopyMapEntry *attrEntry;
	ListCell *lc, *innerLc;
	List *joinVars;

	if(IsA(joinItem, JoinExpr))
	{
		join = (JoinExpr *) joinItem;

		joinVars = ((RangeTblEntry *) rt_fetch(join->rtindex, query->rtable))->joinaliasvars;

		/* get children maps */
		lMap = handleJoinTreeItem (query, subMaps, join->larg);
		rMap = handleJoinTreeItem (query, subMaps, join->rarg);

		/* compose map for join */
		result = mergeMaps(lMap, rMap);

		foreach(lc, result->entries)
		{
			entry = (CopyMapRelEntry *) lfirst(lc);
//			entry->rtindex = join->rtindex;

			/* generate the new Var mapping for each attr */
			foreach(innerLc, entry->attrEntries)
			{
				attrEntry = (CopyMapEntry *) lfirst(innerLc);

				mapJoinAttrs(attrEntry, joinVars, join->rtindex);
			}
		}

		return result;
	}
	else
	{
		rtRef = (RangeTblRef *) joinItem;

		result = getCopyMapForRtindex(subMaps, rtRef->rtindex);
		swtichInAndOutVars(result);

		return result;
	}
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
	ListCell *lc, *innerLc;
	CopyMapEntry *attrEntry;
	CopyMapRelEntry *entry;

	foreach(lc, map->entries)
	{
		entry = (CopyMapRelEntry *) lfirst(lc);

		/* generate the new Var mapping for each attr */
		foreach(innerLc, entry->attrEntries)
		{
			attrEntry = (CopyMapEntry *) lfirst(innerLc);
			attrEntry->inVars = attrEntry->outVars;
			attrEntry->outVars = NIL;
		}
	}
}

/*
 * Maps each var of a copy map attr entry used in a join to the output var of the join.
 */

static void
mapJoinAttrs (CopyMapEntry *attr, List *joinVars, Index rtindex)
{
	ListCell *lc, *innerLc;
	List *newIns;
	Var *joinVar;
	Var *newVar;
	Var *attrVar;
	int i;

	newIns = copyObject(attr->inVars);

	foreach(lc, attr->inVars)
	{
		attrVar = (Var *) lfirst(lc);

		foreachi(innerLc, i, joinVars)
		{
			joinVar = (Var *) lfirst(innerLc);

			/* found the in Var, map it to joinVar */
			if(equal(joinVar, attrVar))
			{
				newVar = makeVar(rtindex, i + 1, joinVar->vartype, joinVar->vartypmod, 0);
				newIns = lappend(newIns, newVar);
			}
		}
	}

	attr->inVars = newIns;
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
	ListCell *lc, *attrLc;
	CopyMapRelEntry *relEntry;
	CopyMapEntry *attr;

	foreach(lc, map->entries)
	{
		relEntry = (CopyMapRelEntry *) lfirst(lc);

		foreach(attrLc, relEntry->attrEntries)
		{
			attr = (CopyMapEntry *) lfirst(attrLc);

			attr->inVars = NIL;
			attr->outVars = NIL;
		}
	}
}

/*
 * generates a copyMap struct for a base relation.
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

	vars = NIL;
	names = NIL;
	result = makeCopyMap();
	result->rtindex = rtindex;
	relMap = makeCopyMapRelEntry ();

	relMap->relation = rte->relid;
	relMap->refNum = getRelationRefNum (rte->relid, true);
	relMap->rtindex = rtindex;

	/* get Attributes */
	expandRTE(rte, rtindex, 0, false, &names, &vars);

	forboth(varLc, vars, nameLc, names)
	{
		var = (Var *) lfirst(varLc);

		attrEntry = makeCopyMapEntry();
		attrEntry->baseRelAttr = var;
		attrEntry->provAttrName = createProvAttrName(rte, strVal((Value *) lfirst(nameLc)));
		attrEntry->inVars = list_make1(var);
		attrEntry->outVars = list_make1(var);

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
 * Checks if the provenance of a baserelation represented by a CopyMapRelEntry should be propagated further up in the query.
 * For CCT-CS it should be propagated if for each base relation attribute there is at least one attribute in the query that
 * is directly copied from this base relation attribute.
 */

bool
isPropagating (CopyMapRelEntry *entry)
{
	ListCell *lc;
	CopyMapEntry *attr;

	foreach(lc, entry->attrEntries)
	{
		attr = (CopyMapEntry *) lfirst(lc);

		if(!attr->outVars)
			return false;
	}

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

	map = GetInfoCopyMap(query);

	foreach(lc, map->entries)
	{
		entry = (CopyMapRelEntry *) lfirst(lc);

		if (entry->propagate)
			return true;
	}

	return false;
}

/*
 * Checks if a range table entry of a query should be rewritten according to copy contribution semantics. This information
 * is stored in the provided copy map.
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

		if ((rtindex == -1 || entry->rtindex == rtindex) && entry->propagate)
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
 * Get the CopyMapRelEntry for the "refNum"'th reference to the relation with Oid "rel".
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
 * A generic walker that applies a function to each of the relentries and attr entries of a copy map.
 */

void
copyMapWalker (CopyMap *map, void *context, void *attrContext,
				bool (*relWalker) (CopyMapRelEntry *entry, void *context),
				bool (*attrWalker) (CopyMapRelEntry *entry, CopyMapEntry *attr, void *context))
{
	CopyMapRelEntry *entry;
	CopyMapEntry *attr;
	ListCell *lc;
	ListCell *attrLc;

	foreach(lc, map->entries)
	{
		entry = (CopyMapRelEntry *) lfirst(lc);

		if (relWalker)
			relWalker (entry, context);

		if (attrWalker)
		{
			foreach(attrLc, entry->attrEntries)
			{
				attr = (CopyMapEntry *) lfirst(attrLc);

				attrWalker(entry, attr, attrContext);
			}
		}
	}
}
