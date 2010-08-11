/*-------------------------------------------------------------------------
 *
 * provattrname.c
 *	  POSTGRES C provenance attribute name generator
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/provattrname.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"					// needed for all stuff

#include "provrewrite/provattrname.h"
#include "utils/lsyscache.h"

/* prefix for provenance attributes */
const char ProvPraefix[] = "prov_";
const char TransProvName[] = "trans_prov";

/* list of counters for references to relations */
static List *relRefCount;
static List *queryRefCount;

/*
 *
 */
static char *escapeAttrName (char *name);


/*
 * Creates a provenance attribute name by taking the
 * constant ProvPraefix + relation name + attribute name
 */
char *
createProvAttrName (RangeTblEntry *rte, char *name)
{
	char *attrName;
	char *provRelName;
	char *escapedName;
	int newLength;

	if (rte->rtekind == RTE_RELATION)
		provRelName = getRelationName (rte->relid);
	else if (rte->rtekind == RTE_SUBQUERY)
		provRelName = getQueryName (rte);
	else
	{
		//TODO error
	}

	escapedName = escapeAttrName (name);

	newLength = strlen(ProvPraefix) + strlen(provRelName) + strlen(escapedName) + 2;
	attrName = (char *) palloc (newLength);

	attrName = strcpy(attrName, ProvPraefix);
	attrName = strcat(attrName, provRelName);
	attrName = strcat(attrName, "_");
	attrName = strcat(attrName, escapedName);

	pfree(provRelName);
	pfree(escapedName);

	return attrName;
}

/*
 * Escape the underscores in an attribute name (by '_' -> '__')
 */

static char *
escapeAttrName (char *name)
{
	char *result;
	char *pos, *resPos;
	int newlength;

	newlength = strlen(name) + 1;

	/* count underscores */
	pos = name;
	for(pos = name; pos != NULL && *pos != '\0'; pos++)
	{
		if (*pos == '_')
			newlength++;
	}

	/* copy with escape */
	result = (char *) palloc (newlength);

	for(pos = name, resPos = result; *pos != '\0'; pos++, resPos++)
	{
		if ((*pos) == '_')
		{
			*resPos = '_';
			*(++resPos) = '_';
		}
		else
			*resPos = *pos;
	}
	*resPos = '\0';

	return result;
}

/*
 * Returns the name of a base relation. For multiple references to a relation
 * append an integer identifying the reference.
 */

char *
getRelationName (Oid relid)
{
	char *schema;
	char *relname;
	char *result;
	char *numberStr = NULL;
	Oid namespOid;
	int newLength;
	int numRelRef;

	namespOid = get_rel_namespace(relid);
	schema = get_namespace_name (namespOid);
	relname = get_rel_name(relid);
	numRelRef = getRelationRefNum(relid, false);

	newLength = strlen(schema) + strlen(relname)  + 2;

	if (numRelRef > 0)
	{
		numberStr = palloc((sizeof(int)) * 8 + 1);
		sprintf(numberStr, "%i",numRelRef);
		newLength += strlen(numberStr) + 1;
	}

	result = (char *) palloc(newLength);

	result = strcpy(result, schema);
	result = strcat(result, "_");
	result = strcat(result, relname);

	if (numRelRef > 0)
	{
		result = strcat(result, "_");
		result = strcat(result, numberStr);
	}

	pfree(schema);
	pfree(relname);

	return result;
}

/*
 * Get a relation name without appended unique number.
 */

char *
getRelationNameUnqualified (Oid relid)
{
	char *schema;
	char *relname;
	char *result;
	Oid namespOid;
	int newLength;

	namespOid = get_rel_namespace(relid);
	schema = get_namespace_name (namespOid);
	relname = get_rel_name(relid);

	newLength = strlen(schema) + strlen(relname)  + 2;

	result = (char *) palloc(newLength);

	result = strcpy(result, schema);
	result = strcat(result, ".");
	result = strcat(result, relname);

	pfree(schema);
	pfree(relname);

	return result;
}

/*
 * Returns the name of a base relation or subquery marked with the BASERELATION
 * keyword. For multiple references to a relation append an integer identifying
 * the reference.
 */

