/*-------------------------------------------------------------------------
 *
 * prov_copy_inclattr.c
 *	  PERM C 
 *
 * Portions Copyright (c) 2010 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $Perm: pgsql//SF_Perm/src/backend/provrewrite/copysem/prov_copy_inclattr.c,v 1.542 Aug 27, 2010 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "parser/parse_expr.h"

#include "provrewrite/prov_util.h"
#include "provrewrite/provlog.h"
#include "provrewrite/provattrname.h"
#include "provrewrite/prov_nodes.h"
#include "provrewrite/provstack.h"
#include "provrewrite/prov_trans_bitset.h"
#include "provrewrite/prov_trans_util.h"

#include "provrewrite/prov_copy_inclattr.h"


/* prototypes */
static Node *makeCaseForProvAttr(Node *condition, Expr *result);
static Node *createPartialFinalInclConds (CopyMapRelEntry *rel,
		int origAttrNum);
static Node *createCompleteFinalInclConds (CopyMapRelEntry *rel,
		int origAttrNum);
static Node *generateVarBitConstruction (Query *query, CopyMapRelEntry *rel,
		int numAttrs);
static Node *generateBitInclusionCond (Query *query, CopyMapRelEntry *rel,
		CopyMapEntry *attr, InclusionCond *cond, int bitLength, int bitNum);
static Node *getInputBitset (Query *query, CopyMapRelEntry *rel,
		CopyMapEntry *attr, int outAttr);

/*
 *
 */

void
addTopCopyInclExpr (Query *query)
{
	CopyMapRelEntry *relEntry;
	CopyMap *map = GET_COPY_MAP(query);
	TargetEntry *provAttr;
	ListCell *lc, *innerLc;
	int numProvAttr = list_length(((List *) linitial(pStack)));
	int origAttrNum = list_length(query->targetList) - numProvAttr
			- list_length(map->entries);
	int tlPos;
//	int baseAttrPos;
	Node *condition;

	// remove the copymap attributes
	list_truncate(query->targetList, origAttrNum + numProvAttr);

	tlPos = list_length(query->targetList) - numProvAttr;

	/* for each copy map rel entry add the conditional inclusion checks to the
	 * provenance attributes from the base relation represented by this entry */
	foreach(lc, map->entries)
	{
		relEntry = (CopyMapRelEntry *) lfirst(lc);

		/* provenance is always NULL. Replace provenance attrs with NULL
		 * constants */
		if (relEntry->noRewrite)
		{
			Var *var;

			foreach(innerLc, relEntry->attrEntries)
			{
				provAttr = (TargetEntry *) list_nth(query->targetList,
						tlPos++);
				var = (Var *) provAttr->expr;
				provAttr->expr = (Expr *) makeNullConst(var->vartype,
						var->vartypmod);
			}
		}
		// is static: provenance attribute values are taken from the input
		else if (relEntry->isStatic)
			tlPos += list_length(relEntry->attrEntries);
		/* neither static nor always NULL. Need to add checks for inclusion
		 * of provenance */
		else
		{
			if (IS_PARTIALC(ContributionType(query)))
				condition = createPartialFinalInclConds(relEntry, origAttrNum);
			else
				condition = createCompleteFinalInclConds(relEntry,
						origAttrNum);

			foreach(innerLc, relEntry->attrEntries)
			{
				provAttr = (TargetEntry *) list_nth(query->targetList,
						tlPos++);

				provAttr->expr = (Expr *) makeCaseForProvAttr(condition,
						provAttr->expr);
			}
		}
	}

	//TODO remove CopyMaps
}

/*
 *
 */

static Node *
makeCaseForProvAttr(Node *condition, Expr *result)
{
	CaseExpr *caseExpr;
	CaseWhen *ifPart;

	ifPart = (CaseWhen *) makeNode(CaseWhen);
	ifPart->result = result;
	ifPart->expr = (Expr *) condition;

	caseExpr = (CaseExpr *) makeNode(CaseExpr);
	caseExpr->defresult = (Expr *) makeNullConst(exprType((Node *) result),
			exprTypmod((Node *) result));
	caseExpr->args = list_make1(ifPart);
	caseExpr->casetype = exprType((Node *) result);

	return (Node *) caseExpr;
}

/*
 *
 */

