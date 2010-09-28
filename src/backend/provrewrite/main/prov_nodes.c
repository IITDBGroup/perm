/*-------------------------------------------------------------------------
 *
 * prov_nodes.c
 *	  PERM C -  Implementation of the standard postgres node functions (serialize as string, deserialize from string, deep copy,
 *	  			deep comparison) for the new node types defined in prov_nodes.h
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/provrewrite/prov_nodes.c,v 1.542 15.12.2008 12:42:44 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "nodes/parsenodes.h"
#include "lib/stringinfo.h"
#include "nodes/readfuncs.h"
#include "utils/datum.h"

#include "provrewrite/prov_nodes.h"
#include "provrewrite/provrewrite.h"

/*
 * Helper macros copied from postgres node function .c files
 */
//TODO extract a .h file for these defs.

/* A few guys need only local_node */
#define READ_LOCALS_NO_FIELDS(nodeTypeName) \
	nodeTypeName *local_node = makeNode(nodeTypeName)

/* And a few guys need only the pg_strtok support fields */
#define READ_TEMP_LOCALS()	\
	char	   *token;		\
	int			length

/* ... but most need both */
#define READ_LOCALS(nodeTypeName)			\
	READ_LOCALS_NO_FIELDS(nodeTypeName);	\
	READ_TEMP_LOCALS()

/* Read an integer field (anything written as ":fldname %d") */
#define READ_INT_FIELD(fldname) \
	token = pg_strtok(&length);		/* skip :fldname */ \
	token = pg_strtok(&length);		/* get field value */ \
	local_node->fldname = atoi(token)

/* Read an unsigned integer field (anything written as ":fldname %u") */
#define READ_UINT_FIELD(fldname) \
	token = pg_strtok(&length);		/* skip :fldname */ \
	token = pg_strtok(&length);		/* get field value */ \
	local_node->fldname = atoui(token)

/* Read an OID field (don't hard-wire assumption that OID is same as uint) */
#define READ_OID_FIELD(fldname) \
	token = pg_strtok(&length);		/* skip :fldname */ \
	token = pg_strtok(&length);		/* get field value */ \
	local_node->fldname = atooid(token)

/* Read a char field (ie, one ascii character) */
#define READ_CHAR_FIELD(fldname) \
	token = pg_strtok(&length);		/* skip :fldname */ \
	token = pg_strtok(&length);		/* get field value */ \
	local_node->fldname = token[0]

/* Read an enumerated-type field that was written as an integer code */
#define READ_ENUM_FIELD(fldname, enumtype) \
	token = pg_strtok(&length);		/* skip :fldname */ \
	token = pg_strtok(&length);		/* get field value */ \
	local_node->fldname = (enumtype) atoi(token)

/* Read a float field */
#define READ_FLOAT_FIELD(fldname) \
	token = pg_strtok(&length);		/* skip :fldname */ \
	token = pg_strtok(&length);		/* get field value */ \
	local_node->fldname = atof(token)

/* Read a boolean field */
#define READ_BOOL_FIELD(fldname) \
	token = pg_strtok(&length);		/* skip :fldname */ \
	token = pg_strtok(&length);		/* get field value */ \
	local_node->fldname = strtobool(token)

/* Read a character-string field */
#define READ_STRING_FIELD(fldname) \
	token = pg_strtok(&length);		/* skip :fldname */ \
	token = pg_strtok(&length);		/* get field value */ \
	local_node->fldname = nullable_string(token, length)

/* Read a Node field */
#define READ_NODE_FIELD(fldname) \
	token = pg_strtok(&length);		/* skip :fldname */ \
	local_node->fldname = nodeRead(NULL, 0)

/* Read Datum Varbit field */
#define READ_BITSET_FIELD(fldname) \
	(local_node->fldname = readDatum(false))

/* Routine exit */
#define READ_DONE() \
	return local_node

/*
 * NOTE: use atoi() to read values written with %d, or atoui() to read
 * values written with %u in outfuncs.c.  An exception is OID values,
 * for which use atooid().	(As of 7.1, outfuncs.c writes OIDs as %u,
 * but this will probably change in the future.)
 */
#define atoui(x)  ((unsigned int) strtoul((x), NULL, 10))

#define atooid(x)  ((Oid) strtoul((x), NULL, 10))

#define strtobool(x)  ((*(x) == 't') ? true : false)

#define nullable_string(token,length)  \
	((length) == 0 ? NULL : debackslash(token, length))

/* Compare a simple scalar field (int, float, bool, enum, etc) */
#define COMPARE_SCALAR_FIELD(fldname) \
	do { \
		if (a->fldname != b->fldname) \
			return false; \
	} while (0)

/* Compare a field that is a pointer to some kind of Node or Node tree */
#define COMPARE_NODE_FIELD(fldname) \
	do { \
		if (!equal(a->fldname, b->fldname)) \
			return false; \
	} while (0)

/* Compare a field that is a pointer to a Bitmapset */
#define COMPARE_BITMAPSET_FIELD(fldname) \
	do { \
		if (!bms_equal(a->fldname, b->fldname)) \
			return false; \
	} while (0)

/* Compare a field that is a pointer to a C string, or perhaps NULL */
#define COMPARE_STRING_FIELD(fldname) \
	do { \
		if (!equalstr(a->fldname, b->fldname)) \
			return false; \
	} while (0)

/* Macro for comparing string fields that might be NULL */
#define equalstr(a, b)	\
	(((a) != NULL && (b) != NULL) ? (strcmp(a, b) == 0) : (a) == (b))

/* Compare a field that is a pointer to a simple palloc'd object of size sz */
#define COMPARE_POINTER_FIELD(fldname, sz) \
	do { \
		if (memcmp(a->fldname, b->fldname, (sz)) != 0) \
			return false; \
	} while (0)

/* Compare a Datum Varbit field */
#define COMPARE_BITSET_FIELD(fldname) \
		(datumIsEqual(a->fldname, b->fldname, false, -1))

/* Write the label for the node type */
#define WRITE_NODE_TYPE(nodelabel) \
	appendStringInfoString(str, nodelabel)

/* Write an integer field (anything written as ":fldname %d") */
#define WRITE_INT_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %d", node->fldname)

/* Write an var length int array field */
#define WRITE_VARLENGTHINT_FIELD(fldname, numMembers) \
	(appendStringInfo(str, " :" CppAsString(fldname) " ") , \
		_outVarLenghtIntArray(str, node->fldname, numMembers))

/* Write an unsigned integer field (anything written as ":fldname %u") */
#define WRITE_UINT_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %u", node->fldname)

/* Write an OID field (don't hard-wire assumption that OID is same as uint) */
#define WRITE_OID_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %u", node->fldname)

/* Write a long-integer field */
#define WRITE_LONG_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %ld", node->fldname)

/* Write a char field (ie, one ascii character) */
#define WRITE_CHAR_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %c", node->fldname)

/* Write an enumerated-type field as an integer code */
#define WRITE_ENUM_FIELD(fldname, enumtype) \
	appendStringInfo(str, " :" CppAsString(fldname) " %d", \
					 (int) node->fldname)

/* Write a float field --- caller must give format to define precision */
#define WRITE_FLOAT_FIELD(fldname,format) \
	appendStringInfo(str, " :" CppAsString(fldname) " " format, node->fldname)

/* Write a boolean field */
#define WRITE_BOOL_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %s", \
					 booltostr(node->fldname))

/* Write a character-string (possibly NULL) field */
#define WRITE_STRING_FIELD(fldname) \
	(appendStringInfo(str, " :" CppAsString(fldname) " "), \
	 _outToken(str, node->fldname))

/* Write a Node field */
#define WRITE_NODE_FIELD(fldname) \
	(appendStringInfo(str, " :" CppAsString(fldname) " "), \
	 _outNode(str, node->fldname))

/* Write a bitmapset field */
#define WRITE_BITMAPSET_FIELD(fldname) \
	(appendStringInfo(str, " :" CppAsString(fldname) " "), \
	 _outBitmapset(str, node->fldname))

/* Write a Datum-Varbit field */
#define WRITE_BITSET_FIELD(fldname) \
	(appendStringInfo(str, " :" CppAsString(fldname) " "), \
	_outDatum(str, node->fldname, -1, false))

#define booltostr(x)  ((x) ? "true" : "false")

/* Copy a simple scalar field (int, float, bool, enum, etc) */
#define COPY_SCALAR_FIELD(fldname) \
	(newnode->fldname = from->fldname)

/* Copy a field that is a pointer to some kind of Node or Node tree */
#define COPY_NODE_FIELD(fldname) \
	(newnode->fldname = copyObject(from->fldname))

/* Copy a field that is a pointer to a Bitmapset */
#define COPY_BITMAPSET_FIELD(fldname) \
	(newnode->fldname = bms_copy(from->fldname))

/* Copy a field that is a pointer to a C string, or perhaps NULL */
#define COPY_STRING_FIELD(fldname) \
	(newnode->fldname = from->fldname ? pstrdup(from->fldname) : (char *) NULL)

/* Copy a field that is a pointer to a simple palloc'd object of size sz */
#define COPY_POINTER_FIELD(fldname, sz) \
	do { \
		Size	_size = (sz); \
		newnode->fldname = palloc(_size); \
		memcpy(newnode->fldname, from->fldname, _size); \
	} while (0)

/* Copy a varbit Datum */
#define COPY_BITSET_FIELD(fldname) \
	(newnode->fldname = datumCopy(from->fldname, false, -1))


