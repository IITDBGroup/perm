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
	query = rewriteQueryHow(query);
	query = rewriteWhyHowProv(query);
	return query;

}

Query *
rewriteWhyHowProv (Query *query)
{
	//1. retrieve the last target entry of the query tree: e.g.   the howprov
	//
	TargetEntry *te = (TargetEntry *) llast(query->targetList);


	//2. save the howprov target entry content into polish notation stack??
	// or retreive the howprove polynomial output
	//te->expr = (Expr *) makeFuncExpr(F_HWHY, OIDARRAYOID, list_make1(te->expr), COERCE_EXPLICIT_CALL);  //working


	te = makeTargetEntry((Expr *) makeFuncExpr(F_HWHY, OIDARRAYOID, list_make1(te->expr), COERCE_EXPLICIT_CALL),
			list_length(query->targetList)+1, "whyprov", false);


	query->targetList = lappend(query->targetList, te); //working
	//query->targetList = lappend(list_delete_cell(query->targetList, list_tail(query->targetList), NULL), te);  //need to test



	return query;
}


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



Datum
hwhy (PG_FUNCTION_ARGS)   //user defined function?  process polynomial using stacks
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
	Datum result = (Datum) NULL;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	howIn = PG_GETARG_HOWPROV_P(0);
	data = HOWDATA(howIn);
	headPtr = HOWHEADER(howIn);

	//3. process the howprov polish notation stack??
	for (i=0; i<HOWNUMELEMS(howIn); i++)
	{
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
	//the listResult is the set of set output, e.g. final output of why provenance
	listResult = linitial(stack);
	logNode(listResult, "result of merging");

	//logNode(pretty_format_node_dump(nodeToString(listResult)), "testeststst");

	//4. use how prov  function to translate polynomial into oids?
	{
		List *cpyListResult = list_copy(listResult);
		int setofsetSize; // compute number of elements
		int count = 0;
		List *nthOidSet;
		int setSize = 0;
		int coldim = 0;
		int totalElem = 0; // total number of Oids in the set of sets.
		int j = 0;


		setofsetSize = list_length(cpyListResult);

		//int *dims;



		//compute max col dimension
		for (count=0; count<setofsetSize; count++)
		{
			setSize = list_length(linitial(cpyListResult));
			totalElem += setSize;
			listResult = list_delete_first(cpyListResult);
			if (setSize > coldim)  coldim = setSize;
		}

		//now we have row count = setofsetSize, col amount = coldim, and a set of set in ListResult
		//

		//		Oid myResult[setofsetSize][coldim];
		//		construct_md_array needs a Datum * as input that is a dynamically allocated array.
		Datum *myResult = palloc(totalElem * sizeof(Datum));
		Oid nthOID;

		ListCell *SetofOids;
		bool *nullMap = palloc(setofsetSize * coldim *sizeof(bool));

		int myResultOIDIndex = 0;
		int nullIndex = 0;

		// here I would loop through the lists using foreach and manually update a position pointer. Plus set the nullMap (see below)
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
		//
		//the definition of construct_md_array function is as follow, the

		//		construct_md_array(Datum *elems,
		//						   bool *nulls,
		//						   int ndims,
		//						   int *dims,
		//						   int *lbs,
		//						   Oid elmtype, int elmlen, bool elmbyval, char elmalign)
		//
		//
		/* make sure data is not toasted */
		//i dont know what to put in lbs, elmtype, elmlen, elmbyval, and elmalign
		//

		// serveral errors here:
		// 1) the second parameter is a array of booleans indicating for each element whether it is null or not. Allocate an bool array: bool *nullMap = palloc(setofsetSize * coldim *sizeof(bool))
		// then you can access this thing like an array nullMap[0] = true; nullMap[1] = false;
		// 2) similar the dims parameter is an array storing the size of each dimension. Allocate: int *dims = palloc(2 * sizeof(int));
		// 3) lbs is also an array of int. It should contain only 1's.

		int arraysize = totalElem * coldim;

		int *lbs = palloc(2 * sizeof(int));
		int *dims = palloc(2 * sizeof(int));
		lbs[0] = 1;
		lbs[1] = 1;
		dims[0] = setofsetSize;
		dims[1] = coldim;

		Datum result = (Datum) construct_md_array(myResult, nullMap, setofsetSize, dims, lbs, 1028, -1, false, 'i' );
		PG_RETURN_ARRAYTYPE_P(result);

	}

}

List *
computeWhyOper(List *args, char op)
{
	List *result = NIL;
	List *oidSet;
	List *oidList;

	int SetSize = list_length(args);
	int SetIndex;
	int OidSetIndex;
	int OidSetSize;
	int OidListSize;
	int OidIndex;

	List *operEle;


	switch(op)
	{
	case '+':
	{

		for (SetIndex=0; SetIndex<SetSize; SetIndex++)
		{
			result = list_concat_unique(result,(List *) list_nth(args,SetIndex));
		}
	}
	break;
	case '*':
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
			//list_concat_unique_oid(List *list1, List *list2)

			result = newResult;
		}
	}
	break;
	}
	//logNode(result, "result of merging");
	return result;
}
