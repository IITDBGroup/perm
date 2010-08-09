/*-------------------------------------------------------------------------
 *
 * prov_nodes.h
 *		 : Nodes definitions used by the provenance rewriter.
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_nodes.h,v 1.29 15.12.2008 12:39:34 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_NODES_H_
#define PROV_NODES_H_

#include "nodes/nodes.h"
#include "nodes/parsenodes.h"
#include "nodes/primnodes.h"
#include "nodes/value.h"
#include "postgres.h"
#include "nodes/bitmapset.h"

/*
 * -------------------------------------------------------------------------
 * 			Node definitions
 * -------------------------------------------------------------------------
 */

/*
 *
 */

typedef enum ContributionType
{
	CONTR_COPY_COMPLETE_TRANSITIVE,
	CONTR_COPY_COMPLETE_NONTRANSITIVE,
	CONTR_COPY_PARTIAL_TRANSITIVE,
	CONTR_COPY_PARTIAL_NONTRANSITIVE,
	CONTR_INFLUENCE,
	CONTR_TRANS_SET,
	CONTR_TRANS_SQL,
	CONTR_TRANS_XML,
	CONTR_TRANS_XML_SIMPLE,
	CONTR_MAP
} ContributionType;


/*
 * A data structure used for copy-contribution provenance computation.
 */

typedef struct CopyMap {
	NodeTag type;
	List *entries;
	Index rtindex;
} CopyMap;

/*
 * CopyProvInfo
 */

typedef struct CopyProvInfo
{
	NodeTag type;
	CopyMap *inMap;
	CopyMap *outMap;
//	List *outAttrs;
} CopyProvInfo;



/*
 * An entry in a copy map.
 */
typedef struct CopyMapRelEntry
{
	NodeTag type;
	Oid relation;
	int refNum;
	List *eqList;
	List *attrEntries;
	Index rtindex;
	bool propagate;
} CopyMapRelEntry;

/*
 * Maps a base relation attribute to a list of attributes of a query block.
 */

typedef struct CopyMapEntry
{
	NodeTag type;
	Var *baseRelAttr;
	char *provAttrName;
	List *inVars;
	List *outVars;
} CopyMapEntry;

/*
 * An attribute reference with a possible condition attached. Used in the eqList of an copy map entry.
 */

typedef struct EquivalenceAttr {
	NodeTag type;
	Var attr;
	Node *condition;
} EquivalenceAttr;

/*
 *
 */

typedef struct ProvInfo
{
	NodeTag	type;
	bool provSublinkRewritten;
	bool shouldRewrite;
	ContributionType contribution;
	Node *copyInfo;
	Node *rewriteInfo;
	List *annotations;
} ProvInfo;

/* enum representing the position of a sublink in a query */
typedef enum SublinkLocation
{
	SUBLOC_SELECT,
	SUBLOC_WHERE,
	SUBLOC_HAVING,
	SUBLOC_GROUPBY,
	SUBLOC_ORDER
} SublinkLocation;

/*
 * enum used to distinguish between different types of sublinks. A sublinks is
 * uncorrelated (SUBCAT_UNCORRELATED) if its query does not contain column references
 * for columns from the query where the sublink is used in. It is SUBCAT_SELPUSHUP if it
 * contains such references, but the conditions where these references are used can be pulled up
 * to the top-level of the the sublink query. Otherwise a sublink is SUBCAT_NOSELPUSHUP.
 */

typedef enum SublinkCat
{
	SUBCAT_UNCORRELATED,
	SUBCAT_CORRELATED,
	SUBCAT_CORRELATED_INSIDE
} SublinkCat;


/*
 *
 */
typedef enum SublinkAggLocation
{
	SUBAGG_IN_GROUPBY,
	SUBAGG_IN_AGG,
	SUBAGG_OUTSIDE
} SublinkAggLocation;

/*
 * Struct for passing around information about a SubLink used by provenance rewrite module
 */
