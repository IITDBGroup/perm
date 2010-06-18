/*-------------------------------------------------------------------------
 *
 * xmlqtree.c
 *	  POSTGRES C - Convert a query tree into an XML-string or XML datatype representation. We use two XML representations:
 *	  		1) a verbose representation that is similar to the string representation produced by outfuncs.c.
 *	  		2) a more condensed human readable representation.
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: /postgres/src/backend/metaq/xmlqtree.c,v 1.542 15.07.2009 11:13:59 bglav Exp $
 *
 * NOTES
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "utils/xml.h"
#include "utils/lsyscache.h"
#include "parser/parse_type.h"
#include "nodes/parsenodes.h"
#include "nodes/makefuncs.h"
#include "tcop/tcopprot.h"
#include "fmgr.h"


#include "metaq/xmlqtree.h"
#include "metaq/outxmlfuncs.h"
#include "provrewrite/prov_trans_parse_back_xml.h"

/* functions */
static xmltype *stringinfo_to_xmltype(StringInfo buf);
static xmltype *queryToXmlWorker (char *queryText, bool simple);

/*
 * Converts a query node into its xml representation (text).
 */

StringInfo
queryTreeToXmlText (Query *query) {
	return nodeToXmlStringInfo((Node *) query);
}


/*
 * Converts a query node into its xml representation and returns it as a xml data type value.
 */

xmltype *
queryTreeToXml (Query *query)
{
	StringInfo str;

	str = queryTreeToXmlText (query);

	return stringinfo_to_xmltype(str);
}



/*
 * Converts a query provided as SQL into an XML representation.
 */

Datum
query_sql_to_xml (PG_FUNCTION_ARGS)
{
	text *data = PG_GETARG_TEXT_PP(0);
	char *queryText;
	int len;

	len = VARSIZE_ANY_EXHDR(data);

	queryText = (char *) palloc(len + 1);
	memcpy (queryText, VARDATA_ANY(data), len);
	queryText[len] = '\0';

	PG_RETURN_XML_P(queryToXmlWorker (queryText, false));
}

/*
 *
 */

Datum
query_sql_to_simple_xml (PG_FUNCTION_ARGS)
{
	text *data = PG_GETARG_TEXT_PP(0);
	char *queryText;
	int len;

	len = VARSIZE_ANY_EXHDR(data);

	queryText = (char *) palloc(len + 1);
	memcpy (queryText, VARDATA_ANY(data), len);
	queryText[len] = '\0';

	PG_RETURN_XML_P(queryToXmlWorker (queryText, true));
}

/*
 *
 */

static xmltype *
queryToXmlWorker (char *queryText, bool simple)
{
	Query *query;
	xmltype *xml = NULL;
	List *parseTrees;
	List *queries;
	ListCell *lc;

	/* parse and analyze */
	parseTrees = pg_parse_query(queryText);

	/* TODO error if more than one statement */
	queries = pg_analyze_and_rewrite((Node *) linitial(parseTrees), queryText, NULL, 0);

	/* create the xml representation for each generated query */
	foreach(lc, queries)
	{
		query = (Query *) lfirst(lc);

		if (simple)
		{
			if (xml)
				xml = xmlconcat(list_make2(xml, queryTreeToSimpleXml(query)));
			else
				xml = queryTreeToSimpleXml(query);
		}
		else
		{
			if (xml)
				xml = xmlconcat(list_make2(xml, queryTreeToXml(query)));
			else
				xml = queryTreeToXml(query);
		}
	}

	return xml;
}

/*
 *
 */

Const *
queryTreeToXmlConst (Query *query)
{
	Const *xmlConst;
	xmltype *xml;
	int16		typLen;
	bool		typByVal;
	Oid consttype;
	int32 consttypmod;

	parseTypeString("xml",&consttype, &consttypmod);

	get_typlenbyval(consttype, &typLen, &typByVal);
	xml = queryTreeToXml(query);
	xmlConst = makeConst(consttype, consttypmod, XmlPGetDatum(xml), typLen, false, typByVal);

	return xmlConst;
}

/*
 *
 */
StringInfo
queryTreeToSimpleXmlText (Query *query)
{
	return parseBackTransToXML(query, NULL, true, CurrentMemoryContext);
}

/*
 *
 */

xmltype *
queryTreeToSimpleXml (Query *query)
{
	StringInfo str;

	str = queryTreeToSimpleXmlText (query);

	return stringinfo_to_xmltype(str);
}

/*
 *
 */

Const *
queryTreeToSimpleXmlConst (Query *query)
{
	Const *xmlConst;
	xmltype *xml;
	int16		typLen;
	bool		typByVal;
	Oid consttype;
	int32 consttypmod;

	parseTypeString("xml",&consttype, &consttypmod);

	get_typlenbyval(consttype, &typLen, &typByVal);
	xml = queryTreeToSimpleXml(query);
	xmlConst = makeConst(consttype, consttypmod, XmlPGetDatum(xml), typLen, false, typByVal);

	return xmlConst;
}


/*
 * Converts a StringInfo into an xmldata type
 */

static xmltype *
stringinfo_to_xmltype(StringInfo buf)
{
	int32		len;
	xmltype    *result;

	len = buf->len + VARHDRSZ;
	result = palloc(len);
	SET_VARSIZE(result, len);
	memcpy(VARDATA(result), buf->data, buf->len);

	return result;
}