/*
 * Function declarations
 */

/* copy functions */
static CorrVarInfo *_copyCorrVarInfo(CorrVarInfo *from);
static ProvInfo *_copyProvInfo(ProvInfo *from);
static SublinkInfo *_copySublinkInfo(SublinkInfo *from);
static SelectionInfo *_copySelectionInfo(SelectionInfo *from);
static EquivalenceList *_copyEquivalenceList(EquivalenceList *from);
static PushdownInfo *_copyPushdownInfo(PushdownInfo *from);
static InequalityGraph *_copyInequalityGraph(InequalityGraph *from);
static InequalityGraphNode *_copyInequalityGraphNode(InequalityGraphNode *from);
static QueryPushdownInfo *_copyQueryPushdownInfo(QueryPushdownInfo *from);
static SelScope *_copySelScope(SelScope *from);
static CopyMap *_copyCopyMap(CopyMap *from);
static CopyMapRelEntry *_copyCopyMapRelEntry(CopyMapRelEntry *from);
static CopyMapEntry *_copyCopyMapEntry(CopyMapEntry *from);
static CopyProvAttrInfo *_copyCopyProvAttrInfo(CopyProvAttrInfo *from);
static AttrInclusions *_copyAttrInclusions(AttrInclusions *from);
static InclusionCond *_copyInclusionCond(InclusionCond *from);
static TransProvInfo *_copyTransProvInfo(TransProvInfo *from);
static TransSubInfo *_copyTransSubInfo(TransSubInfo *from);
static XsltFuncExpr *_copyXsltFuncExpr(XsltFuncExpr *from);
static ThisExpr *_copyThisExpr(ThisExpr *from);

static int *_copyVarLengthIntArray(int *array, int numMembers);


/* copy repair functions */
static void _repairCyclesQueryPushdownInfo(QueryPushdownInfo *newnode, QueryPushdownInfo *from);
static void _repairCyclesQueryPushdownInfoCreateScopesList (SelScope *sel, QueryPushdownInfo *info);
static void _repairCyclesSelScope(SelScope *newnode, SelScope *from);
static void _repairCyclesSelScopeCreateSelInfoList(EquivalenceList *eq, SelScope *scope);

/* read functions */
static SublinkInfo *_readSublinkInfo(void);
static CorrVarInfo *_readCorrVarInfo(void);
static ProvInfo *_readProvInfo(void);
static SelectionInfo *_readSelectionInfo(void);
static EquivalenceList *_readEquivalenceList(void);
static PushdownInfo *_readPushdownInfo(void);
static InequalityGraph *_readInequalityGraph(void);
static InequalityGraphNode *_readInequalityGraphNode(void);
static CopyMap *_readCopyMap(void);
//static CopyProvInfo *_readCopyProvInfo(void);
static CopyMapRelEntry *_readCopyMapRelEntry(void);
static CopyMapEntry *_readCopyMapEntry(void);
static CopyProvAttrInfo *_readCopyProvAttrInfo(void);
static AttrInclusions *_readAttrInclusions(void);
static InclusionCond *_readInclusionCond(void);
static TransProvInfo *_readTransProvInfo(void);
static TransSubInfo *_readTransSubInfo(void);
static XsltFuncExpr *_readXsltFuncExpr(void);
static ThisExpr *_readThisExpr(void);

/* out functions */
static void _outSublinkInfo(StringInfo str, SublinkInfo *node);
static void _outCorrVarInfo(StringInfo str, CorrVarInfo *node);
static void _outProvInfo(StringInfo str, ProvInfo *node);
static void _outSelectionInfo(StringInfo str, SelectionInfo *node);
static void _outEquivalenceList(StringInfo str, EquivalenceList *node);
static void _outPushdownInfo(StringInfo str, PushdownInfo *node);
static void _outInequalityGraph(StringInfo str, InequalityGraph *node);
static void _outInequalityGraphNode(StringInfo str, InequalityGraphNode *node);
static void _outQueryPushdownInfo(StringInfo str, QueryPushdownInfo *node);
static void _outCopyMap (StringInfo str, CopyMap *node);
//static void _outCopyProvInfo (StringInfo str, CopyProvInfo *node);
static void _outCopyMapRelEntry (StringInfo str, CopyMapRelEntry *node);
static void _outCopyMapEntry (StringInfo str, CopyMapEntry *node);
static void _outAttrInclusions (StringInfo str, AttrInclusions *node);
static void _outInclusionCond (StringInfo str, InclusionCond *node);
static void _outCopyProvAttrInfo (StringInfo str, CopyProvAttrInfo *node);
static void _outSelScope(StringInfo str, SelScope *node);
static void _outTransProvInfo(StringInfo str, TransProvInfo *node);
static void _outTransSubInfo(StringInfo str, TransSubInfo *node);
static void _outXsltFuncExpr(StringInfo str, XsltFuncExpr *node);
static void _outThisExpr(StringInfo str, ThisExpr *node);

static void _outVarLenghtIntArray (StringInfo str, int *array, int numMembers);
static void _outToken(StringInfo str, char *s);


/* equals functions */
static bool _equalSublinkInfo(SublinkInfo *a, SublinkInfo *b);
static bool _equalCorrVarInfo(CorrVarInfo *a, CorrVarInfo *b);
static bool _equalProvInfo(ProvInfo *a, ProvInfo *b);
static bool _equalSelectionInfo(SelectionInfo *a, SelectionInfo *b);
static bool _equalEquivalenceList(EquivalenceList *a, EquivalenceList *b);
static bool _equalPushdownInfo(PushdownInfo *a, PushdownInfo *b);
static bool _equalInequalityGraph(InequalityGraph *a, InequalityGraph *b);
static bool _equalInequalityGraphNode(InequalityGraphNode *a, InequalityGraphNode *b);
static bool _equalQueryPushdownInfo(QueryPushdownInfo *a, QueryPushdownInfo *b);
static bool _equalSelScope(SelScope *a, SelScope *b);
static bool _equalCopyMap (CopyMap *a, CopyMap *b);
//static bool _equalCopyProvInfo (CopyProvInfo *a, CopyProvInfo *b);
static bool _equalCopyMapRelEntry (CopyMapRelEntry *a, CopyMapRelEntry *b);
static bool _equalCopyMapEntry (CopyMapEntry *a, CopyMapEntry *b);
static bool _equalAttrInclusions (AttrInclusions *a, AttrInclusions *b);
static bool _equalInclusionCond (InclusionCond *a, InclusionCond *b);
static bool _equalCopyProvAttrInfo (CopyProvAttrInfo *a, CopyProvAttrInfo *b);
static bool _equalVarLengthIntArray (int *a, int *b, int numA, int numB);
static bool _equalTransProvInfo (TransProvInfo *a, TransProvInfo *b);
static bool _equalTransSubInfo (TransSubInfo *a, TransSubInfo *b);
static bool _equalXsltFuncExpr (XsltFuncExpr *a, XsltFuncExpr *b);
static bool _equalThisExpr (ThisExpr *a, ThisExpr *b);

/*
 * Make functions
 */

/*
 * Creates a Query node and intializes its fields with default values. Lists are initialized to NIL, Node fields to NULL
 * and boolean and constant values to reasonable defaults.
 */

Query *
makeQuery (void)
{
	Query *result;

	result = makeNode(Query);
	result->canSetTag = true;
	result->commandType = CMD_SELECT;
	result->hasAggs = false;
	result->targetList = NIL;
	result->hasSubLinks = false;
	result->jointree = makeNode(FromExpr);
	result->jointree->fromlist = NIL;
	result->groupClause = NIL;
	result->havingQual = NULL;
	result->distinctClause = NIL;
	result->sortClause = NIL;
	result->resultRelation = 0;
	result->returningList = NIL;
	result->rowMarks = NIL;

	result->provInfo = (Node *) makeProvInfo();
//	SetProvRewrite(result,false);
//	SetSublinkRewritten(result,false);

	return result;
}

ProvInfo *
makeProvInfo (void)
{
	ProvInfo *result;

	result = makeNode(ProvInfo);
	result->copyInfo = NULL;
	result->contribution = CONTR_INFLUENCE;
	result->rewriteInfo = NULL;
	result->annotations = NIL;
	result->provSublinkRewritten = false;
	result->shouldRewrite = false;

	return result;
}

RangeTblEntry *
makeRte (RTEKind type)
{
	RangeTblEntry *result;

	result = makeNode(RangeTblEntry);

	result->rtekind = type;
	result->alias = makeNode(Alias);
	result->alias->aliasname = NULL;
	result->alias->colnames = NIL;
	result->eref = makeNode(Alias);
	result->eref->aliasname = NULL;
	result->eref->colnames = NIL;
	result->checkAsUser = InvalidOid;
	result->inFromCl = true;
	result->inh = false;
	result->isProvBase = false;
	result->requiredPerms = ACL_SELECT;
	result->annotations = NIL;

	switch(type)
	{
		case RTE_RELATION:
			result->relid = InvalidOid;
		break;
		case RTE_SUBQUERY:
			result->subquery = NULL;
		break;
		case RTE_JOIN:
			result->joinaliasvars = NIL;
			result->jointype = JOIN_INNER;
		break;
		default:
			break;
	}

	return result;
}

SelectionInfo *
makeSelectionInfo (void)
{
	SelectionInfo *result;

	result = makeNode(SelectionInfo);

	result->derived = false;
	result->notMovable = false;
	result->vars = NIL;
	result->eqExpr = NULL;

	return result;
}