typedef struct SublinkInfo
{
	NodeTag		type;
	Index sublinkPos;					/* the position of the sublink in the query it is used in (needed for provenance attribute ordering) */
	SublinkLocation location;			/* the clause in which the expression containing the sublink is from */
	SublinkCat category;				/* type of sublink */
	SublinkAggLocation aggLoc;
	bool unnested;						/* if true this sublink has been unnested and is not available anymore */
	SubLink *sublink;
	Node *parent;
	Node **grandParentExprPointer;		/* the parent node of the sublink in exprRoot if available */
	Node *exprRoot;						/* root node of the expression in which the SubLink is used */
	Node *rootCopy;						/* unchanged copy of the root expr */
	Node **subLinkExprPointer;			/* the (Node *) pointer that points to the SubLink expression */
	Node *aggOrGroup;					/* the aggregation or groupby expression the Sublink is used in */
	Node **aggOrGroupPointer;			/* a poiner to the parents (of aggOrGroup) pointer to aggOrGroup */
	List *corrVarInfos;					/* information about attrbute references to a query outside the sublink query */
	List *condRTEs;						/* range table entries referenced in the sublink condition */
	List *corrRTEs;						/* range table entries for correlated attributes */
	Var *targetVar;
	Index sublinkRTindex;				/* range table index of the copied sublink query */
	Query *subLinkJoinQuery;			/* modified sublink query copy used in left join for provenance */
	Index leftJoinPos;					/* the left join node that joins the subLinkJoinQuery with the normal query */
	Query *rewrittenSublinkQuery;		/* rewritten version of the query used in the sublink */
} SublinkInfo;

/*
 * Struct used to store information about an correlated Var
 */

typedef struct CorrVarInfo
{
	NodeTag type;
	Var *corrVar;
	List *vars;
	Node *parent;
	Query *parentQuery;
	Node *exprRoot;
	bool outside;
	RangeTblEntry *refRTE;
	SublinkLocation location;
//	int nestingDepth;
	int trueVarLevelsUp;		/* number of sublinks nesting levels between the referenced var and the correlated var */
	bool belowAgg;
	bool belowSet;
} CorrVarInfo;



/*-------------------------------------------------------------------------
 * Nodes for selection pushdown
 *-------------------------------------------------------------------------*/

/*
 * Struct for restricted provenance computation. (Selection pushdown)
 */

typedef struct SelectionInfo
{
	NodeTag type;
	Node *expr;
	Node *eqExpr;
	List *vars;
	bool notMovable;
	bool derived;
	Index rtOrigin;
} SelectionInfo;


/*
 * A list of expression that are provable equivalent
 */

typedef struct EquivalenceList
{
	NodeTag type;
	List *genSels;
	List *exprs;
	List *derivedSels;
	Const *constant;
	struct SelScope *scope;
} EquivalenceList;	//TODO add convenience stuff like contained vars, etc.

/*
 * Info of a single selection condition used for selection pushdown
 */

typedef struct PushdownInfo
{
	NodeTag type;
	List *conjuncts;
	List *equiLists;
	bool contradiction;
} PushdownInfo;

typedef struct InequalityGraph
{
	NodeTag type;
	List *equiLists;
	List *nodes;
} InequalityGraph;

typedef struct InequalityGraphNode
{
	NodeTag type;
	List *equis;
	List *lessThen;
	List *lessEqThen;
	List *greaterThen;
	List *greaterEqThen;
	List *consts;		// if equis contain constants, the list of values of these constant
} InequalityGraphNode;

/*
 * Enum for types of scopes. This is needed to know which expression rewrites can be applied to a scope and
 * to determine which types of expressions can be exchanged between scopes.
 */

typedef enum SelScopeType
{
	SCOPE_AGG,
	SCOPE_NORMAL,
	SCOPE_OUTERLEFT,
	SCOPE_OUTERRIGHT,
	SCOPE_OUTERFULL,
	SCOPE_NULLABLE_OUTER,
	SCOPE_NONNULL_OUTER,
	SETOP
} SelScopeType;

/*
 * Used to store information about selection conditions and the scope they are valid in.
 */

typedef struct SelScope
{
	NodeTag type;
	Bitmapset *baseRTEs;		// base relation range table indices over which selection conditions of the scope can belong too
	Bitmapset *joinRTEs;		// join nodes range table indices that belong to the scope
	List *equiLists;			// equivalence lists that are valid in this scope
	List *selInfos;				// selection condition that are valid in this scope
	struct SelScope *parent;	// direct parent scope
	Bitmapset *childRTEs;		// range table indices of join nodes and base relations that belong to the scope
	List *children;				// direct child scopes
	bool contradiction;			// if true we were able to derive a contradiction from the selection condition of this scope
	SelScopeType scopeType;		// type of scope. See SelScopeType
	struct QueryPushdownInfo *pushdown;	// QueryPushdownInfo for the query this scope belongs to
	Index topIndex;				// range table index of the top join nodes that belongs to this scope (RTINDEX_TOPQUAL, if the where-qual belongs to the scope
	int *baseRelMap;			// maps each base relation range table index to the rt index of the lowest join tree node that accesses each base rel and belongs to the scope
} SelScope; //TODO add for each base rel a point where we have to add the qual

