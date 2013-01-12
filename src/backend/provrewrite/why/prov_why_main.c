/*
 * whymain.c
 *
 *  Created on: Dec 12, 2012
 *      Author: zhenwang919
 */

#include "postgres.h"

#include "nodes/parsenodes.h"
#include "nodes/makefuncs.h"

#include "catalog/pg_type.h"

#include "provrewrite/prov_util.h"
#include "provrewrite/provlog.h"
#include "provrewrite/prov_why_main.h"
#include "provrewrite/prov_how_main.h"
#include "provrewrite/prov_how_adt.h"


#include "fmgr.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"

#include "utils/fmgroids.h"

#include "nodes/pg_list.h"


#include "utils/array.h"
#include <stdio.h>
/* function declarations */
//static void addHWhy ( Query *query);
static List *computeWhyOper(List *args, char op);

Query *
rewriteQueryWhy (Query *query)
{
	//1. call how provenance to add how provenance target entry to standard query tree
	query = rewriteQueryHow(query);

	//2. call function to add why provenance target entry to the how-provenance query tree
	query = rewriteWhyHowProv(query);

	//3. return the why-provenance enabled query tree
	return query;

}

Query *
rewriteWhyHowProv (Query *query)
{
	//1. retrieve the last target entry of the query tree: e.g. the how-provenance
	TargetEntry *te = (TargetEntry *) llast(query->targetList);

	//2. call UDF to rewrite how-provenance polynomial into set of OID sets and create a new target entry using the output of this UDF
	te = makeTargetEntry((Expr *) makeFuncExpr(F_HWHY, OIDARRAYOID, list_make1(te->expr), COERCE_EXPLICIT_CALL),
			list_length(query->targetList)+1, "whyprov", false);

	//3. add the target entry back to the end of query tree, e.g. add why provenance in query tree output
	query->targetList = lappend(query->targetList, te);

	//4. return the modified query tree
	return query;
}

//macro of reading result by comparing bitmap using a mask
#define GETBIT(result,ptr,mask) \
		do { \
			result = (*ptr & mask); \
			mask >>= 1; \
			if (!mask) \
			{ \
				mask = HIGHBIT; \
				ptr++; \
			} \
		} while(0)


#define POP(stack, ptr) \
	do { \
		ptr = linitial(stack); \
		stack = list_delete_first(stack); \
	} while(0)

#define POP_INT(stack, val) \
	do { \
		val = linitial_int(stack); \
		stack = list_delete_first(stack); \
	} while(0)