static Node *
createPartialFinalInclConds (CopyMapRelEntry *rel, int origAttrNum)
{
	int bitLength = list_length(rel->attrEntries) * origAttrNum;
	Node *copymapValue;

	copymapValue = rel->provAttrInfo->bitSetComposition;
	return (Node *) MAKE_SETNEQ_FUNC(copymapValue,
			MAKE_EMPTY_BITSET_CONST(bitLength));
}

/*
 *
 */

static Node *
createCompleteFinalInclConds (CopyMapRelEntry *rel, int origAttrNum)
{
	Node *copymapValue;

	copymapValue = rel->provAttrInfo->bitSetComposition;
	return (Node *) MAKE_SETREPEAT_FUNC(copymapValue,
			makeConst(INT4OID, -1, 4, Int32GetDatum(origAttrNum), false, true));
}

/*
 *
 */

void
generateCopyMapAttributs (Query *query, int numQAttrs)
{
	CopyMap *map = GET_COPY_MAP(query);
	CopyMapRelEntry *rel;
	CopyProvAttrInfo *attrInfo;
	ListCell *lc;
	TargetEntry *te;
	int i;
	int copyAttrPos;

	copyAttrPos = list_length(query->targetList) + 1;

	// for each copy rel map...
	foreachi(lc,i,map->entries)
	{
		rel = (CopyMapRelEntry *) lfirst(lc);

		// generate copy provenance attribute info
		attrInfo = makeNode(CopyProvAttrInfo);
		attrInfo->bitSetComposition = generateVarBitConstruction(query, rel,
				numQAttrs);
		attrInfo->provVar = makeVar(map->rtindex, copyAttrPos + i, VARBITOID,
				-1, 0);
		attrInfo->outAttrNum = numQAttrs;

		rel->provAttrInfo = attrInfo;

		// append copy map attribute for current rel map to target list
		te = makeTargetEntry((Expr *) copyObject(attrInfo->bitSetComposition),
				copyAttrPos + i, appendIdToString("copymap_", &i) ,false);
		query->targetList = lappend(query->targetList, te);
	}
}

/*
 *
 */

static Node *
generateVarBitConstruction (Query *query, CopyMapRelEntry *rel, int numAttrs)
{
	CopyMapEntry *entry;
	AttrInclusions *attr;
	InclusionCond *cond;
	ListCell *aLc, *outLc, *condLc, *inCondLc;
	int inAtt, outAtt;
	List *exprs = NIL;
	Node *result = NULL;
	Node *condition = NULL;
	int bitLength = numAttrs * list_length(rel->attrEntries);
	int bitSingleton;

	// for each inclusion condition create a conditional bitset computation
	foreachi(aLc, inAtt, rel->attrEntries)
	{
		entry = (CopyMapEntry *) lfirst(aLc);

		// if attr entry is static true, generate fixed bitset
		if (entry->isStaticTrue)
		{
			Datum bitset = generateVarbitSetElem(bitLength, 0);
			bool fixedSet = true;

			// check if all attribute inclusions are static
			foreach(outLc, entry->outAttrIncls)
			{
				attr = (AttrInclusions *) lfirst(outLc);
				fixedSet &= attr->isStatic;
			}

			// if so generate a fixed bitset
			if (fixedSet)
			{
				foreach(outLc, entry->outAttrIncls)
				{
					attr = (AttrInclusions *) lfirst(outLc);
					outAtt = attr->attr->varattno;
					bitSingleton = (inAtt * numAttrs) + outAtt + 1;
					bitset = varBitOr(generateVarbitSetElem
							(bitLength, bitSingleton), bitset);
				}
				condition = (Node *) MAKE_VARBIT_CONST(bitset);
				exprs = lappend(exprs, condition);
				continue;
			}
		}
		// is static false generate 0 bitset
		if (entry->isStaticFalse)
		{
			condition = (Node *) MAKE_VARBIT_CONST(generateVarbitSetElem(bitLength, 0));
			exprs = lappend(exprs, condition);
			continue;
		}

		foreach(outLc, entry->outAttrIncls)
		{
			attr = (AttrInclusions *) lfirst(outLc);
			outAtt = attr->attr->varattno;
			bitSingleton = inAtt * numAttrs + outAtt + 1;

			// out attribute is included statically
			if (attr->isStatic)
			{
				condition = (Node *) MAKE_VARBIT_CONST(generateVarbitSetElem(
						numAttrs, bitSingleton));
				exprs = lappend(exprs, condition);
			}
			// non-static create conditional for each InclusionCond
			else
			{
				foreach(condLc, attr->inclConds)
				{
					AttrInclusions *inAttrIncl;
					InclusionCond *innerCond;

					cond = (InclusionCond *) lfirst(condLc);
					Assert(IsA(cond->existsAttr, AttrInclusions)
							&& cond->inclType == INCL_EXISTS);

					inAttrIncl = (AttrInclusions *) cond->existsAttr;

					foreach(inCondLc, inAttrIncl->inclConds)
					{
						innerCond = (InclusionCond *) lfirst(inCondLc);
						condition = generateBitInclusionCond (query, rel, entry,
							innerCond, bitLength, bitSingleton);
					}

					exprs = lappend(exprs, condition);
				}
			}
		}
	}

	// bit_or the individual computations
	result = (Node *) linitial(exprs);

	for(aLc = exprs->head->next; aLc != NULL; aLc = aLc->next)
		result = (Node *) MAKE_SETOR_FUNC(list_make2(result, lfirst(aLc)));

	return result;
}