char *
getQueryName (RangeTblEntry *rte)
{
	char *result;
	char *numberStr = NULL;
	int refCount;
	int newLength;

	refCount = getQueryRefNum(rte, false);

	newLength = strlen(rte->alias->aliasname) + 2;

	if (refCount > 0)
	{
		numberStr = palloc((sizeof(int)) * 8 + 1);
		sprintf(numberStr, "%i",refCount);
		newLength += strlen(numberStr) + 1;
	}

	result = (char *) palloc(newLength);

	result = strcpy(result, rte->alias->aliasname);

	if (refCount > 0)
	{
		result = strcat(result, "_");
		result = strcat(result, numberStr);
	}

	return result;
}

/*
 * Returns the number of references to a relation. This is done using a counter
 * for each relation (OID) that is incremented each time this method is called
 * with the OID of the relation.
 */

int
getRelationRefNum (Oid relid, bool increment)
{
	ListCell *lc;
	OidRef *oidRef;

	/* walk through list of OidRef structs. If a entry with oid = relid is
	 * found increment ref count and return the incremented ref count value.
	 */

	foreach(lc, relRefCount)
	{
		oidRef = (OidRef *) lfirst(lc);
		if (oidRef->relid == relid)
		{
			if (increment)
				oidRef->refCounter++;

			return oidRef->refCounter;
		}
	}

	/* first reference to relation add OidRef for this relation to
	 * relRefCount */
	oidRef = (OidRef *) palloc(sizeof(OidRef));
	oidRef->relid = relid;
	oidRef->refCounter = 0;

	relRefCount = lappend(relRefCount, oidRef);

	return 0;
}

/*
 *	Returns the count of references to a range table entry that is either a
 *	base relation or is a subquery that is marked with the BASERELATION
 *	keyword.
 */

int
getQueryRefNum (RangeTblEntry *rte, bool increment)
{
	ListCell *lc;
	RTEref *rteRef;
	char *name;

	if (rte->rtekind == RTE_RELATION && !rte->isProvBase)
		return getRelationRefNum (rte->relid, increment);

	/* if RTE has an alias use this as name, if not check for eref and finally
	 * fall back to view name if RTE is a view */
	if (!rte->alias || !rte->alias->aliasname)
	{
		if (rte->eref && rte->eref->aliasname)
		{
			name = rte->eref->aliasname;
		}
		else
		{
			Assert (rte->relid != InvalidOid);
			name = get_rel_name(rte->relid);
		}

		rte->alias = makeNode(Alias);
		rte->alias->aliasname = pstrdup(name);
	}

	/* try to find reference counter for RTE */
	foreach(lc, queryRefCount)
	{
		rteRef =  (RTEref *) lfirst(lc);
		if (strcmp(rte->alias->aliasname, rteRef->name) == 0)
		{
			if (increment)
				rteRef->refCounter++;

			return rteRef->refCounter;
		}
	}

	/* first reference to this query add a new SubqueryRef to list */
	rteRef = (RTEref *) palloc(sizeof(RTEref));
	rteRef->name = rte->alias->aliasname;
	rteRef->refCounter = 0;

	queryRefCount = lappend(queryRefCount, rteRef);

	return rteRef->refCounter;
}

/*
 * Reset the relation reference counters.
 */

void
resetRelReferences (void)
{
	relRefCount = NIL;
	queryRefCount = NIL;
}

/*
 * Returns true, if te is a provnenace attribute.
 */

bool
isProvAttr (TargetEntry *te)
{
	if (te->resname == NULL || strlen(te->resname) < 5)
		return false;

	if (!strncmp(te->resname, ProvPraefix, 5))
		return true;

	if (strlen(te->resname) < 10)
		return false;

	return (!strncmp(te->resname, TransProvName, 10));
}

/*
 * Creates a provenance attribute name for a subquery that for which the user
 * has specified a list of provenance attributes. This is done by adding the
 * prefix provenance and escaping underscores in the attribute name.
 */

char *
createExternalProvAttrName (char *name)
{
	char *escapedName;
	char *result;
	int length;

	escapedName = escapeAttrName(name);

	length = strlen(ProvPraefix) + strlen(escapedName) + 1;

	result = (char *) palloc (length);

	result = strcpy(result, ProvPraefix);
	result = strcat(result, escapedName);

	return result;
}