PushdownInfo *
makePushdownInfo (void)
{
	PushdownInfo *result;

	result = makeNode(PushdownInfo);

	result->conjuncts = NIL;
	result->equiLists = NIL;
	result->contradiction = false;

	return result;
}

EquivalenceList *
makeEquivalenceList (void)
{
	EquivalenceList *result;

	result = makeNode(EquivalenceList);
	result->exprs = NIL;
	result->derivedSels = NIL;
	result->genSels = NIL;
//	result->constant = NULL;

	return result;
}

CorrVarInfo *
makeCorrVar (Var *corrVar, List *vars, Node *parent, Node *exprRoot, SublinkLocation location,
		bool outside, RangeTblEntry *refRTE, bool belowAgg, bool belowSet, int trueVarUp)
{
	CorrVarInfo *result;

	result = makeNode(CorrVarInfo);

	result->corrVar = corrVar;
	result->vars = vars;
	result->parent = parent;
	result->parentQuery = (Query *) parent;
	result->exprRoot = exprRoot;
	result->location = location;
	result->outside = outside;
	result->refRTE = refRTE;
	result->trueVarLevelsUp = trueVarUp;
	result->belowAgg = belowAgg;
	result->belowSet = belowSet;

	return result;
}

InequalityGraph *
makeInequalityGraph (void)
{
	InequalityGraph *result;

	result = makeNode(InequalityGraph);

	result->equiLists = NIL;
	result->nodes = NIL;

	return result;
}

InequalityGraphNode *
makeInequalityGraphNode (Node *content)
{
	InequalityGraphNode *result;

	result = makeInequalityGraphNodeNIL ();
	result->equis = list_make1(content);

	return result;
}

InequalityGraphNode *
makeInequalityGraphNodeNIL (void)
{
	InequalityGraphNode *result;

	result = makeNode(InequalityGraphNode);

	result->lessEqThen = NIL;
	result->lessThen = NIL;
	result->greaterEqThen = NIL;
	result->greaterThen = NIL;
	result->equis = NIL;
	result->consts = NIL;

	return result;
}

QueryPushdownInfo *
makeQueryPushdownInfo (void)
{
	QueryPushdownInfo *result;

	result = makeNode(QueryPushdownInfo);

	result->children = NIL;
	result->qualPointers = NIL;
	result->scopes = NIL;
	result->validityScopes = NIL;

	return result;
}

SelScope *
makeSelScope (void)
{
	SelScope *result;

	result = makeNode(SelScope);

	result->children = NIL;
	result->contradiction = false;
	result->equiLists = NIL;
	result->selInfos = NIL;
	result->topIndex = -1;
	result->scopeType = SCOPE_NORMAL;

	return result;
}

CopyMap *
makeCopyMap (void)
{
	CopyMap *result;

	result = makeNode(CopyMap);

	result->entries = NIL;
	result->rtindex = -1;

	return result;
}

//CopyProvInfo *
//makeCopyProvInfo (void)
//{
//	CopyProvInfo *result;
//
//	result = makeNode(CopyProvInfo);
//
//	result->inMap = NULL;
//	result->outMap = NULL;
//
//	return result;
//}

CopyMapRelEntry *
makeCopyMapRelEntry (void)
{
	CopyMapRelEntry *result;

	result = makeNode(CopyMapRelEntry);

	result->attrEntries = NIL;
	result->relation = InvalidOid;
	result->rtindex = -1;
	result->isStatic = false;
	result->noRewrite = false;
	result->refNum = -1;
	result->child = NULL;
	result->provAttrInfo = NULL;
	result->provAttrs = NIL;

	return result;
}

CopyMapEntry *
makeCopyMapEntry (void)
{
	CopyMapEntry *result;

	result = makeNode(CopyMapEntry);

	result->baseRelAttr = NULL;
	result->isStaticTrue = false;
	result->isStaticFalse = false;
	result->outAttrIncls = NIL;
	result->provAttrName = NULL;

	return result;
}

AttrInclusions *
makeAttrInclusions (void)
{
	AttrInclusions *result;

	result = makeNode(AttrInclusions);

	result->attr = NULL;
	result->inclConds = NIL;
	result->isStatic = false;

	return result;
}

InclusionCond *
makeInclusionCond (void)
{
	InclusionCond *result;

	result = makeNode(InclusionCond);

	result->cond = NULL;
	result->eqVars = NIL;
	result->existsAttr = NULL;
	result->inclType = INCL_EXISTS;

	return result;
}

TransProvInfo *
makeTransProvInfo (void)
{
	TransProvInfo *result;

	result = makeNode(TransProvInfo);

	result->isStatic = false;
	result->root = NULL;
	result->rtIndex = TRANS_NO_RTINDEX;

	return result;
}

TransSubInfo *
makeTransSubInfo (int id, SubOperationType opType)
{
	TransSubInfo *result;

	result = makeNode(TransSubInfo);

	result->id = id;
	result->opType = opType;
	result->children = NIL;
	result->isStatic = false;
	//TODO result->setForNode; is left uninitialized
	result->rtIndex = TRANS_NO_RTINDEX;
	result->annot = NIL;

	return result;
}


Query *
flatCopyWithoutProvInfo (Query *from)
{
	Query	   *newnode = makeNode(Query);

	COPY_SCALAR_FIELD(commandType);
	COPY_SCALAR_FIELD(querySource);
	COPY_SCALAR_FIELD(canSetTag);
	COPY_NODE_FIELD(utilityStmt);
	COPY_SCALAR_FIELD(resultRelation);
	COPY_NODE_FIELD(intoClause);
	COPY_SCALAR_FIELD(hasAggs);
	COPY_SCALAR_FIELD(hasSubLinks);
	COPY_NODE_FIELD(jointree);
	COPY_NODE_FIELD(targetList);
	COPY_NODE_FIELD(returningList);
	COPY_NODE_FIELD(groupClause);
	COPY_NODE_FIELD(havingQual);
	COPY_NODE_FIELD(distinctClause);
	COPY_NODE_FIELD(sortClause);
	COPY_NODE_FIELD(limitOffset);
	COPY_NODE_FIELD(limitCount);
	COPY_NODE_FIELD(rowMarks);
	COPY_NODE_FIELD(setOperations);
	newnode->provInfo = (Node *) makeProvInfo();
	newnode->rtable = NIL;

	return newnode;
}


/*
 * EQUAL functions
 */


bool
provNodesEquals(void *a, void *b)
{
	bool retval;

	switch(nodeTag(a))
	{
	case T_SublinkInfo:
		retval = _equalSublinkInfo(a, b);
		break;
	case T_CorrVarInfo:
		retval = _equalCorrVarInfo(a, b);
		break;
	case T_ProvInfo:
		retval = _equalProvInfo(a, b);
		break;
	case T_SelectionInfo:
		retval = _equalSelectionInfo(a, b);
		break;
	case T_EquivalenceList:
		retval = _equalEquivalenceList(a, b);
		break;
	case T_PushdownInfo:
		retval = _equalPushdownInfo(a, b);
		break;
	case T_InequalityGraph:
		retval = _equalInequalityGraph(a, b);
		break;
	case T_InequalityGraphNode:
		retval = _equalInequalityGraphNode(a, b);
		break;
	case T_QueryPushdownInfo:
		retval = _equalQueryPushdownInfo(a,b);
		break;
	case T_SelScope:
		retval = _equalSelScope(a,b);
		break;
	case T_CopyMap:
		retval = _equalCopyMap(a,b);
		break;
	case T_CopyMapRelEntry:
		retval = _equalCopyMapRelEntry(a,b);
		break;
	case T_CopyMapEntry:
		retval = _equalCopyMapEntry(a,b);
		break;
	case T_CopyProvAttrInfo:
		retval = _equalCopyProvAttrInfo(a,b);
		break;
	case T_AttrInclusions:
		retval = _equalAttrInclusions(a,b);
		break;
	case T_InclusionCond:
		retval = _equalInclusionCond(a,b);
		break;
	case T_TransProvInfo:
		retval = _equalTransProvInfo(a,b);
		break;
	case T_TransSubInfo:
		retval = _equalTransSubInfo(a,b);
		break;
	case T_XsltFuncExpr:
		retval = _equalXsltFuncExpr(a,b);
		break;
	case T_ThisExpr:
		retval = _equalThisExpr(a,b);
		break;
	default:
		elog(ERROR, "unrecognized node type: %d",
			 (int) nodeTag(a));
		retval = false;		/* keep compiler quiet */
		break;
	}

	return retval;
}

static bool
_equalSublinkInfo(SublinkInfo *a, SublinkInfo *b)
{
	COMPARE_SCALAR_FIELD(location);
	COMPARE_SCALAR_FIELD(category);
	COMPARE_SCALAR_FIELD(sublinkPos);
	COMPARE_SCALAR_FIELD(aggLoc);
	COMPARE_NODE_FIELD(sublink);
	COMPARE_NODE_FIELD(parent);
	COMPARE_POINTER_FIELD(grandParentExprPointer, sizeof(void *));
	COMPARE_NODE_FIELD(exprRoot);
	COMPARE_NODE_FIELD(rootCopy);
	COMPARE_POINTER_FIELD(subLinkExprPointer, sizeof(void *));
	COMPARE_NODE_FIELD(aggOrGroup);
	COMPARE_POINTER_FIELD(aggOrGroupPointer, sizeof(void *));
	COMPARE_NODE_FIELD(corrVarInfos);
	COMPARE_NODE_FIELD(condRTEs);
	COMPARE_NODE_FIELD(corrRTEs);
	COMPARE_NODE_FIELD(targetVar);
	COMPARE_SCALAR_FIELD(sublinkRTindex);
	COMPARE_NODE_FIELD(subLinkJoinQuery);
	COMPARE_SCALAR_FIELD(leftJoinPos);
	COMPARE_NODE_FIELD(rewrittenSublinkQuery);

	return true;
}

