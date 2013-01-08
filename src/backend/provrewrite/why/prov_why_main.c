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
#include "provrewrite/prov_how_set.h"
#include "provrewrite/prov_how_spj.h"

#include "fmgr.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"

#include "utils/fmgroids.h"

#include <stdio.h>
/* function declarations */
//static void addHWhy ( Query *query);


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
	te->expr = makeFuncExpr(F_HWHY, OIDARRAYOID, list_make1(te->expr), COERCE_EXPLICIT_CALL);

	return query;
}



Datum
hwhy (PG_FUNCTION_ARGS)   //user defined function?  process polynomial using stacks
{

	//is the input  how_prov_out?


	//3. process the howprov polish notation stack??
	//   for (i=0; i<HOWNUMELEMS(PG_FUNCTION_ARGS); i++)
	//   {
	//      if (size == 1)
	//          return originalPolynomial;  //  if the Oid is single we do no operations;
	//
	//      stack myStack;
	//
	//      ele = how_polynomial.pop;
	//
	//      if (ele is not "|")
	//      {
	//          myStack.push(ele);
	//      }
	//      else
	//      {
	//          ele1 = myStack.pop;
	//          ele2 = myStack.pop;
	//          operand = myStack.pop;
	//          newele = whypolynomial(ele1,ele2,operand)
	//          myStack.push(newele);
	//       }
	//    }
	//
	//

	//  why_polynomial = myStack.pop;

	//4. use how prov  function to translate polynomial into oids?
	//target entry expression = oid_translation(why_polynomial);

	PG_RETURN_NULL();
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