//UDF of rewrite how-provenance polynomial into why-provenance set of sets
//stores how-provenance polynomial in stack and its bitmap
//push the provenance stack and bitmap, when reach the "|" , pop out related elements and compute new set of oids and push the set back to stack
//the final result of the stack will contain only one element which is the set of sets, it's stored in the listResults
//the listResults is then converted into perm built-in data type multi-dimensional array of oids.
//the md array is stored in result
Datum
hwhy (PG_FUNCTION_ARGS)
{
	HowProv *howIn;
	List *stack = NIL;
	List *typeStack = NIL;
	char *data;
	bits8 *headPtr;
	bits8 bitMask = HIGHBIT;
	bool isOid;
	int i;
	List *listResult;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	howIn = PG_GETARG_HOWPROV_P(0);
	data = HOWDATA(howIn);
	headPtr = HOWHEADER(howIn);

	//1. read the howprov polish notation and push in stack
	for (i=0; i<HOWNUMELEMS(howIn); i++)
	{
		//read bitmap to determine what's the type of how-provenance polynomial element
		GETBIT(isOid,headPtr,bitMask);

		if (isOid)
		{
			Oid oid;
			List *oidSet;
			memcpy(&oid, data, sizeof(Oid));

			oidSet = list_make1(list_make1_oid(oid));
			stack = lcons(oidSet, stack);
			typeStack = lcons_int(1, typeStack);

			data += sizeof(Oid);
		}
		else
		{
			char *op = palloc(sizeof(char));
			*op = *(data);

			switch (*op)
			{
			case '+':
			{
				stack = lcons(op, stack);
				typeStack = lcons_int(0, typeStack);
			}
			break;

			case '*':
			{
				stack = lcons(op, stack);
				typeStack = lcons_int(0, typeStack);
			}
			break;

			//2. when meet the "|", do not push, pop out elements for computation and push back the result into stack
			case '|':
			{
				List *args = NIL;
				char *oper;
				List *arg;
				List *opRes;
				int type;

				POP_INT(typeStack, type);
				while (type != 0) {
					POP(stack, arg);
					args = lappend(args, arg);
					POP_INT(typeStack, type);
				}
				POP(stack, oper);

				// compute result and push it back
				opRes = computeWhyOper(args, *oper);
				stack = lcons(opRes, stack);
				typeStack = lcons_int(1, typeStack);


				// free pointer
				pfree(oper);

			}
			break;
			}

			data++;
		}

	}
	//
	//3. the listResult is the set of sets output, e.g. final output of why provenance
	listResult = (List *) linitial(stack);

	//4. convert the list into multi-dimension array of oids
	{
		int setofsetSize; // compute number of elements
		int setSize = 0;
		int coldim = 0;
		int totalElem = 0; // total number of Oids in the set of sets.

		ListCell *lc;

		setofsetSize = list_length(listResult);


		//compute max col dimension
		foreach(lc, listResult)
		{
			setSize = list_length((List *) lfirst(lc));
			totalElem += setSize;
			if (setSize > coldim) coldim = setSize;
		}

		//now we have row count = setofsetSize, col amount = coldim, and a set of set in ListResult
		//		construct_md_array needs a Datum * as input that is a dynamically allocated array.
		Datum *myResult = palloc(totalElem * sizeof(Datum));
		//Oid nthOID;

		ListCell *SetofOids;
		bool *nullMap = palloc(setofsetSize * coldim *sizeof(bool));

		int myResultOIDIndex = 0; //position pointer for array elements
		int nullIndex = 0; //position pointer for nullMap

		//loop through the lists using foreach and manually update a position pointer. Plus the nullMap
		foreach(SetofOids, listResult)
		{
			List *currentOIDSet;
			int loopdiffer = 0;
			int currentSetSize;
			int differ;
			ListCell *oidinset;

			currentOIDSet = (List *) lfirst(SetofOids);
			currentSetSize = list_length(currentOIDSet);
			differ = coldim - currentSetSize;

			foreach (oidinset, currentOIDSet)
			{
				//manually update a position pointer. Plus set the nullMap (see below)
				myResult[myResultOIDIndex++] = ObjectIdGetDatum(lfirst_oid(oidinset));
				nullMap[nullIndex++] = false;
			}

			for (loopdiffer = 0; loopdiffer < differ; loopdiffer ++)
				nullMap[nullIndex++] = true;
		}

		//after getting the OID matrix, create multi-dimensional arrays;

		int *lbs = palloc(2 * sizeof(int));
		int *dims = palloc(2 * sizeof(int));

		lbs[0] = 1;
		lbs[1] = 1;

		dims[0] = setofsetSize;
		dims[1] = coldim;

		//the set of sets is represented in a 2-dimensional array of OIDs
		Datum result = (Datum) construct_md_array(myResult, nullMap, 2, dims, lbs, OIDOID, 4, true, 'i' );

		PG_RETURN_ARRAYTYPE_P(result);

	}

}

List *
computeWhyOper(List *args, char op)
{
	List *result = NIL;


	int SetSize = list_length(args);
	int SetIndex;



	switch(op)
	{
	case '+': //the actual "+" aggregation function in why-provenance
	{

		for (SetIndex=0; SetIndex<SetSize; SetIndex++)
		{
			result = list_concat_unique(result,(List *) list_nth(args,SetIndex));
		}
	}
	break;
	case '*': //the actual "*" aggregation function in why-provenance
	{
		List *newResult = NIL;
		result = copyObject(linitial(args));

		for (SetIndex=1; SetIndex<SetSize; SetIndex++)
		{
			ListCell *li, *lj;
			List *cur = (List *) list_nth(args,SetIndex);

			foreach(li, cur)
			{
				foreach(lj, result)
				{
					List *wl, *wr, *merge;
					wl = lfirst(li);
					wr = lfirst(lj);
					merge = list_concat_unique_oid(wl, wr);
					newResult = lappend(newResult, merge);
				}
			}

			result = newResult;
		}
	}
	break;
	}

	return result;
}