static bool
_equalCorrVarInfo(CorrVarInfo *a, CorrVarInfo *b)
{
	COMPARE_NODE_FIELD(corrVar);
	COMPARE_NODE_FIELD(vars);
	COMPARE_NODE_FIELD(parent);
	COMPARE_NODE_FIELD(parentQuery);
	COMPARE_NODE_FIELD(exprRoot);
	COMPARE_SCALAR_FIELD(outside);
	COMPARE_NODE_FIELD(refRTE);
	COMPARE_SCALAR_FIELD(location);
	COMPARE_SCALAR_FIELD(trueVarLevelsUp);
	COMPARE_SCALAR_FIELD(belowAgg);
	COMPARE_SCALAR_FIELD(belowSet);

	return true;
}

static bool
_equalProvInfo(ProvInfo *a, ProvInfo *b)
{
	COMPARE_SCALAR_FIELD(provSublinkRewritten);
	COMPARE_SCALAR_FIELD(shouldRewrite);
	COMPARE_NODE_FIELD(rewriteInfo);

	return true;
}

static bool
_equalSelectionInfo(SelectionInfo *a, SelectionInfo *b)
{
	COMPARE_NODE_FIELD(expr);
	COMPARE_NODE_FIELD(eqExpr);
	COMPARE_NODE_FIELD(vars);
	COMPARE_SCALAR_FIELD(notMovable);
	COMPARE_SCALAR_FIELD(derived);
	COMPARE_SCALAR_FIELD(rtOrigin);

	return true;
}

static bool
_equalEquivalenceList(EquivalenceList *a, EquivalenceList *b)
{
	COMPARE_NODE_FIELD(exprs);
	COMPARE_NODE_FIELD(genSels);
	COMPARE_NODE_FIELD(exprs);
	COMPARE_NODE_FIELD(derivedSels);
	COMPARE_NODE_FIELD(constant);
	//COMPARE_NODE_FIELD(scope); dont do it -> cycles

	return true;
}

static bool
_equalPushdownInfo(PushdownInfo *a, PushdownInfo *b)
{
	COMPARE_NODE_FIELD(conjuncts);
	COMPARE_NODE_FIELD(equiLists);
	COMPARE_SCALAR_FIELD(contradiction);

	return true;
}

static bool
_equalInequalityGraph(InequalityGraph *a, InequalityGraph *b)
{
	COMPARE_NODE_FIELD(equiLists);
	COMPARE_NODE_FIELD(nodes);

	return true;
}

static bool
_equalInequalityGraphNode(InequalityGraphNode *a, InequalityGraphNode *b)
{
	COMPARE_NODE_FIELD(equis);
	COMPARE_NODE_FIELD(lessThen);
	COMPARE_NODE_FIELD(lessEqThen);
	COMPARE_NODE_FIELD(greaterThen);
	COMPARE_NODE_FIELD(greaterEqThen);
	COMPARE_NODE_FIELD(consts);

	return true;
}

static bool
_equalQueryPushdownInfo(QueryPushdownInfo *a, QueryPushdownInfo *b)
{
	//COMPARE_NODE_FIELD(query); don't do it can cause cycles
	//COMPARE_NODE_FIELD(scopes); don't do it redundent because top scope links to other scopes of QueryPushdown
	COMPARE_NODE_FIELD(topScope);
	COMPARE_NODE_FIELD(qualPointers);
	//COMPARE_NODE_FIELD(parent); don't do it can cause cycles
	COMPARE_NODE_FIELD(children);
	COMPARE_NODE_FIELD(validityScopes);

	return true;
}

static bool
_equalSelScope(SelScope *a, SelScope *b)
{
	COMPARE_BITMAPSET_FIELD(baseRTEs);
	COMPARE_BITMAPSET_FIELD(joinRTEs);
	COMPARE_NODE_FIELD(equiLists);
	COMPARE_NODE_FIELD(selInfos);
	//COMPARE_NODE_FIELD(*parent); don't do it can cause cycles
	COMPARE_BITMAPSET_FIELD(childRTEs);
	COMPARE_NODE_FIELD(children);
	COMPARE_SCALAR_FIELD(contradiction);
	COMPARE_SCALAR_FIELD(scopeType);
	//COMPARE_NODE_FIELD(pushdown); don't do it can cause cycles
	COMPARE_SCALAR_FIELD(topIndex);
	_equalVarLengthIntArray(a->baseRelMap, b->baseRelMap, bms_num_members(a->baseRTEs), bms_num_members(b->baseRTEs));

	return true;
}

static bool
_equalCopyMap (CopyMap *a, CopyMap *b)
{
	COMPARE_NODE_FIELD(entries);
	COMPARE_SCALAR_FIELD(rtindex);

	return true;
}

//static bool
//_equalCopyProvInfo(CopyProvInfo *a, CopyProvInfo *b)
//{
//	COMPARE_NODE_FIELD(inMap);
//	COMPARE_NODE_FIELD(outMap);
//
//	return true;
//}



static bool
_equalCopyMapRelEntry(CopyMapRelEntry *a, CopyMapRelEntry *b)
{
	COMPARE_SCALAR_FIELD(relation);
	COMPARE_SCALAR_FIELD(refNum);
	COMPARE_NODE_FIELD(attrEntries);
	COMPARE_SCALAR_FIELD(rtindex);
	COMPARE_SCALAR_FIELD(isStatic);
	COMPARE_SCALAR_FIELD(noRewrite);
	COMPARE_NODE_FIELD(child);
	COMPARE_NODE_FIELD(provAttrInfo);
	COMPARE_NODE_FIELD(provAttrs);

	return true;
}

static bool
_equalCopyMapEntry(CopyMapEntry *a, CopyMapEntry *b)
{
	COMPARE_NODE_FIELD(baseRelAttr);
	COMPARE_STRING_FIELD(provAttrName);
	COMPARE_NODE_FIELD(outAttrIncls);
	COMPARE_SCALAR_FIELD(isStaticTrue);
	COMPARE_SCALAR_FIELD(isStaticFalse);

	return true;
}

static bool
_equalAttrInclusions (AttrInclusions *a, AttrInclusions *b)
{
	COMPARE_NODE_FIELD(attr);
	COMPARE_NODE_FIELD(inclConds);
	COMPARE_SCALAR_FIELD(isStatic);

	return true;
}

static bool
_equalInclusionCond (InclusionCond *a, InclusionCond *b)
{
	COMPARE_SCALAR_FIELD(inclType);
	COMPARE_NODE_FIELD(existsAttr);
	COMPARE_NODE_FIELD(eqVars);
	COMPARE_NODE_FIELD(cond);

	return true;
}

static bool
_equalCopyProvAttrInfo (CopyProvAttrInfo *a, CopyProvAttrInfo *b)
{
	COMPARE_NODE_FIELD(provVar);
	COMPARE_NODE_FIELD(bitSetComposition);
	COMPARE_SCALAR_FIELD(outAttrNum);

	return true;
}

static bool
_equalVarLengthIntArray (int *a, int *b, int numA, int numB)
{
	int i;

	if (numA != numB)
		return false;

	if (a == NULL && b == NULL)
		return true;

	if (a == NULL || b == NULL)
		return false;

	for(i = 0; i < numA; i++)
	{
		if(a[i] != b[i])
			return false;
	}

	return true;
}

static bool
_equalTransProvInfo (TransProvInfo *a, TransProvInfo *b)
{
	COMPARE_SCALAR_FIELD(isStatic);
	COMPARE_NODE_FIELD(root);
	COMPARE_SCALAR_FIELD(transProvAttrNum);
	COMPARE_SCALAR_FIELD(rtIndex);

	return true;
}

static bool
_equalTransSubInfo (TransSubInfo *a, TransSubInfo *b)
{
	COMPARE_SCALAR_FIELD(id);
	COMPARE_SCALAR_FIELD(isStatic);
	COMPARE_SCALAR_FIELD(opType);
	COMPARE_NODE_FIELD(children);
	COMPARE_BITSET_FIELD(setForNode);
	COMPARE_SCALAR_FIELD(rtIndex);
	COMPARE_NODE_FIELD(annot);

	return true;
}

static bool
_equalXsltFuncExpr (XsltFuncExpr *a, XsltFuncExpr *b)
{
	COMPARE_STRING_FIELD(funcName);
	COMPARE_NODE_FIELD(param);

	return true;
}

static bool
_equalThisExpr (ThisExpr *a, ThisExpr *b)
{
	COMPARE_STRING_FIELD(thisType);

	return true;
}

/*
 * OUT functions
 */