/*
 *
 */

static Node *
generateBitInclusionCond (Query *query, CopyMapRelEntry *rel,
		CopyMapEntry *attr, InclusionCond *cond, int bitLength, int bitNum)
{
	Node *condition = NULL;
	Datum bitSet;
	Node *inputBitset;
	Var *inProvAttr = NULL;
	Node *cons;
	CaseExpr *caseExpr;
	CaseWhen *ifPart;
	ListCell *lc;
	CopyMapRelEntry *relChild;
	int inputAttr;

	bitSet = generateVarbitSetElem(bitLength, bitNum);
	cons = (Node *) MAKE_VARBIT_CONST(bitSet);
	inputAttr = ((Var *) cond->existsAttr)->varattno;

	/* get child copy-provenance attribute and bitset-singleton for the exists
	 * part of the inclusion condition */
	relChild = rel->child;

	// create condition based on inclusion type
	if (!relChild->isStatic)
	{
		inProvAttr = relChild->provAttrInfo->provVar;
		inputBitset = getInputBitset (query, rel, attr, inputAttr);
		condition = (Node *) MAKE_SETCONT_FUNC(list_make2(inProvAttr,
				inputBitset));
	}
	else if (cond->inclType == INCL_EXISTS)
		return cons;

	switch(cond->inclType)
	{
	case INCL_EQUAL:
	{
		Var *inclVar = (Var *) cond->existsAttr;
		Var *eqVar;

		foreach(lc, cond->eqVars)
		{
			eqVar = (Var *) lfirst(lc);

			if (condition)
			{
				condition = (Node *) makeBoolExpr(AND_EXPR,
						list_make2(condition,
						createEqualityConditionForVars(inclVar, eqVar)));
			}
			else
			{
				condition = (Node *)
						createEqualityConditionForVars(inclVar, eqVar);
			}
		}
	}
		break;
	case INCL_IF:
		if (condition)
			condition = (Node *) makeBoolExpr(AND_EXPR,
					list_make2(condition, cond->cond));
		else
			condition = cond->cond;
		break;
	default:
		break;
	}

	ifPart = (CaseWhen *) makeNode(CaseWhen);
	ifPart->result = (Expr *) cons;
	ifPart->expr = (Expr *) condition;

	caseExpr = (CaseExpr *) makeNode(CaseExpr);
	caseExpr->casetype = VARBITOID;
	caseExpr->defresult = (Expr *) MAKE_EMPTY_BITSET_CONST(bitLength);
	caseExpr->args = list_make1(ifPart);

	return (Node *) caseExpr;
}

/*
 *
 */

static Node *
getInputBitset (Query *query, CopyMapRelEntry *rel, CopyMapEntry *attr, int outAttr)
{
	int numQAttrs;
	int baseAttrPos;
	int bitLength;
	int bitSingleton;
	CopyMapEntry *entry;
	ListCell *lc;

	numQAttrs = rel->child->provAttrInfo->outAttrNum;
	bitLength = numQAttrs * list_length(rel->attrEntries);

	// get Attrentry for the base relation attribute
	foreachi(lc, baseAttrPos, rel->child->attrEntries)
	{
		entry = (CopyMapEntry *) lfirst(lc);
		if (equal(entry->baseRelAttr, attr->baseRelAttr))
			break;
	}

	bitSingleton = (numQAttrs * baseAttrPos) + outAttr + 1;

	return (Node *) MAKE_VARBIT_CONST(generateVarbitSetElem(bitLength, bitSingleton));
}
