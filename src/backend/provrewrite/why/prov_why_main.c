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
//#include "provrewrite/prov_how_set.h"
//#include "provrewrite/prov_how_spj.h"

#include "fmgr.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"

#include "utils/fmgroids.h"

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
	te->expr = (Expr *) makeFuncExpr(F_HWHY, OIDARRAYOID, list_make1(te->expr), COERCE_EXPLICIT_CALL);

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
				lcons(opRes, stack);
				lcons_int(1, typeStack);

				pfree(oper);
				// free ptr
			}
			break;
			}

			data++;
		}

//		if (ele is not "|")
//		{
//			myStack.push(ele);
//		}
//
//		else
//		{
//			ele1 = myStack.pop;
//			ele2 = myStack.pop;
//			operand = myStack.pop;
//			newele = whypolynomial(ele1,ele2,operand)
//	        		  myStack.push(newele);
//		}
	}
	//
	//
	listResult = linitial(stack);
	//  why_polynomial = myStack.pop;

	//4. use how prov  function to translate polynomial into oids?
	{
		int resSize; // compute number of elements

		Datum *oidarray = palloc(resSize * sizeof(Oid));
		int *dims;
		Datum result = (Datum) NULL; //= construct_md_array(oidarray, NULL, 2, dims, NULL, );

		PG_RETURN_DATUM(result);
	}
}

List *
computeWhyOper(List *args, char op)
{
	List *result;
	int size = list_length(args);
	switch(op)
	{

	}

	return result;
}

//OidList *
//whypolynomial(ele1,ele2,operand)
//{
//	if (operand == "+")
//  {
//     // h_(Why) (x+y):   {{a}, {c, d}} + {{b}, {e}} = {{a}, {c,d}, {b}, {e}}
//      OidList ele;
//      for (i=0; i<sizeof(ele1); i++)
//      {
//              ele.append(ele1[i]);
//      }
//      for (j=0; j<sizeof(ele2); j++)
//      {
//              ele.append(ele2[j]);
//      }
//
//
//  }
//  elseif (operand == "*")
//  {
//      //h_(Why) (x*y): {{a}, {c, d}} * {{b}, {e}} = {{a, b}, {a, e}, {c, d, b}, {c, d, e}}
//      OidList ele;
//      for (i=0; i<sizeof(ele1); i++)
//      {
//          for (j=0; j<sizeof(ele2); j++)
//          {
//				OidList newList;
//              newList = oidmerge(ele1[i],ele2[j]);
//              ele.append(newList);
//          }
//      }
//  }
//
//   return ele;
//}