void
outProvNode(StringInfo str, void *obj)
{
	switch(nodeTag(obj))
	{
	case T_SublinkInfo:
		_outSublinkInfo(str, obj);
		break;
	case T_CorrVarInfo:
		_outCorrVarInfo(str, obj);
		break;
	case T_ProvInfo:
		_outProvInfo(str, obj);
		break;
	case T_SelectionInfo:
		_outSelectionInfo(str, obj);
		break;
	case T_EquivalenceList:
		_outEquivalenceList(str, obj);
		break;
	case T_PushdownInfo:
		_outPushdownInfo(str, obj);
		break;
	case T_InequalityGraph:
		_outInequalityGraph(str, obj);
		break;
	case T_InequalityGraphNode:
		_outInequalityGraphNode(str, obj);
		break;
	case T_QueryPushdownInfo:
		_outQueryPushdownInfo(str, obj);
		break;
	case T_SelScope:
		_outSelScope(str, obj);
		break;
	case T_CopyMap:
		_outCopyMap(str, obj);
		break;
//	case T_CopyProvInfo:
//		_outCopyProvInfo(str, obj);
//		break;
	case T_CopyMapRelEntry:
		_outCopyMapRelEntry(str, obj);
		break;
	case T_CopyMapEntry:
		_outCopyMapEntry(str, obj);
		break;
	case T_AttrInclusions:
		_outAttrInclusions(str, obj);
		break;
	case T_InclusionCond:
		_outInclusionCond(str, obj);
		break;
	case T_CopyProvAttrInfo:
		_outCopyProvAttrInfo(str, obj);
		break;
	case T_TransProvInfo:
		_outTransProvInfo(str, obj);
		break;
	case T_TransSubInfo:
		_outTransSubInfo(str, obj);
		break;
	case T_XsltFuncExpr:
		_outXsltFuncExpr(str, obj);
		break;
	case T_ThisExpr:
		_outThisExpr(str, obj);
		break;
	default:

		/*
		 * This should be an ERROR, but it's too useful to be able to
		 * dump structures that _outNode only understands part of.
		 */
		elog(WARNING, "could not dump unrecognized node type: %d",
			 (int) nodeTag(obj));
		break;
	}
}

static void
_outSublinkInfo(StringInfo str, SublinkInfo *node)
{
	WRITE_NODE_TYPE("SUBLINKINFO");

	WRITE_INT_FIELD(sublinkPos);
	WRITE_ENUM_FIELD(location, SublinkLocation);
	WRITE_ENUM_FIELD(category, SublinkCat);
	WRITE_ENUM_FIELD(aggLoc, SublinkAggLocation);
	WRITE_NODE_FIELD(sublink);
	WRITE_NODE_FIELD(parent);
	WRITE_NODE_FIELD(exprRoot);
	WRITE_NODE_FIELD(rootCopy);
	WRITE_NODE_FIELD(aggOrGroup);
	WRITE_NODE_FIELD(corrVarInfos);
	WRITE_NODE_FIELD(condRTEs);
	WRITE_NODE_FIELD(corrRTEs);
	WRITE_NODE_FIELD(targetVar);
	WRITE_INT_FIELD(sublinkRTindex);
	WRITE_NODE_FIELD(subLinkJoinQuery);
	WRITE_INT_FIELD(leftJoinPos);
	WRITE_NODE_FIELD(rewrittenSublinkQuery);
}

static void
_outCorrVarInfo(StringInfo str, CorrVarInfo *node)
{
	WRITE_NODE_TYPE("CORRVARINFO");

	WRITE_NODE_FIELD(corrVar);
	WRITE_NODE_FIELD(vars);
	WRITE_NODE_FIELD(parent);
	WRITE_NODE_FIELD(parentQuery);
	WRITE_NODE_FIELD(exprRoot);
	WRITE_BOOL_FIELD(outside);
	WRITE_NODE_FIELD(refRTE);
	WRITE_ENUM_FIELD(location, SublinkLocation);
	WRITE_INT_FIELD(trueVarLevelsUp);
	WRITE_BOOL_FIELD(belowAgg);
	WRITE_BOOL_FIELD(belowSet);
}

static void
_outProvInfo(StringInfo str, ProvInfo *node)
{
	WRITE_NODE_TYPE("PROVINFO");

	WRITE_BOOL_FIELD(provSublinkRewritten);
	WRITE_BOOL_FIELD(shouldRewrite);
	WRITE_ENUM_FIELD(contribution, ContributionType);
	WRITE_NODE_FIELD(copyInfo);
	WRITE_NODE_FIELD(rewriteInfo);
	WRITE_NODE_FIELD(annotations);

}

static void
_outSelectionInfo(StringInfo str, SelectionInfo *node)
{
	WRITE_NODE_TYPE("SELECTIONINFO");

	WRITE_NODE_FIELD(expr);
	WRITE_NODE_FIELD(eqExpr);
	WRITE_NODE_FIELD(vars);
	WRITE_BOOL_FIELD(notMovable);
	WRITE_BOOL_FIELD(derived);
	WRITE_INT_FIELD(rtOrigin);
}

static void
_outEquivalenceList(StringInfo str, EquivalenceList *node)
{
	WRITE_NODE_TYPE("EQUIVALENCELIST");

	WRITE_NODE_FIELD(exprs);
	WRITE_NODE_FIELD(constant);
	//TODO
}

static void
_outPushdownInfo(StringInfo str, PushdownInfo *node)
{
	WRITE_NODE_TYPE("PUSHDOWNINFO");

	WRITE_NODE_FIELD(conjuncts);
	WRITE_NODE_FIELD(equiLists);
	WRITE_BOOL_FIELD(contradiction);
}

static void
_outInequalityGraph(StringInfo str, InequalityGraph *node)
{
	WRITE_NODE_TYPE("INEQUALITYGRAPH");

	WRITE_NODE_FIELD(equiLists);
	WRITE_NODE_FIELD(nodes);
}

static void
_outInequalityGraphNode(StringInfo str, InequalityGraphNode *node)
{
	WRITE_NODE_TYPE("INEQUALITYGRAPHNODE");

	WRITE_NODE_FIELD(equis);
	WRITE_NODE_FIELD(lessThen);
	WRITE_NODE_FIELD(lessEqThen);
	WRITE_NODE_FIELD(greaterThen);
	WRITE_NODE_FIELD(greaterEqThen);
	WRITE_NODE_FIELD(consts);
}

static void
_outQueryPushdownInfo(StringInfo str, QueryPushdownInfo *node)
{
	WRITE_NODE_TYPE("QUERYPUSHDOWNINFO");

	//WRITE_NODE_FIELD(query);
	//WRITE_NODE_FIELD(scopes);
	WRITE_NODE_FIELD(topScope);
	WRITE_NODE_FIELD(qualPointers);
	//WRITE_NODE_FIELD(parent);
	WRITE_NODE_FIELD(children);
	WRITE_NODE_FIELD(validityScopes);
}

static void
_outSelScope(StringInfo str, SelScope *node)
{
	WRITE_NODE_TYPE("SELSCOPE");

	WRITE_BITMAPSET_FIELD(baseRTEs);
	WRITE_BITMAPSET_FIELD(joinRTEs);
	WRITE_NODE_FIELD(equiLists);
	WRITE_NODE_FIELD(selInfos);
	//WRITE_NODE_FIELD(parent);
	WRITE_BITMAPSET_FIELD(childRTEs);
	WRITE_NODE_FIELD(children);
	WRITE_BOOL_FIELD(contradiction);
	WRITE_ENUM_FIELD(scopeType,SelScopeType);
	//WRITE_NODE_FIELD(pushdown);
	WRITE_INT_FIELD(topIndex);
	WRITE_VARLENGTHINT_FIELD(baseRelMap, bms_num_members(node->baseRTEs));
}

static void
_outTransProvInfo(StringInfo str, TransProvInfo *node)
{
	WRITE_NODE_TYPE("TRANSPROVINFO");

	WRITE_BOOL_FIELD(isStatic);
	WRITE_NODE_FIELD(root);
	WRITE_INT_FIELD(transProvAttrNum);
	WRITE_INT_FIELD(rtIndex);
}

static void
_outTransSubInfo(StringInfo str, TransSubInfo *node)
{
	WRITE_NODE_TYPE("TRANSSUBINFO");

	WRITE_INT_FIELD(id);
	WRITE_BOOL_FIELD(isStatic);
	WRITE_ENUM_FIELD(opType, SubOperationType);
	WRITE_NODE_FIELD(children);
	WRITE_BITSET_FIELD(setForNode);
	WRITE_INT_FIELD(rtIndex);
	WRITE_NODE_FIELD(annot);
}

static void
_outCopyMap (StringInfo str, CopyMap *node)
{
	WRITE_NODE_TYPE("COPYMAP");

	WRITE_NODE_FIELD(entries);
	WRITE_INT_FIELD(rtindex);
}

//static void
//_outCopyProvInfo(StringInfo str, CopyProvInfo *node)
//{
//	WRITE_NODE_TYPE("COPYPROVINFO");
//
//	WRITE_NODE_FIELD(inMap);
//	WRITE_NODE_FIELD(outMap);
//}



static void
_outCopyMapRelEntry(StringInfo str, CopyMapRelEntry *node)
{
	WRITE_NODE_TYPE("COPYMAPRELENTRY");

	WRITE_OID_FIELD(relation);
	WRITE_INT_FIELD(refNum);
	WRITE_NODE_FIELD(attrEntries);
	WRITE_INT_FIELD(rtindex);
	WRITE_BOOL_FIELD(isStatic);
	WRITE_BOOL_FIELD(noRewrite);
	WRITE_NODE_FIELD(child);
	WRITE_NODE_FIELD(provAttrInfo);
	WRITE_NODE_FIELD(provAttrs);
}