/*
 * Stores information about the selection conditions used in a query node.
 */

typedef struct QueryPushdownInfo
{
	NodeTag type;
	Query *query;							// the query node we store information about
	List *scopes;							// list of SelScopes that are defined for the query
	SelScope *topScope;						// top level scope
	List *qualPointers;						// list of pointers for the quals of each range table entry. E.g. quals of a JoinExpr
	struct QueryPushdownInfo *parent;		// link to the QueryPushdownInfo of the parent query node of "query"
	List *children;							// link to the QueryPushdownInfos of the subqueries of "query"
	List *validityScopes;					// a list of Bitmapsets. For each rte we have the set of base relations that are valid at this rte
} QueryPushdownInfo;

/*-------------------------------------------------------------------------
 * Nodes for simple provenance data type
 *-------------------------------------------------------------------------*/

typedef struct SimpleProvData
{
	NodeTag type;
	int numRels;
	Oid *relids;
	List *provtuples;
} SimpleProvData;

typedef struct TupleWrapper
{
	NodeTag type;

} TupleWrapper;

/*-------------------------------------------------------------------------
 * Nodes for transformation provenance
 *-------------------------------------------------------------------------*/

/* types of suboperations of a query node that get ids to be referenced in transformation provenance */
typedef enum SubOperationType
{
	SUBOP_Aggregation,
	SUBOP_Projection,
	SUBOP_Having,
	SUBOP_Selection,
	SUBOP_Join_Inner,
	SUBOP_Join_Left,
	SUBOP_Join_Right,
	SUBOP_Join_Full,
	SUBOP_Join_Cross,
	SUBOP_BaseRel,
	SUBOP_SetOp_Union,
	SUBOP_SetOp_Diff,
	SUBOP_SetOp_Intersect
} SubOperationType;

/* stores information about one suboperation in a query */
typedef struct TransSubInfo
{
	NodeTag type;
	int id;
	bool isStatic;
	SubOperationType opType;
	List *children;
	Datum setForNode;
	Index rtIndex; 				/* rtIndex if a root node otherwise TRANS_NO_RTINDEX */
	List *annot;
} TransSubInfo;

#define TRANS_NO_RTINDEX -1

/* Stores information needed to compute the transformation provenance of a query node.
 * Especially the ids of parts of the query node.
 */
typedef struct TransProvInfo
{
	NodeTag type;
	bool isStatic;
	Node *root;
	Index transProvAttrNum;
	Index rtIndex;
} TransProvInfo;

/*
 * Nodes for meta-querying
 */

typedef struct XsltFuncExpr
{
	NodeTag type;
	char *funcName;
	Node *param;
} XsltFuncExpr;

typedef struct ThisExpr
{
	NodeTag type;
	char *thisType;
} ThisExpr;

/*
 * -------------------------------------------------------------------------
 * 			Node functions
 * -------------------------------------------------------------------------
 */

/* make functions */
extern Query *makeQuery (void);
extern ProvInfo *makeProvInfo (void);
extern RangeTblEntry *makeRte (RTEKind);
extern SelectionInfo *makeSelectionInfo (void);
extern PushdownInfo *makePushdownInfo (void);
extern EquivalenceList *makeEquivalenceList (void);
extern CorrVarInfo *makeCorrVar (Var *corrVar, List *vars, Node *parent,
								Node *exprRoot, SublinkLocation location,
								bool outside, RangeTblEntry *refRTE,
								bool belowAgg, bool belowSet, int trueVarUp);
extern InequalityGraph *makeInequalityGraph (void);
extern InequalityGraphNode *makeInequalityGraphNode (Node *content);
extern InequalityGraphNode *makeInequalityGraphNodeNIL (void);
extern QueryPushdownInfo *makeQueryPushdownInfo (void);
extern SelScope *makeSelScope (void);
extern CopyMap *makeCopyMap (void);
extern CopyProvInfo *makeCopyProvInfo (void);
extern CopyMapRelEntry *makeCopyMapRelEntry (void);
extern CopyMapEntry *makeCopyMapEntry (void);
extern TransProvInfo *makeTransProvInfo (void);
extern TransSubInfo *makeTransSubInfo (int id, SubOperationType opType);

