/*-------------------------------------------------------------------------
 *
 * prov_trans_map.c
 *	  POSTGRES C
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/transformationp/prov_trans_map.c,v 1.542 26.10.2009 18:37:41 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "nodes/parsenodes.h"
#include "utils/varbit.h"
#include "parser/parsetree.h"
#include "nodes/value.h"

#include "provrewrite/provstack.h"
#include "provrewrite/prov_util.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/prov_trans_map.h"
#include "provrewrite/prov_trans_util.h"
#include "provrewrite/prov_trans_bitset.h"

/* methods */
static void generateMappingBitsets (Query *query, List **bitsets, List **annotations);
static List *gatherAnnotations (Query *query, Node *info, TransProvInfo *curInfo);
static void unionBitsets (TransSubInfo *info, List *bitsets, List *annotations);

/*
 *
 */

void
generateMapString (Query *query, TransRepQueryInfo *repInfo,
		MemoryContext funcPrivateCntx)
{
	List *bitSets;
	List *annotations;
	int i, numMappings;
	ListCell *llc, *rlc;
	Value *annot;
	VarBit *set;
	MemoryContext oldCtx;

	bitSets = NIL;
	annotations = NIL;
	generateMappingBitsets(query, &bitSets, &annotations);
	numMappings = list_length(bitSets);

	/* switch to mapprov function private memory context */
	oldCtx = MemoryContextSwitchTo(funcPrivateCntx);

	repInfo->numRanges = numMappings;
	repInfo->begins = (int *) palloc(sizeof(int) * numMappings);
	repInfo->ends = (int *) palloc(sizeof(int) * numMappings);
	repInfo->sets = (VarBit **) palloc(sizeof(VarBit *) * numMappings);
	repInfo->string = NULL;
	repInfo->stringPointers = (char **) palloc(sizeof(char *) * numMappings);

	forbothi(llc, rlc, i, annotations, bitSets)
	{
		annot = (Value *) lfirst(llc);
		set = (VarBit *) lfirst(rlc);

		repInfo->begins[i] = 0;
		repInfo->stringPointers[i] = pstrdup(strVal(annot));
		repInfo->ends[i] = strlen(repInfo->stringPointers[i]);
		repInfo->sets[i] = DatumGetVarBitP(datumCopy(VarBitPGetDatum(set), false, -1));
	}

	MemoryContextSwitchTo(oldCtx);

	list_free_deep(bitSets);	//CHECK is safe
}

/*
 *
 */

static void
generateMappingBitsets (Query *query, List **bitsets, List **annotations)
{
	TransProvInfo *info;
	TransSubInfo *root;
	int i;

	info = GET_TRANS_INFO(query);
	root = getRootSubForNode(info);

	*annotations = gatherAnnotations(query, (Node *) info, info);

	for(i = 0; i < list_length(*annotations); i++)
		*bitsets = lappend(*bitsets,
				(void *) generateEmptyBitset(VARBITD_LENGTH(root->setForNode)));

	unionBitsets(root, *bitsets, *annotations);
}


/*
 *
 */

static void
unionBitsets (TransSubInfo *info, List *bitsets, List *annotations)
{
	ListCell *lc;
	Node *child;
	VarBit *bitSet;
	Value *annot;
	int listPos;

	/* check for each annotation if it is present at this subInfo.
	 * If so, add the bitset of the subInfo to the bitset of the annotation.
	 */
	foreach(lc, info->annot)
	{
		annot = (Value *) lfirst(lc);
		listPos = nodePositionInList(annotations, (Node *) annot);

		if (listPos != -1)
		{
			bitSet = (VarBit *) list_nth(bitsets, listPos);
			bitSet = DatumGetVarBitP(varBitOr(VarBitPGetDatum(bitSet),
					info->setForNode));
		}
	}

	/* process children */
	foreach(lc, info->children)
	{
		child = (Node *) lfirst(lc);

		while(IsA(child, TransProvInfo))
			child = ((TransProvInfo *) child)->root;

		unionBitsets((TransSubInfo *) child, bitsets, annotations);
	}
}

/*
 *
 */

static List *
gatherAnnotations (Query *query, Node *info, TransProvInfo *curInfo)
{
	List *annots;
	TransProvInfo *tInfo;
	TransSubInfo *sub;
	Query *newQuery;
	ListCell *lc;
	Node *child;

	if(IsA(info, TransProvInfo))
	{
		tInfo = (TransProvInfo *) info;

		if ((Node *) tInfo == curInfo->root)
			newQuery = (rt_fetch(tInfo->rtIndex, query->rtable))->subquery;
		else
			newQuery = query;

		annots = list_copy(Provinfo(query)->annotations);
		annots = list_concat_unique(annots, gatherAnnotations(newQuery, tInfo->root, tInfo));
	}
	else
	{
		sub = (TransSubInfo *) info;
		annots = NIL;

		if (sub->opType == SUBOP_BaseRel)
		{
			RangeTblEntry *rte;

			rte = rt_fetch(sub->rtIndex, query->rtable);

			annots = copyObject(rte->annotations);
		}

		foreach(lc, sub->children)
		{
			child = (Node *) lfirst(lc);
			annots = list_concat_unique(annots, gatherAnnotations(query, child, curInfo));
		}
		//TODO change if RTE annotations are allowed
		sub->annot = copyObject(annots);
	}

	return annots;
}