static void
_outCopyMapEntry(StringInfo str, CopyMapEntry *node)
{
	WRITE_NODE_TYPE("COPYMAPENTRY");

	WRITE_NODE_FIELD(baseRelAttr);
	WRITE_STRING_FIELD(provAttrName);
	WRITE_NODE_FIELD(outAttrIncls);
	WRITE_BOOL_FIELD(isStaticTrue);
	WRITE_BOOL_FIELD(isStaticFalse);
}

static void
_outAttrInclusions (StringInfo str, AttrInclusions *node)
{
	WRITE_NODE_TYPE("ATTRINCLUSIONS");

	WRITE_NODE_FIELD(attr);
	WRITE_NODE_FIELD(inclConds);
	WRITE_BOOL_FIELD(isStatic);
}

static void
_outInclusionCond (StringInfo str, InclusionCond *node)
{
	WRITE_NODE_TYPE("INCLUSIONCOND");

	WRITE_ENUM_FIELD(inclType, InclCondType);
	WRITE_NODE_FIELD(existsAttr);
	WRITE_NODE_FIELD(eqVars);
	WRITE_NODE_FIELD(cond);
}

static void
_outCopyProvAttrInfo (StringInfo str, CopyProvAttrInfo *node)
{
	WRITE_NODE_TYPE("COPYPROVATTRINFO");

	WRITE_NODE_FIELD(provVar);
	WRITE_NODE_FIELD(bitSetComposition);
	WRITE_INT_FIELD(outAttrNum);
}

static void
_outXsltFuncExpr (StringInfo str, XsltFuncExpr *node)
{
	WRITE_NODE_TYPE("XSLTFUNCEXPR");

	WRITE_STRING_FIELD(funcName);
	WRITE_NODE_FIELD(param);
}

static void
_outThisExpr (StringInfo str, ThisExpr *node)
{
	WRITE_NODE_TYPE("THISEXPR");

	WRITE_STRING_FIELD(thisType);
}

static void
_outVarLenghtIntArray (StringInfo str, int *array, int numMembers)
{
	int i;

	if(array == NULL || numMembers == 0)
	{
		appendStringInfo(str, "[]");
		return;
	}

	appendStringInfo(str, "[");
	for(i = 0; i < numMembers - 1; i++)
	{
		appendStringInfo(str, "%i ,", array[i]);
	}
	appendStringInfo (str, "%i]", array[numMembers - 1]);
}

/*
 * _outToken
 *	  Convert an ordinary string (eg, an identifier) into a form that
 *	  will be decoded back to a plain token by read.c's functions.
 *
 *	  If a null or empty string is given, it is encoded as "<>".
 */
static void
_outToken(StringInfo str, char *s)
{
	if (s == NULL || *s == '\0')
	{
		appendStringInfo(str, "<>");
		return;
	}

	/*
	 * Look for characters or patterns that are treated specially by read.c
	 * (either in pg_strtok() or in nodeRead()), and therefore need a
	 * protective backslash.
	 */
	/* These characters only need to be quoted at the start of the string */
	if (*s == '<' ||
		*s == '\"' ||
		isdigit((unsigned char) *s) ||
		((*s == '+' || *s == '-') &&
		 (isdigit((unsigned char) s[1]) || s[1] == '.')))
		appendStringInfoChar(str, '\\');
	while (*s)
	{
		/* These chars must be backslashed anywhere in the string */
		if (*s == ' ' || *s == '\n' || *s == '\t' ||
			*s == '(' || *s == ')' || *s == '{' || *s == '}' ||
			*s == '\\')
			appendStringInfoChar(str, '\\');
		appendStringInfoChar(str, *s++);
	}
}

/*
 * READ functions
 */

Node *
parseProvNodeString(char *token, int length)
{
	void *retval;

#define MATCH(tokname, namelen) \
	(length == namelen && strncmp(token, tokname, namelen) == 0)

	if (MATCH("SUBLINKINFO", 11))
		retval = _readSublinkInfo();
	else if (MATCH("CORRVARINFO", 11))
		retval = _readCorrVarInfo();
	else if (MATCH("PROVINFO", 8))
		retval = _readProvInfo();
	else if (MATCH("SELECTIONINFO", 13))
		retval = _readSelectionInfo();
	else if (MATCH("EQUIVALENCELIST", 15))
		retval = _readEquivalenceList();
	else if (MATCH("PUSHDOWNINFO", 12))
		retval = _readPushdownInfo();
	else if (MATCH("INEQUALITYGRAPH", 15))
		retval = _readInequalityGraph();
	else if (MATCH("INEQUALITYGRAPHNODE", 19))
		retval = _readInequalityGraphNode();
//	else if (MATCH("COPYPROVINFO", 12))
//		retval = _readCopyProvInfo();
	else if (MATCH("COPYMAP", 7))
		retval = _readCopyMap();
	else if (MATCH("COPYMAPRELENTRY", 15))
		retval = _readCopyMapRelEntry();
	else if (MATCH("ATTRINCLUSIONS", 14))
		retval = _readAttrInclusions();
	else if (MATCH("INCLUSIONCOND", 13))
		retval = _readInclusionCond();
	else if (MATCH("COPYPROVATTRINFO", 16))
		retval = _readCopyProvAttrInfo();
	else if (MATCH("COPYMAPENTRY", 12))
		retval = _readCopyMapEntry();
	else if (MATCH("TRANSPROVINFO", 13))
		retval = _readTransProvInfo();
	else if (MATCH("TRANSSUBINFO", 12))
		retval = _readTransSubInfo();
	else if (MATCH("XSLTFUNCEXPR", 12))
		retval = _readXsltFuncExpr();
	else if (MATCH("THISEXPR", 8))
		retval = _readThisExpr();
	else
	{
		elog(ERROR, "badly formatted node string \"%.32s\"...", token);
		retval = NULL;	/* keep compiler quiet */
	}

	return retval;
}

static SublinkInfo *
_readSublinkInfo(void)
{
	READ_LOCALS(SublinkInfo);

	READ_INT_FIELD(sublinkPos);
	READ_ENUM_FIELD(location, SublinkLocation);
	READ_ENUM_FIELD(category, SublinkCat);
	READ_ENUM_FIELD(aggLoc, SublinkAggLocation);
	READ_NODE_FIELD(sublink);
	READ_NODE_FIELD(parent);
	READ_NODE_FIELD(exprRoot);
	READ_NODE_FIELD(rootCopy);
	READ_NODE_FIELD(aggOrGroup);
	READ_NODE_FIELD(corrVarInfos);
	READ_NODE_FIELD(condRTEs);
	READ_NODE_FIELD(corrRTEs);
	READ_NODE_FIELD(targetVar);
	READ_INT_FIELD(sublinkRTindex);
	READ_NODE_FIELD(subLinkJoinQuery);
	READ_INT_FIELD(leftJoinPos);
	READ_NODE_FIELD(rewrittenSublinkQuery);

	READ_DONE();
}

static CorrVarInfo *
_readCorrVarInfo(void)
{
	READ_LOCALS(CorrVarInfo);

	READ_NODE_FIELD(corrVar);
	READ_NODE_FIELD(vars);
	READ_NODE_FIELD(parent);
	READ_NODE_FIELD(parentQuery);
	READ_NODE_FIELD(exprRoot);
	READ_BOOL_FIELD(outside);
	READ_NODE_FIELD(refRTE);
	READ_ENUM_FIELD(location, SublinkLocation);
	READ_INT_FIELD(trueVarLevelsUp);
	READ_BOOL_FIELD(belowAgg);
	READ_BOOL_FIELD(belowSet);

	READ_DONE();
}

static ProvInfo *
_readProvInfo(void)
{
	READ_LOCALS(ProvInfo);

	READ_BOOL_FIELD(provSublinkRewritten);
	READ_BOOL_FIELD(shouldRewrite);
	READ_ENUM_FIELD(contribution, ContributionType);
	READ_NODE_FIELD(copyInfo);
	READ_NODE_FIELD(rewriteInfo);
	READ_NODE_FIELD(annotations);

	READ_DONE();
}

static SelectionInfo *
_readSelectionInfo (void)
{
	READ_LOCALS(SelectionInfo);

	READ_NODE_FIELD(expr);
	READ_NODE_FIELD(vars);
//TODO

	READ_DONE();
}

static EquivalenceList *
_readEquivalenceList (void)
{
	READ_LOCALS(EquivalenceList);

	READ_NODE_FIELD(exprs);
	//TODO

	READ_DONE();
}

static PushdownInfo *
_readPushdownInfo (void)
{
	READ_LOCALS(PushdownInfo);

	READ_NODE_FIELD(conjuncts);
	READ_NODE_FIELD(equiLists);
	READ_BOOL_FIELD(contradiction);

	READ_DONE();
}

static InequalityGraph *
_readInequalityGraph(void)
{
	READ_LOCALS(InequalityGraph);

	READ_NODE_FIELD(equiLists);
	READ_NODE_FIELD(nodes);

	READ_DONE();
}

static InequalityGraphNode *
_readInequalityGraphNode(void)
{
	READ_LOCALS(InequalityGraphNode);

	READ_NODE_FIELD(equis);
	READ_NODE_FIELD(lessThen);
	READ_NODE_FIELD(lessEqThen);
	READ_NODE_FIELD(greaterThen);
	READ_NODE_FIELD(greaterEqThen);
	READ_NODE_FIELD(consts);

	READ_DONE();
}

