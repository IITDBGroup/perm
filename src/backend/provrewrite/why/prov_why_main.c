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
	//
	listResult = linitial(stack);

	logNode(listResult, "result of merging");

	//4. use how prov  function to translate polynomial into oids?
	{
		int resSize; // compute number of elements
		resSize = 1;
		Datum *oidarray = palloc(resSize * sizeof(Oid));
		int *dims;
		Datum result = (Datum) NULL; //= construct_md_array(oidarray, NULL, 2, dims, NULL, );

		//PG_RETURN_POINTER(listResult);
		//PG_RETURN_NULL();

		PG_RETURN_DATUM((char *) pretty_format_node_dump(nodeToString(listResult)));


//		Datum data[1];
//		int dim[1];
//
//		dim[0] = 1;
//		data[0] = PG_GETARG_DATUM(1);
//
//		result = construct_md_array(data, NULL, 1, dim, dim, TEXTOID, -1,
//				false, 'i');
//		PG_RETURN_ARRAYTYPE_P(result);
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
