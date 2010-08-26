/*-------------------------------------------------------------------------
 *
 * prov_util.h
 *		External interface to provenance correlated sublink rewriting
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql/src/include/provrewrite/prov_util.h,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_UTIL_H_
#define PROV_UTIL_H_

#include "nodes/parsenodes.h"
#include "nodes/primnodes.h"

#define REPLACE_SUB_EXPR_SUBLINK		0x01
#define REPLACE_SUB_EXPR_QUERY			0x02
#define REPLACE_CHECK_REPLACERS			0x04
#define REPLACE_SUB_EXPR_ALL			0x03

/* macros */
#define MAKE_RTREF(rtref, refindex) \
	do { \
		(rtref) = makeNode(RangeTblRef); \
		(rtref)->rtindex = refindex; \
	} while (0)

#define AND_EXPR_WITH_NODE(qual, node) \
	do { \
		(qual) = ((qual) == NULL) ? ((qual) = ((Node *) node)) :  ((qual) = (Node *) makeBoolExpr(AND_EXPR, list_make2((qual), node))); \
	} while (0)

/* loop throug list and increase integer */
#define foreachi(cell,i,list) \
	for((cell) = list_head(list), (i) = 0; (cell) != NULL; (cell) = (cell)->next, i++)

#define forbothi(lcell, rcell, i, llist, rlist) \
	for((lcell) = list_head(llist), (rcell) = list_head(rlist), (i) = 0; (lcell) != NULL && (rcell) != NULL; (lcell) = (lcell)->next, (rcell) = (rcell)->next, i++)

#define foreachsince(cell,start) \
	for((cell) = (start); (cell) != NULL; (cell) = (cell)->next)

#define forbothsince(lcell,lstart,rcell,rstart) \
	for((lcell) = (lstart), (rcell) = (rstart); (lcell) != NULL; (lcell) = (lcell)->next, (rcell) = (rcell)->next)

#define foreachisince(cell,i,start,startI) \
	for((cell) = (start), (i) = (startI); (cell) != NULL; (cell) = (cell)->next, i++)

#define foreachwithbefore(cell,before,list) \
	for((cell) = list_head(list), (before) = NULL; (cell) != NULL; (before) = (cell), (cell) = (cell)->next)

#define IGNORE_RTE_INDEX(query,index) \
	(setIgnoreRTE(rt_fetch((Index) index, ((Query *) query)->rtable)))

/* enums */
typedef enum ComparisonType
{
	COMP_SMALLER,
	COMP_SMALLEREQ,
	COMP_BIGGER,
	COMP_BIGGEREQ,
	COMP_EQUAL,
	COMP_NOCOMP
} ComparisonType;

typedef enum JoinChild
{
	JCHILD_LEFT,
	JCHILD_RIGHT,
	JCHILD_RTREF
} JoinChild;

/*
 *
 */
typedef struct ReplaceSubExpressionsContext
{
	int flags;			// defines which node types the mutator recurses into
	List *searchList;
	List *replaceList;
} ReplaceSubExpressionsContext;

/*
 *
 */
typedef struct JoinQualWalkerContext
{
	Node *expr;
	Node **curParent;
	Node **result;
} JoinQualWalkerContext;

/*
 *
 */
typedef struct GetFromItemWalkerContext
{
	Index rtIndex;
	Node **curParent;
	Node **result;
} GetFromItemWalkerContext;

/*
 *
 */
typedef struct FindSubExpressionWalkerContext
{
	Node *result;
	Node *searchExpr;
	int flags;
} FindSubExpressionWalkerContext;


/*prototypes */

/* provenance support functions */
extern bool ignoreRTE(RangeTblEntry *rte);
extern void setIgnoreRTE (RangeTblEntry *rte);
extern void removeProvInfoNodes (Query *query);
extern bool queryHasRewriteChildren (Query *query);
extern bool hasProvenanceSubquery (Query *query);
extern List *addProvenanceAttrsForRange (Query *query, int min, int max,
		List *pList);
extern List *addProvenanceAttrs (Query *query, List *subList, List *pList);
extern List *findBaseRelationsForProvenanceQuery (Query *query);
extern void findBaseRelationsForProvenanceRTE (RangeTblEntry *rte,
		List **result);
extern void getRTindexForProvTE (Query *query, Var* var);

/* node creation support functions */
extern Query *generateQueryFromBaseRelation (RangeTblEntry *rte);
extern RangeTblEntry *generateQueryRTEFromRelRTE (RangeTblEntry *rte);
extern void addSubqueryToRTWithParam (Query *query, Query *subQuery,
		char *aliasName, bool inFrom, AclMode reqPerms, bool append);
extern void addSubqueryToRT (Query *query, Query* subQuery, char *aliasName);
extern void correctSubQueryAlias (Query *query);
extern void correctRecurSubQueryAlias (Query *query);
extern void correctRTEAlias (RangeTblEntry *rte);
extern void addConditionToQualWithAnd (Query *query, Node *condition,
		bool and);
extern void adaptRTEsForJoins(List *subJoins, Query *query, char *joinRTEname);
extern void recreateJoinRTEs (Query *query);
extern JoinExpr *createJoinExpr (Query *query, JoinType joinType);
extern JoinExpr *createJoinOnAttrs (Query *query, JoinType joinType,
		Index leftRT, Index rightRT, List *leftAttrs, List *rightAttrs,
		bool useNotDistinct);
extern Node *replaceSubExpression (Node *node, List *searchList,
		List *replaceList, int flags);

/* expression creation support functions */
extern Node *createEqualityConditionForVars (Var *leftChild, Var *rightChild);
extern Node *createEqualityConditionForNodes (Node *left, Node *right);
extern Node *createSmallerCondition (Node *left, Node *right);
extern Node *createSmallerEqCondition (Node *left, Node *right);
extern Node *createBiggerCondition (Node *left, Node *right);
extern Node *createNotDistinctConditionForVars (Var *leftChild,
		Var *rightChild);
extern Node *createAndFromList (List *exprs);

/* node navigation and search support functions */
extern bool findRTindexInFrom (Index rtindex, Query *query, List **rtList,
		List **joinPath);
extern bool findRTindexInJoin (Index rtindex, JoinExpr *joinExpr,
		List** rtList, List **joinPath);
extern Node *getJoinTreeNode (Query *query, Index rtindex);
extern List *getAggrExprs (Node *node);
extern Node **getJoinForJoinQual (Query *query, Node *node);
extern Node **getLinkToFromItem (Query *query, Index rtIndex);
extern Node *findSubExpression (Node *expr, Node *subExpr, int flags);
extern List *getGroupByTLEs (Query *query);
extern List *getGroupByExpr (Query *query);
extern bool isUsedInGroupBy(Query *query, TargetEntry *te);
extern bool isUsedInOrderBy (Query *query, TargetEntry *te);
extern bool isVarOrConstWithCast (Node *node);
extern Var *getVarFromTeIfSimple (Node *node);
extern bool isInequality (Node *node);
extern ComparisonType getComparisonType (Node *node);
extern bool isConstExpr (Node *node);
extern TargetEntry *findTeForVar (Var *var, List *targetList);
extern bool hasOuterJoins (Query *query);
extern char *getAlias (RangeTblEntry *rte);
extern Var *resolveToRteVar (Var *var, Query *query);

#endif /*PROV_UTIL_H_*/