static CopyMap *
_readCopyMap(void)
{
	READ_LOCALS(CopyMap);

	READ_NODE_FIELD(entries);
	READ_INT_FIELD(rtindex);

	READ_DONE();
}

//static CopyProvInfo *
//_readCopyProvInfo(void)
//{
//	READ_LOCALS(CopyProvInfo);
//
//	READ_NODE_FIELD(inMap);
//	READ_NODE_FIELD(outMap);
//
//	READ_DONE();
//}

static CopyMapRelEntry *
_readCopyMapRelEntry(void)
{
	READ_LOCALS(CopyMapRelEntry);

	READ_OID_FIELD(relation);
	READ_INT_FIELD(refNum);
	READ_NODE_FIELD(attrEntries);
	READ_INT_FIELD(rtindex);
	READ_BOOL_FIELD(isStatic);
	READ_BOOL_FIELD(noRewrite);
	// READ_NODE_FIELD(child); do not read child
	READ_NODE_FIELD(provAttrInfo);
	READ_NODE_FIELD(provAttrs);

	READ_DONE();
}

static CopyMapEntry *
_readCopyMapEntry(void)
{
	READ_LOCALS(CopyMapEntry);

	READ_NODE_FIELD(baseRelAttr);
	READ_STRING_FIELD(provAttrName);
	READ_NODE_FIELD(outAttrIncls);
	READ_BOOL_FIELD(isStaticTrue);
	READ_BOOL_FIELD(isStaticFalse);

	READ_DONE();
}

static CopyProvAttrInfo *
_readCopyProvAttrInfo(void)
{
	READ_LOCALS(CopyProvAttrInfo);

	READ_NODE_FIELD(provVar);
	READ_NODE_FIELD(bitSetComposition);
	READ_INT_FIELD(outAttrNum);

	READ_DONE();
}

static AttrInclusions *
_readAttrInclusions(void)
{
	READ_LOCALS(AttrInclusions);

	READ_NODE_FIELD(attr);
	READ_NODE_FIELD(inclConds);
	READ_BOOL_FIELD(isStatic);

	READ_DONE();
}

static InclusionCond *
_readInclusionCond(void)
{
	READ_LOCALS(InclusionCond);

	READ_ENUM_FIELD(inclType, InclCondType);
	READ_NODE_FIELD(existsAttr);
	READ_NODE_FIELD(eqVars);
	READ_NODE_FIELD(cond);

	READ_DONE();
}

static TransProvInfo *
_readTransProvInfo(void)
{
	READ_LOCALS(TransProvInfo);

	READ_BOOL_FIELD(isStatic);
	READ_NODE_FIELD(root);
	READ_INT_FIELD(transProvAttrNum);
	READ_INT_FIELD(rtIndex);

	READ_DONE();
}

static TransSubInfo *
_readTransSubInfo(void)
{
	READ_LOCALS(TransSubInfo);

	READ_INT_FIELD(id);
	READ_BOOL_FIELD(isStatic);
	READ_ENUM_FIELD(opType, SubOperationType);
	READ_NODE_FIELD(children);
	READ_BITSET_FIELD(setForNode);
	READ_INT_FIELD(rtIndex);
	READ_NODE_FIELD(annot);

	READ_DONE();
}

static XsltFuncExpr *
_readXsltFuncExpr(void)
{
	READ_LOCALS(XsltFuncExpr);

	READ_STRING_FIELD(funcName);
	READ_NODE_FIELD(param);

	READ_DONE();
}

static ThisExpr *
_readThisExpr(void)
{
	READ_LOCALS(ThisExpr);

	READ_STRING_FIELD(thisType);

	READ_DONE();
}

/*
 * COPY functions
 */

void *copyProvNode (void *from)
{
	void *retval;

	switch(nodeTag(from))
	{
		case T_SublinkInfo:
			retval = _copySublinkInfo(from);
			break;
		case T_CorrVarInfo:
			retval = _copyCorrVarInfo(from);
			break;
		case T_ProvInfo:
			retval = _copyProvInfo(from);
			break;
		case T_SelectionInfo:
			retval = _copySelectionInfo(from);
			break;
		case T_EquivalenceList:
			retval = _copyEquivalenceList(from);
			break;
		case T_PushdownInfo:
			retval = _copyPushdownInfo(from);
			break;
		case T_InequalityGraph:
			retval = _copyInequalityGraph(from);
			break;
		case T_InequalityGraphNode:
			retval = _copyInequalityGraphNode(from);
			break;
		case T_QueryPushdownInfo:
			retval = _copyQueryPushdownInfo(from);
			break;
		case T_SelScope:
			retval = _copySelScope(from);
			break;
		case T_CopyMap:
			retval = _copyCopyMap(from);
			break;
//		case T_CopyProvInfo:
//			retval = _copyCopyProvInfo(from);
//			break;
		case T_CopyMapRelEntry:
			retval = _copyCopyMapRelEntry(from);
			break;
		case T_CopyMapEntry:
			retval = _copyCopyMapEntry(from);
			break;
		case T_AttrInclusions:
			retval = _copyAttrInclusions(from);
			break;
		case T_InclusionCond:
			retval = _copyInclusionCond(from);
			break;
		case T_CopyProvAttrInfo:
			retval = _copyCopyProvAttrInfo(from);
			break;
		case T_TransProvInfo:
			retval = _copyTransProvInfo(from);
			break;
		case T_TransSubInfo:
			retval = _copyTransSubInfo(from);
			break;
		case T_XsltFuncExpr:
			retval = _copyXsltFuncExpr(from);
			break;
		case T_ThisExpr:
			retval = _copyThisExpr(from);
			break;
		default:
			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(from));
			retval = from;		/* keep compiler quiet */
			break;
	}

	return retval;
}

static SublinkInfo *
_copySublinkInfo(SublinkInfo *from)
{

	SublinkInfo *newnode = makeNode(SublinkInfo);

	COPY_SCALAR_FIELD(location);
	COPY_SCALAR_FIELD(sublinkPos);
	COPY_SCALAR_FIELD(category);
	COPY_SCALAR_FIELD(aggLoc);
	COPY_NODE_FIELD(sublink);
	COPY_NODE_FIELD(parent);
	newnode->grandParentExprPointer = from->grandParentExprPointer;
	COPY_NODE_FIELD(exprRoot);
	COPY_NODE_FIELD(rootCopy);
	newnode->subLinkExprPointer = from->subLinkExprPointer;
	COPY_NODE_FIELD(aggOrGroup);
	newnode->aggOrGroupPointer = from->aggOrGroupPointer;
	COPY_NODE_FIELD(corrVarInfos);
	COPY_NODE_FIELD(condRTEs);
	COPY_NODE_FIELD(corrRTEs);
	COPY_NODE_FIELD(targetVar);
	COPY_SCALAR_FIELD(sublinkRTindex);
	COPY_NODE_FIELD(subLinkJoinQuery);
	COPY_SCALAR_FIELD(leftJoinPos);
	COPY_NODE_FIELD(rewrittenSublinkQuery);

	return newnode;
}

static CorrVarInfo *
_copyCorrVarInfo(CorrVarInfo *from)
{

	CorrVarInfo *newnode = makeNode(CorrVarInfo);

	COPY_NODE_FIELD(corrVar);
	COPY_NODE_FIELD(vars);
	COPY_NODE_FIELD(parent);
	COPY_NODE_FIELD(parentQuery);
	COPY_NODE_FIELD(exprRoot);
	COPY_SCALAR_FIELD(outside);
	COPY_NODE_FIELD(refRTE);
	COPY_SCALAR_FIELD(location);
	COPY_SCALAR_FIELD(trueVarLevelsUp);
	COPY_SCALAR_FIELD(belowAgg);
	COPY_SCALAR_FIELD(belowSet);

	return newnode;
}

static ProvInfo *
_copyProvInfo(ProvInfo *from)
{

	ProvInfo *newnode = makeNode(ProvInfo);

	COPY_SCALAR_FIELD(provSublinkRewritten);
	COPY_SCALAR_FIELD(shouldRewrite);
	COPY_SCALAR_FIELD(contribution);
	COPY_NODE_FIELD(copyInfo);
	COPY_NODE_FIELD(rewriteInfo);
	COPY_NODE_FIELD(annotations);

	return newnode;
}

static SelectionInfo *
_copySelectionInfo(SelectionInfo *from)
{
	SelectionInfo *newnode = makeNode(SelectionInfo);

	COPY_NODE_FIELD(expr);
	//eqExpr;
	COPY_NODE_FIELD(vars);
	COPY_SCALAR_FIELD(notMovable);
	COPY_SCALAR_FIELD(derived);
	COPY_SCALAR_FIELD(rtOrigin);

	return newnode;
}

static EquivalenceList *
_copyEquivalenceList(EquivalenceList *from)
{
	EquivalenceList *newnode = makeNode(EquivalenceList);

	COPY_NODE_FIELD(exprs);
	//COPY_NODE_FIELD(eqExpr);
	COPY_NODE_FIELD(genSels);
	COPY_NODE_FIELD(exprs);
	COPY_NODE_FIELD(derivedSels);
	COPY_NODE_FIELD(constant);
	newnode->scope = from->scope;

	return newnode;
}

static PushdownInfo *
_copyPushdownInfo(PushdownInfo *from)
{
	PushdownInfo *newnode = makeNode(PushdownInfo);

	COPY_NODE_FIELD(conjuncts);
	COPY_NODE_FIELD(equiLists);
	COPY_SCALAR_FIELD(contradiction);

	return newnode;
}