/* copy functions */
extern void *copyProvNode (void *from);

/* read functions */
extern Node *parseProvNodeString(char *token, int length);

/* out functions */
extern void outProvNode(StringInfo str, void *obj);

/* equals functions */
extern bool provNodesEquals(void *a, void *b);

/*
 * -------------------------------------------------------------------------
 * 		convenience macros to access rewrite information from the provInfo field of a query node
 * -------------------------------------------------------------------------
 */

/* get the QueryPushdownInfo for a query node */
#define getQueryInfo(query) \
	((QueryPushdownInfo *) (((Query *) query)->provInfo))

/* sets the alias aliasname and eref aliasname fields of an RTE */
#define SetRteNames(rte,name) \
	do { \
		((RangeTblEntry *) rte)->alias->aliasname = name; \
		((RangeTblEntry *) rte)->eref->aliasname = name; \
	} while (0)

/* is a query marked for provenance rewrite? */
#define IsProvRewrite(query) \
	((((Query *) (query))->provInfo != NULL) && ((ProvInfo *) ((Query *) query)->provInfo)->shouldRewrite)

/* mark or unmark a query for provenance rewrite */
#define SetProvRewrite(query,value) \
	do { \
		if (!(query->provInfo)) \
			query->provInfo = (Node *) makeProvInfo(); \
		((ProvInfo *) ((Query *) (query))->provInfo)->shouldRewrite = (value);  \
	} while (0)

/* get the provinfo */
#define Provinfo(query) \
	((ProvInfo *) ((Query *) query)->provInfo)

/* get the copymap stored in a provinfo */
#define GetInfoCopyMap(query) \
	((CopyMap *) (Provinfo(query))->copyInfo)

/* true if the sublinks of a query have been rewritten */
#define IsSublinkRewritten(query) \
	((((Query *) (query))->provInfo != NULL) && ((ProvInfo *) ((Query *) query)->provInfo)->provSublinkRewritten)

/* set true if the sublinks of a query have been rewritten */
#define SetSublinkRewritten(query, value) \
	do { \
		if (!(query->provInfo)) \
			query->provInfo = (Node *) makeProvInfo(); \
 		((ProvInfo *) ((Query *) (query))->provInfo)->provSublinkRewritten = (value);  \
	} while (0)

/* get ContributionType */
#define ContributionType(query) \
	(((ProvInfo *) ((Query *) query)->provInfo)->contribution)

/* checks if the expr of a SelectionInfo contains no aggregates, no volatile functions and no sublinks */
#define SelPushable(sel) \
	(!(((SelectionInfo *) sel)->containsAggs) && !((SelectionInfo *) sel)->hasSublinks && !((SelectionInfo *) sel)->hasVolatileFuncs)

#define SelNoSubOrVolatile(sel) \
	(!((SelectionInfo *) sel)->hasSublinks && !((SelectionInfo *) sel)->hasVolatileFuncs)

#define GET_TRANS_INFO(query) \
	((TransProvInfo *) ((ProvInfo *) ((Query *) query)->provInfo)->rewriteInfo)

#define SET_TRANS_INFO(query) \
	((TransProvInfo *)(((ProvInfo *) ((Query *) query)->provInfo)->rewriteInfo = (Node *) makeTransProvInfo()))

#define SET_TRANS_INFO_TO(query,newInfo) \
	(((ProvInfo *) ((Query *) query)->provInfo)->rewriteInfo = (Node *) (newInfo))

#define DEL_TRANS_INFO(query) \
	(((ProvInfo *) ((Query *) query)->provInfo)->rewriteInfo = NULL)

#define TRANS_SET_RTINDEX(node, value) \
	do { \
		if (IsA(node, TransSubInfo)) \
			((TransSubInfo *) node)->rtIndex = (value); \
		else \
			((TransProvInfo *) node)->rtIndex = (value); \
	} while (0)

#define TRANS_GET_RTINDEX(node) \
	(IsA(node, TransProvInfo) ? ((TransProvInfo *) node)->rtIndex : ((TransSubInfo *) node)->rtIndex)

#define TRANS_GET_STATIC(node) \
	(IsA(node, TransProvInfo) ? ((TransProvInfo *) node)->isStatic : ((TransSubInfo *) node)->isStatic)

#endif /* PROV_NODES_H_ */