static InequalityGraph *
_copyInequalityGraph(InequalityGraph *from)
{
	InequalityGraph *newnode = makeNode(InequalityGraph);

	COPY_NODE_FIELD(equiLists);
	COPY_NODE_FIELD(nodes);

	return newnode;
}

static InequalityGraphNode *
_copyInequalityGraphNode(InequalityGraphNode *from)
{
	InequalityGraphNode *newnode = makeNode(InequalityGraphNode);

	COPY_NODE_FIELD(equis);
	COPY_NODE_FIELD(lessThen);
	COPY_NODE_FIELD(lessEqThen);
	COPY_NODE_FIELD(greaterThen);
	COPY_NODE_FIELD(greaterEqThen);
	COPY_NODE_FIELD(consts);

	return newnode;
}

static QueryPushdownInfo *
_copyQueryPushdownInfo(QueryPushdownInfo *from)
{
	QueryPushdownInfo *newnode = makeNode(QueryPushdownInfo);

	//COPY_NODE_FIELD(query); cannot do this might cause cycles. Anyway normally we want the copy to point at exactly the same object
	newnode->query = from->query;
	//COPY_NODE_FIELD(scopes);
	COPY_NODE_FIELD(topScope);
	//COPY_NODE_FIELD(qualPointers);
	//COPY_NODE_FIELD(parent);
	COPY_NODE_FIELD(children);
	COPY_NODE_FIELD(validityScopes);

	/* repair cyclular links that cannot be copied by the normal copy machinery */
	_repairCyclesQueryPushdownInfo(newnode, from);

	return newnode;
}

static void
_repairCyclesQueryPushdownInfo(QueryPushdownInfo *newnode, QueryPushdownInfo *from)
{
	ListCell *lc;
	Node *qualPointer;
	QueryPushdownInfo *child;

	/* first recreate scopes list by recursing into the scope tree starting with top level scope */
	_repairCyclesQueryPushdownInfoCreateScopesList(newnode->topScope, newnode);

	/* flat copy qual pointers */
	newnode->qualPointers = NIL;

	foreach(lc, from->qualPointers)
	{
		qualPointer = (Node *) lfirst(lc);
		newnode->qualPointers = lappend(newnode->qualPointers, qualPointer);
	}

	/* set parent pointers of children */
	foreach(lc, newnode->children)
	{
		child = (QueryPushdownInfo *) lfirst(lc);
		child->parent = newnode;
	}
}

static void
_repairCyclesQueryPushdownInfoCreateScopesList (SelScope *sel, QueryPushdownInfo *info)
{
	ListCell *lc;
	SelScope *curScope;

	info->scopes = lappend(info->scopes, sel);
	sel->pushdown = info;

	foreach(lc, sel->children)
	{
		curScope = (SelScope *) lfirst(lc);
		_repairCyclesQueryPushdownInfoCreateScopesList(curScope, info);
	}
}

static SelScope *
_copySelScope(SelScope *from)
{
	SelScope *newnode = makeNode(SelScope);

	COPY_BITMAPSET_FIELD(baseRTEs);
	COPY_BITMAPSET_FIELD(joinRTEs);
	COPY_NODE_FIELD(equiLists);
	COPY_BITMAPSET_FIELD(childRTEs);
	COPY_NODE_FIELD(children);
	COPY_SCALAR_FIELD(contradiction);
	COPY_SCALAR_FIELD(scopeType);
	COPY_SCALAR_FIELD(topIndex);
	newnode->baseRelMap = _copyVarLengthIntArray(from->baseRelMap, bms_num_members(from->baseRTEs));

	_repairCyclesSelScope(newnode, from);

	return newnode;
}

static CopyMap *
_copyCopyMap(CopyMap *from)
{
	CopyMap *newnode = makeNode(CopyMap);

	COPY_NODE_FIELD(entries);
	COPY_SCALAR_FIELD(rtindex);

	return newnode;
}

//static CopyProvInfo *
//_copyCopyProvInfo(CopyProvInfo *from)
//{
//	CopyProvInfo *newnode = makeNode(CopyProvInfo);
//
//	COPY_NODE_FIELD(inMap);
//	COPY_NODE_FIELD(outMap);
//
//	return newnode;
//}

static CopyMapRelEntry *
_copyCopyMapRelEntry(CopyMapRelEntry *from)
{
	CopyMapRelEntry *newnode = makeNode(CopyMapRelEntry);

	COPY_SCALAR_FIELD(relation);
	COPY_SCALAR_FIELD(refNum);
	COPY_NODE_FIELD(attrEntries);
	COPY_SCALAR_FIELD(rtindex);
	COPY_SCALAR_FIELD(isStatic);
	COPY_SCALAR_FIELD(noRewrite);
	COPY_NODE_FIELD(provAttrInfo);
	COPY_NODE_FIELD(provAttrs);
	// do not copy child TODO reconstruction
	newnode->child = NULL;

	return newnode;
}

static CopyMapEntry *
_copyCopyMapEntry(CopyMapEntry *from)
{
	CopyMapEntry *newnode = makeNode(CopyMapEntry);

	COPY_NODE_FIELD(baseRelAttr);
	COPY_STRING_FIELD(provAttrName);
	COPY_NODE_FIELD(outAttrIncls);
	COPY_SCALAR_FIELD(isStaticTrue);
	COPY_SCALAR_FIELD(isStaticFalse);

	return newnode;
}

static CopyProvAttrInfo *
_copyCopyProvAttrInfo(CopyProvAttrInfo *from)
{
	CopyProvAttrInfo *newnode = makeNode(CopyProvAttrInfo);

	COPY_NODE_FIELD(provVar);
	COPY_NODE_FIELD(bitSetComposition);
	COPY_SCALAR_FIELD(outAttrNum);

	return newnode;
}

static AttrInclusions *
_copyAttrInclusions(AttrInclusions *from)
{
	AttrInclusions *newnode = makeNode(AttrInclusions);

	COPY_NODE_FIELD(attr);
	COPY_NODE_FIELD(inclConds);
	COPY_SCALAR_FIELD(isStatic);

	return newnode;
}

static InclusionCond *
_copyInclusionCond(InclusionCond *from)
{
	InclusionCond *newnode = makeNode(InclusionCond);

	COPY_SCALAR_FIELD(inclType);
	COPY_NODE_FIELD(existsAttr);
	COPY_NODE_FIELD(eqVars);
	COPY_NODE_FIELD(cond);

	return newnode;
}

static TransProvInfo *
_copyTransProvInfo(TransProvInfo *from)
{
	TransProvInfo *newnode = makeNode(TransProvInfo);

	COPY_SCALAR_FIELD(isStatic);
	COPY_NODE_FIELD(root);
	COPY_SCALAR_FIELD(transProvAttrNum);
	COPY_SCALAR_FIELD(rtIndex);

	return newnode;
}

static TransSubInfo *
_copyTransSubInfo(TransSubInfo *from)
{
	TransSubInfo *newnode = makeNode(TransSubInfo);

	COPY_SCALAR_FIELD(id);
	COPY_SCALAR_FIELD(isStatic);
	COPY_SCALAR_FIELD(opType);
	COPY_NODE_FIELD(children);
	COPY_BITSET_FIELD(setForNode);
	COPY_SCALAR_FIELD(rtIndex);
	COPY_NODE_FIELD(annot);

	return newnode;
}

static XsltFuncExpr *
_copyXsltFuncExpr(XsltFuncExpr *from)
{
	XsltFuncExpr *newnode = makeNode(XsltFuncExpr);

	COPY_STRING_FIELD(funcName);
	COPY_NODE_FIELD(param);

	return newnode;
}

static ThisExpr *
_copyThisExpr(ThisExpr *from)
{
	ThisExpr *newnode = makeNode(ThisExpr);

	COPY_STRING_FIELD(thisType);

	return newnode;
}

static int *
_copyVarLengthIntArray (int *array, int numMembers)
{
	int *result;
	int i;

	if (array == NULL)
		return NULL;

	result = (int *) palloc(numMembers * sizeof(int));

	for(i = 0; i < numMembers; i++)
	{
		result[i] = array[i];
	}

	return result;
}

static void
_repairCyclesSelScope(SelScope *newnode, SelScope *from)
{
	ListCell *lc;
	SelScope *child;
	EquivalenceList *equi;

	/* set parent */
	newnode->pushdown = from->pushdown;

	/* recreate selinfo list */
	newnode->selInfos = NIL;

	foreach(lc, newnode->equiLists)
	{
		equi = lfirst(lc);
		_repairCyclesSelScopeCreateSelInfoList(equi, newnode);
	}

	/* set child parents */
	foreach(lc, newnode->children)
	{
		child = (SelScope *) lfirst(lc);
		child->parent = newnode;
	}
}

static void
_repairCyclesSelScopeCreateSelInfoList(EquivalenceList *eq, SelScope *scope)
{
	ListCell *lc;
	SelectionInfo *sel;

	eq->scope = scope;

	foreach(lc, eq->genSels)
	{
		sel = (SelectionInfo *) lfirst(lc);
		scope->selInfos = lappend(scope->selInfos, sel);
	}

	foreach(lc, eq->derivedSels)
	{
		sel = (SelectionInfo *) lfirst(lc);
		scope->selInfos = lappend(scope->selInfos, sel);
	}
}



