/*
 * psql - the PostgreSQL interactive terminal
 *
 * Copyright (c) 2000-2008, PostgreSQL Global Development Group
 *
 * $PostgreSQL: pgsql/src/bin/psql/describe.c,v 1.164 2008/01/01 19:45:56 momjian Exp $
 */
#include "postgres_fe.h"
#include "describe.h"

#include "dumputils.h"

#include "common.h"
#include "settings.h"
#include "print.h"
#include "variables.h"

#include <ctype.h>

#ifdef WIN32
/*
 * mbvalidate() is used in function describeOneTableDetails() to make sure
 * all characters of the cells will be printed to the DOS console in a
 * correct way
 */
#include "mbprint.h"
#endif


static bool describeOneTableDetails(const char *schemaname,
						const char *relationname,
						const char *oid,
						bool verbose);
static bool add_tablespace_footer(char relkind, Oid tablespace, char **footers,
					  int *count, PQExpBufferData buf, bool newline);
static bool listTSParsersVerbose(const char *pattern);
static bool describeOneTSParser(const char *oid, const char *nspname,
					const char *prsname);
static bool listTSConfigsVerbose(const char *pattern);
static bool describeOneTSConfig(const char *oid, const char *nspname,
					const char *cfgname,
					const char *pnspname, const char *prsname);


/*----------------
 * Handlers for various slash commands displaying some sort of list
 * of things in the database.
 *
 * If you add something here, try to format the query to look nice in -E output.
 *----------------
 */


/* \da
 * Takes an optional regexp to select particular aggregates
 */
bool
describeAggregates(const char *pattern, bool verbose)
{
	PQExpBufferData buf;
	PGresult   *res;
	printQueryOpt myopt = pset.popt;

	initPQExpBuffer(&buf);

	/*
	 * There are two kinds of aggregates: ones that work on particular types
	 * and ones that work on all (denoted by input type = "any")
	 */
	printfPQExpBuffer(&buf,
					  "SELECT n.nspname as \"%s\",\n"
					  "  p.proname AS \"%s\",\n"
				  "  pg_catalog.format_type(p.prorettype, NULL) AS \"%s\",\n"
					  "  CASE WHEN p.pronargs = 0\n"
					  "    THEN CAST('*' AS pg_catalog.text)\n"
					  "    ELSE\n"
					  "    pg_catalog.array_to_string(ARRAY(\n"
					  "      SELECT\n"
				 "        pg_catalog.format_type(p.proargtypes[s.i], NULL)\n"
					  "      FROM\n"
					  "        pg_catalog.generate_series(0, pg_catalog.array_upper(p.proargtypes, 1)) AS s(i)\n"
					  "    ), ', ')\n"
					  "  END AS \"%s\",\n"
				 "  pg_catalog.obj_description(p.oid, 'pg_proc') as \"%s\"\n"
					  "FROM pg_catalog.pg_proc p\n"
	   "     LEFT JOIN pg_catalog.pg_namespace n ON n.oid = p.pronamespace\n"
					  "WHERE p.proisagg\n",
					  gettext_noop("Schema"),
					  gettext_noop("Name"),
					  gettext_noop("Result data type"),
					  gettext_noop("Argument data types"),
					  gettext_noop("Description"));

	processSQLNamePattern(pset.db, &buf, pattern, true, false,
						  "n.nspname", "p.proname", NULL,
						  "pg_catalog.pg_function_is_visible(p.oid)");

	appendPQExpBuffer(&buf, "ORDER BY 1, 2, 3;");

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	myopt.nullPrint = NULL;
	myopt.title = _("List of aggregate functions");
	myopt.trans_headers = true;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	PQclear(res);
	return true;
}

/* \db
 * Takes an optional regexp to select particular tablespaces
 */
bool
describeTablespaces(const char *pattern, bool verbose)
{
	PQExpBufferData buf;
	PGresult   *res;
	printQueryOpt myopt = pset.popt;

	if (pset.sversion < 80000)
	{
		fprintf(stderr, _("The server version (%d) does not support tablespaces.\n"),
				pset.sversion);
		return true;
	}

	initPQExpBuffer(&buf);

	printfPQExpBuffer(&buf,
					  "SELECT spcname AS \"%s\",\n"
					  "  pg_catalog.pg_get_userbyid(spcowner) AS \"%s\",\n"
					  "  spclocation AS \"%s\"",
					  gettext_noop("Name"),
					  gettext_noop("Owner"),
					  gettext_noop("Location"));

	if (verbose)
		appendPQExpBuffer(&buf,
						  ",\n  spcacl AS \"%s\""
		 ",\n  pg_catalog.shobj_description(oid, 'pg_tablespace') AS \"%s\"",
						  gettext_noop("Access privileges"),
						  gettext_noop("Description"));

	appendPQExpBuffer(&buf,
					  "\nFROM pg_catalog.pg_tablespace\n");

	processSQLNamePattern(pset.db, &buf, pattern, false, false,
						  NULL, "spcname", NULL,
						  NULL);

	appendPQExpBuffer(&buf, "ORDER BY 1;");

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	myopt.nullPrint = NULL;
	myopt.title = _("List of tablespaces");
	myopt.trans_headers = true;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	PQclear(res);
	return true;
}


/* \df
 * Takes an optional regexp to select particular functions
 */
bool
describeFunctions(const char *pattern, bool verbose)
{
	PQExpBufferData buf;
	PGresult   *res;
	printQueryOpt myopt = pset.popt;

	initPQExpBuffer(&buf);

	printfPQExpBuffer(&buf,
					  "SELECT n.nspname as \"%s\",\n"
					  "  p.proname as \"%s\",\n"
					  "  CASE WHEN p.proretset THEN 'setof ' ELSE '' END ||\n"
				  "  pg_catalog.format_type(p.prorettype, NULL) as \"%s\",\n"
					  "  CASE WHEN proallargtypes IS NOT NULL THEN\n"
					  "    pg_catalog.array_to_string(ARRAY(\n"
					  "      SELECT\n"
					  "        CASE\n"
					  "          WHEN p.proargmodes[s.i] = 'i' THEN ''\n"
					  "          WHEN p.proargmodes[s.i] = 'o' THEN 'OUT '\n"
					"          WHEN p.proargmodes[s.i] = 'b' THEN 'INOUT '\n"
					  "        END ||\n"
					  "        CASE\n"
			 "          WHEN COALESCE(p.proargnames[s.i], '') = '' THEN ''\n"
					  "          ELSE p.proargnames[s.i] || ' ' \n"
					  "        END ||\n"
			  "        pg_catalog.format_type(p.proallargtypes[s.i], NULL)\n"
					  "      FROM\n"
					  "        pg_catalog.generate_series(1, pg_catalog.array_upper(p.proallargtypes, 1)) AS s(i)\n"
					  "    ), ', ')\n"
					  "  ELSE\n"
					  "    pg_catalog.array_to_string(ARRAY(\n"
					  "      SELECT\n"
					  "        CASE\n"
		   "          WHEN COALESCE(p.proargnames[s.i+1], '') = '' THEN ''\n"
					  "          ELSE p.proargnames[s.i+1] || ' '\n"
					  "          END ||\n"
				 "        pg_catalog.format_type(p.proargtypes[s.i], NULL)\n"
					  "      FROM\n"
					  "        pg_catalog.generate_series(0, pg_catalog.array_upper(p.proargtypes, 1)) AS s(i)\n"
					  "    ), ', ')\n"
					  "  END AS \"%s\"",
					  gettext_noop("Schema"),
					  gettext_noop("Name"),
					  gettext_noop("Result data type"),
					  gettext_noop("Argument data types"));

	if (verbose)
		appendPQExpBuffer(&buf,
						  ",\n CASE\n"
						  "  WHEN p.provolatile = 'i' THEN 'immutable'\n"
						  "  WHEN p.provolatile = 's' THEN 'stable'\n"
						  "  WHEN p.provolatile = 'v' THEN 'volatile'\n"
						  "END as \"%s\""
						  ",\n  r.rolname as \"%s\",\n"
						  "  l.lanname as \"%s\",\n"
						  "  p.prosrc as \"%s\",\n"
				  "  pg_catalog.obj_description(p.oid, 'pg_proc') as \"%s\"",
						  gettext_noop("Volatility"),
						  gettext_noop("Owner"),
						  gettext_noop("Language"),
						  gettext_noop("Source code"),
						  gettext_noop("Description"));

	if (!verbose)
		appendPQExpBuffer(&buf,
						  "\nFROM pg_catalog.pg_proc p"
						  "\n     LEFT JOIN pg_catalog.pg_namespace n ON n.oid = p.pronamespace\n");
	else
		appendPQExpBuffer(&buf,
						  "\nFROM pg_catalog.pg_proc p"
		"\n     LEFT JOIN pg_catalog.pg_namespace n ON n.oid = p.pronamespace"
			 "\n     LEFT JOIN pg_catalog.pg_language l ON l.oid = p.prolang"
				"\n     JOIN pg_catalog.pg_roles r ON r.oid = p.proowner\n");

	/*
	 * we skip in/out funcs by excluding functions that take or return cstring
	 */
	appendPQExpBuffer(&buf,
		   "WHERE p.prorettype <> 'pg_catalog.cstring'::pg_catalog.regtype\n"
					  "      AND (p.proargtypes[0] IS NULL\n"
					  "      OR   p.proargtypes[0] <> 'pg_catalog.cstring'::pg_catalog.regtype)\n"
					  "      AND NOT p.proisagg\n");

	processSQLNamePattern(pset.db, &buf, pattern, true, false,
						  "n.nspname", "p.proname", NULL,
						  "pg_catalog.pg_function_is_visible(p.oid)");

	appendPQExpBuffer(&buf, "ORDER BY 1, 2, 3, 4;");

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	myopt.nullPrint = NULL;
	myopt.title = _("List of functions");
	myopt.trans_headers = true;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	PQclear(res);
	return true;
}



/*
 * \dT
 * describe types
 */
bool
describeTypes(const char *pattern, bool verbose)
{
	PQExpBufferData buf;
	PGresult   *res;
	printQueryOpt myopt = pset.popt;

	initPQExpBuffer(&buf);

	printfPQExpBuffer(&buf,
					  "SELECT n.nspname as \"%s\",\n"
					  "  pg_catalog.format_type(t.oid, NULL) AS \"%s\",\n",
					  gettext_noop("Schema"),
					  gettext_noop("Name"));
	if (verbose)
		appendPQExpBuffer(&buf,
						  "  t.typname AS \"%s\",\n"
						  "  CASE WHEN t.typrelid != 0\n"
						  "      THEN CAST('tuple' AS pg_catalog.text)\n"
						  "    WHEN t.typlen < 0\n"
						  "      THEN CAST('var' AS pg_catalog.text)\n"
						  "    ELSE CAST(t.typlen AS pg_catalog.text)\n"
						  "  END AS \"%s\",\n",
						  gettext_noop("Internal name"),
						  gettext_noop("Size"));
	appendPQExpBuffer(&buf,
				"  pg_catalog.obj_description(t.oid, 'pg_type') as \"%s\"\n",
					  gettext_noop("Description"));

	appendPQExpBuffer(&buf, "FROM pg_catalog.pg_type t\n"
	 "     LEFT JOIN pg_catalog.pg_namespace n ON n.oid = t.typnamespace\n");

	/*
	 * do not include array types (start with underscore); do not include
	 * complex types (typrelid!=0) unless they are standalone composite types
	 */
	appendPQExpBuffer(&buf, "WHERE (t.typrelid = 0 ");
	appendPQExpBuffer(&buf, "OR (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c "
					  "WHERE c.oid = t.typrelid)) ");
	appendPQExpBuffer(&buf, "AND t.typname !~ '^_'\n");

	/* Match name pattern against either internal or external name */
	processSQLNamePattern(pset.db, &buf, pattern, true, false,
						  "n.nspname", "t.typname",
						  "pg_catalog.format_type(t.oid, NULL)",
						  "pg_catalog.pg_type_is_visible(t.oid)");

	appendPQExpBuffer(&buf, "ORDER BY 1, 2;");

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	myopt.nullPrint = NULL;
	myopt.title = _("List of data types");
	myopt.trans_headers = true;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	PQclear(res);
	return true;
}


/* \do
 */
bool
describeOperators(const char *pattern)
{
	PQExpBufferData buf;
	PGresult   *res;
	printQueryOpt myopt = pset.popt;

	initPQExpBuffer(&buf);

	printfPQExpBuffer(&buf,
					  "SELECT n.nspname as \"%s\",\n"
					  "  o.oprname AS \"%s\",\n"
					  "  CASE WHEN o.oprkind='l' THEN NULL ELSE pg_catalog.format_type(o.oprleft, NULL) END AS \"%s\",\n"
					  "  CASE WHEN o.oprkind='r' THEN NULL ELSE pg_catalog.format_type(o.oprright, NULL) END AS \"%s\",\n"
				   "  pg_catalog.format_type(o.oprresult, NULL) AS \"%s\",\n"
			 "  coalesce(pg_catalog.obj_description(o.oid, 'pg_operator'),\n"
	"           pg_catalog.obj_description(o.oprcode, 'pg_proc')) AS \"%s\"\n"
					  "FROM pg_catalog.pg_operator o\n"
	  "     LEFT JOIN pg_catalog.pg_namespace n ON n.oid = o.oprnamespace\n",
					  gettext_noop("Schema"),
					  gettext_noop("Name"),
					  gettext_noop("Left arg type"),
					  gettext_noop("Right arg type"),
					  gettext_noop("Result type"),
					  gettext_noop("Description"));

	processSQLNamePattern(pset.db, &buf, pattern, false, true,
						  "n.nspname", "o.oprname", NULL,
						  "pg_catalog.pg_operator_is_visible(o.oid)");

	appendPQExpBuffer(&buf, "ORDER BY 1, 2, 3, 4;");

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	myopt.nullPrint = NULL;
	myopt.title = _("List of operators");
	myopt.trans_headers = true;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	PQclear(res);
	return true;
}


/*
 * listAllDbs
 *
 * for \l, \list, and -l switch
 */
bool
listAllDbs(bool verbose)
{
	PGresult   *res;
	PQExpBufferData buf;
	printQueryOpt myopt = pset.popt;

	initPQExpBuffer(&buf);

	printfPQExpBuffer(&buf,
					  "SELECT d.datname as \"%s\",\n"
					  "       r.rolname as \"%s\",\n"
					  "       pg_catalog.pg_encoding_to_char(d.encoding) as \"%s\"",
					  gettext_noop("Name"),
					  gettext_noop("Owner"),
					  gettext_noop("Encoding"));
	if (verbose)
	{
		appendPQExpBuffer(&buf,
						  ",\n       t.spcname as \"%s\"",
						  gettext_noop("Tablespace"));
		appendPQExpBuffer(&buf,
						  ",\n       pg_catalog.shobj_description(d.oid, 'pg_database') as \"%s\"",
						  gettext_noop("Description"));
	}
	appendPQExpBuffer(&buf,
					  "\nFROM pg_catalog.pg_database d"
					  "\n  JOIN pg_catalog.pg_roles r ON d.datdba = r.oid\n");
	if (verbose)
		appendPQExpBuffer(&buf,
		   "  JOIN pg_catalog.pg_tablespace t on d.dattablespace = t.oid\n");
	appendPQExpBuffer(&buf, "ORDER BY 1;");
	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	myopt.nullPrint = NULL;
	myopt.title = _("List of databases");
	myopt.trans_headers = true;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	PQclear(res);
	return true;
}


/*
 * List Tables Grant/Revoke Permissions
 * \z (now also \dp -- perhaps more mnemonic)
 */
bool
permissionsList(const char *pattern)
{
	PQExpBufferData buf;
	PGresult   *res;
	printQueryOpt myopt = pset.popt;
	static const bool trans_columns[] = {false, false, true, false};

	initPQExpBuffer(&buf);

	/*
	 * we ignore indexes and toast tables since they have no meaningful rights
	 */
	printfPQExpBuffer(&buf,
					  "SELECT n.nspname as \"%s\",\n"
					  "  c.relname as \"%s\",\n"
					  "  CASE c.relkind WHEN 'r' THEN '%s' WHEN 'v' THEN '%s' WHEN 'S' THEN '%s' END as \"%s\",\n"
					  "  c.relacl as \"%s\"\n"
					  "FROM pg_catalog.pg_class c\n"
	   "     LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace\n"
					  "WHERE c.relkind IN ('r', 'v', 'S')\n",
					  gettext_noop("Schema"),
					  gettext_noop("Name"),
					  gettext_noop("table"), gettext_noop("view"), gettext_noop("sequence"),
					  gettext_noop("Type"),
					  gettext_noop("Access privileges"));

	/*
	 * Unless a schema pattern is specified, we suppress system and temp
	 * tables, since they normally aren't very interesting from a permissions
	 * point of view.  You can see 'em by explicit request though, eg with \z
	 * pg_catalog.*
	 */
	processSQLNamePattern(pset.db, &buf, pattern, true, false,
						  "n.nspname", "c.relname", NULL,
			"n.nspname !~ '^pg_' AND pg_catalog.pg_table_is_visible(c.oid)");

	appendPQExpBuffer(&buf, "ORDER BY 1, 2;");

	res = PSQLexec(buf.data, false);
	if (!res)
	{
		termPQExpBuffer(&buf);
		return false;
	}

	myopt.nullPrint = NULL;
	printfPQExpBuffer(&buf, _("Access privileges for database \"%s\""),
					  PQdb(pset.db));
	myopt.title = buf.data;
	myopt.trans_headers = true;
	myopt.trans_columns = trans_columns;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	termPQExpBuffer(&buf);
	PQclear(res);
	return true;
}



/*
 * Get object comments
 *
 * \dd [foo]
 *
 * Note: This only lists things that actually have a description. For complete
 * lists of things, there are other \d? commands.
 */
bool
objectDescription(const char *pattern)
{
	PQExpBufferData buf;
	PGresult   *res;
	printQueryOpt myopt = pset.popt;
	static const bool trans_columns[] = {false, false, true, false};

	initPQExpBuffer(&buf);

	appendPQExpBuffer(&buf,
					  "SELECT DISTINCT tt.nspname AS \"%s\", tt.name AS \"%s\", tt.object AS \"%s\", d.description AS \"%s\"\n"
					  "FROM (\n",
					  gettext_noop("Schema"),
					  gettext_noop("Name"),
					  gettext_noop("Object"),
					  gettext_noop("Description"));

	/* Aggregate descriptions */
	appendPQExpBuffer(&buf,
					  "  SELECT p.oid as oid, p.tableoid as tableoid,\n"
					  "  n.nspname as nspname,\n"
					  "  CAST(p.proname AS pg_catalog.text) as name,"
					  "  CAST('%s' AS pg_catalog.text) as object\n"
					  "  FROM pg_catalog.pg_proc p\n"
	 "       LEFT JOIN pg_catalog.pg_namespace n ON n.oid = p.pronamespace\n"
					  "  WHERE p.proisagg\n",
					  gettext_noop("aggregate"));
	processSQLNamePattern(pset.db, &buf, pattern, true, false,
						  "n.nspname", "p.proname", NULL,
						  "pg_catalog.pg_function_is_visible(p.oid)");

	/* Function descriptions (except in/outs for datatypes) */
	appendPQExpBuffer(&buf,
					  "UNION ALL\n"
					  "  SELECT p.oid as oid, p.tableoid as tableoid,\n"
					  "  n.nspname as nspname,\n"
					  "  CAST(p.proname AS pg_catalog.text) as name,"
					  "  CAST('%s' AS pg_catalog.text) as object\n"
					  "  FROM pg_catalog.pg_proc p\n"
	 "       LEFT JOIN pg_catalog.pg_namespace n ON n.oid = p.pronamespace\n"

		 "  WHERE p.prorettype <> 'pg_catalog.cstring'::pg_catalog.regtype\n"
					  "      AND (p.proargtypes[0] IS NULL\n"
					  "      OR   p.proargtypes[0] <> 'pg_catalog.cstring'::pg_catalog.regtype)\n"
					  "      AND NOT p.proisagg\n",
					  gettext_noop("function"));
	processSQLNamePattern(pset.db, &buf, pattern, true, false,
						  "n.nspname", "p.proname", NULL,
						  "pg_catalog.pg_function_is_visible(p.oid)");

	/* Operator descriptions (only if operator has its own comment) */
	appendPQExpBuffer(&buf,
					  "UNION ALL\n"
					  "  SELECT o.oid as oid, o.tableoid as tableoid,\n"
					  "  n.nspname as nspname,\n"
					  "  CAST(o.oprname AS pg_catalog.text) as name,"
					  "  CAST('%s' AS pg_catalog.text) as object\n"
					  "  FROM pg_catalog.pg_operator o\n"
	"       LEFT JOIN pg_catalog.pg_namespace n ON n.oid = o.oprnamespace\n",
					  gettext_noop("operator"));
	processSQLNamePattern(pset.db, &buf, pattern, false, false,
						  "n.nspname", "o.oprname", NULL,
						  "pg_catalog.pg_operator_is_visible(o.oid)");

	/* Type description */
	appendPQExpBuffer(&buf,
					  "UNION ALL\n"
					  "  SELECT t.oid as oid, t.tableoid as tableoid,\n"
					  "  n.nspname as nspname,\n"
					  "  pg_catalog.format_type(t.oid, NULL) as name,"
					  "  CAST('%s' AS pg_catalog.text) as object\n"
					  "  FROM pg_catalog.pg_type t\n"
	"       LEFT JOIN pg_catalog.pg_namespace n ON n.oid = t.typnamespace\n",
					  gettext_noop("data type"));
	processSQLNamePattern(pset.db, &buf, pattern, false, false,
						  "n.nspname", "pg_catalog.format_type(t.oid, NULL)",
						  NULL,
						  "pg_catalog.pg_type_is_visible(t.oid)");

	/* Relation (tables, views, indexes, sequences) descriptions */
	appendPQExpBuffer(&buf,
					  "UNION ALL\n"
					  "  SELECT c.oid as oid, c.tableoid as tableoid,\n"
					  "  n.nspname as nspname,\n"
					  "  CAST(c.relname AS pg_catalog.text) as name,\n"
					  "  CAST(\n"
					  "    CASE c.relkind WHEN 'r' THEN '%s' WHEN 'v' THEN '%s' WHEN 'i' THEN '%s' WHEN 'S' THEN '%s' END"
					  "  AS pg_catalog.text) as object\n"
					  "  FROM pg_catalog.pg_class c\n"
	 "       LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace\n"
					  "  WHERE c.relkind IN ('r', 'v', 'i', 'S')\n",
					  gettext_noop("table"),
					  gettext_noop("view"),
					  gettext_noop("index"),
					  gettext_noop("sequence"));
	processSQLNamePattern(pset.db, &buf, pattern, true, false,
						  "n.nspname", "c.relname", NULL,
						  "pg_catalog.pg_table_is_visible(c.oid)");

	/* Rule description (ignore rules for views) */
	appendPQExpBuffer(&buf,
					  "UNION ALL\n"
					  "  SELECT r.oid as oid, r.tableoid as tableoid,\n"
					  "  n.nspname as nspname,\n"
					  "  CAST(r.rulename AS pg_catalog.text) as name,"
					  "  CAST('%s' AS pg_catalog.text) as object\n"
					  "  FROM pg_catalog.pg_rewrite r\n"
				  "       JOIN pg_catalog.pg_class c ON c.oid = r.ev_class\n"
	 "       LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace\n"
					  "  WHERE r.rulename != '_RETURN'\n",
					  gettext_noop("rule"));
	/* XXX not sure what to do about visibility rule here? */
	processSQLNamePattern(pset.db, &buf, pattern, true, false,
						  "n.nspname", "r.rulename", NULL,
						  "pg_catalog.pg_table_is_visible(c.oid)");

	/* Trigger description */
	appendPQExpBuffer(&buf,
					  "UNION ALL\n"
					  "  SELECT t.oid as oid, t.tableoid as tableoid,\n"
					  "  n.nspname as nspname,\n"
					  "  CAST(t.tgname AS pg_catalog.text) as name,"
					  "  CAST('%s' AS pg_catalog.text) as object\n"
					  "  FROM pg_catalog.pg_trigger t\n"
				   "       JOIN pg_catalog.pg_class c ON c.oid = t.tgrelid\n"
	"       LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace\n",
					  gettext_noop("trigger"));
	/* XXX not sure what to do about visibility rule here? */
	processSQLNamePattern(pset.db, &buf, pattern, false, false,
						  "n.nspname", "t.tgname", NULL,
						  "pg_catalog.pg_table_is_visible(c.oid)");

	appendPQExpBuffer(&buf,
					  ") AS tt\n"
					  "  JOIN pg_catalog.pg_description d ON (tt.oid = d.objoid AND tt.tableoid = d.classoid AND d.objsubid = 0)\n");

	appendPQExpBuffer(&buf, "ORDER BY 1, 2, 3;");

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	myopt.nullPrint = NULL;
	myopt.title = _("Object descriptions");
	myopt.trans_headers = true;
	myopt.trans_columns = trans_columns;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	PQclear(res);
	return true;
}



/*
 * describeTableDetails (for \d)
 *
 * This routine finds the tables to be displayed, and calls
 * describeOneTableDetails for each one.
 *
 * verbose: if true, this is \d+
 */
bool
describeTableDetails(const char *pattern, bool verbose)
{
	PQExpBufferData buf;
	PGresult   *res;
	int			i;

	initPQExpBuffer(&buf);

	printfPQExpBuffer(&buf,
					  "SELECT c.oid,\n"
					  "  n.nspname,\n"
					  "  c.relname\n"
					  "FROM pg_catalog.pg_class c\n"
	 "     LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace\n");

	processSQLNamePattern(pset.db, &buf, pattern, false, false,
						  "n.nspname", "c.relname", NULL,
						  "pg_catalog.pg_table_is_visible(c.oid)");

	appendPQExpBuffer(&buf, "ORDER BY 2, 3;");

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	if (PQntuples(res) == 0)
	{
		if (!pset.quiet)
			fprintf(stderr, _("Did not find any relation named \"%s\".\n"),
					pattern);
		PQclear(res);
		return false;
	}

	for (i = 0; i < PQntuples(res); i++)
	{
		const char *oid;
		const char *nspname;
		const char *relname;

		oid = PQgetvalue(res, i, 0);
		nspname = PQgetvalue(res, i, 1);
		relname = PQgetvalue(res, i, 2);

		if (!describeOneTableDetails(nspname, relname, oid, verbose))
		{
			PQclear(res);
			return false;
		}
		if (cancel_pressed)
		{
			PQclear(res);
			return false;
		}
	}

	PQclear(res);
	return true;
}

/*
 * describeOneTableDetails (for \d)
 *
 * Unfortunately, the information presented here is so complicated that it
 * cannot be done in a single query. So we have to assemble the printed table
 * by hand and pass it to the underlying printTable() function.
 */
static bool
describeOneTableDetails(const char *schemaname,
						const char *relationname,
						const char *oid,
						bool verbose)
{
	PQExpBufferData buf;
	PGresult   *res = NULL;
	printTableOpt myopt = pset.popt.topt;
	int			i;
	char	   *view_def = NULL;
	const char *headers[5];
	char	  **cells = NULL;
	char	  **footers = NULL;
	char	  **ptr;
	PQExpBufferData title;
	PQExpBufferData tmpbuf;
	int			cols = 0;
	int			numrows = 0;
	struct
	{
		int16		checks;
		int16		triggers;
		char		relkind;
		bool		hasindex;
		bool		hasrules;
		bool		hasoids;
		Oid			tablespace;
	}			tableinfo;
	bool		show_modifiers = false;
	bool		retval;

	retval = false;

	/* This output looks confusing in expanded mode. */
	myopt.expanded = false;

	initPQExpBuffer(&buf);
	initPQExpBuffer(&title);
	initPQExpBuffer(&tmpbuf);

	/* Get general table info */
	printfPQExpBuffer(&buf,
	   "SELECT relhasindex, relkind, relchecks, reltriggers, relhasrules, \n"
					  "relhasoids %s \n"
					  "FROM pg_catalog.pg_class WHERE oid = '%s'",
					  pset.sversion >= 80000 ? ", reltablespace" : "",
					  oid);
	res = PSQLexec(buf.data, false);
	if (!res)
		goto error_return;

	/* Did we get anything? */
	if (PQntuples(res) == 0)
	{
		if (!pset.quiet)
			fprintf(stderr, _("Did not find any relation with OID %s.\n"),
					oid);
		goto error_return;
	}

	/* FIXME: check for null pointers here? */
	tableinfo.checks = atoi(PQgetvalue(res, 0, 2));
	tableinfo.triggers = atoi(PQgetvalue(res, 0, 3));
	tableinfo.relkind = *(PQgetvalue(res, 0, 1));
	tableinfo.hasindex = strcmp(PQgetvalue(res, 0, 0), "t") == 0;
	tableinfo.hasrules = strcmp(PQgetvalue(res, 0, 4), "t") == 0;
	tableinfo.hasoids = strcmp(PQgetvalue(res, 0, 5), "t") == 0;
	tableinfo.tablespace = (pset.sversion >= 80000) ?
		atooid(PQgetvalue(res, 0, 6)) : 0;
	PQclear(res);

	headers[0] = _("Column");
	headers[1] = _("Type");
	cols = 2;

	if (tableinfo.relkind == 'r' || tableinfo.relkind == 'v')
	{
		show_modifiers = true;
		cols++;
		headers[cols - 1] = _("Modifiers");
	}

	if (verbose)
	{
		cols++;
		headers[cols - 1] = _("Description");
	}

	headers[cols] = NULL;

	/* Get column info (index requires additional checks) */
	printfPQExpBuffer(&buf, "SELECT a.attname,");
	appendPQExpBuffer(&buf, "\n  pg_catalog.format_type(a.atttypid, a.atttypmod),"
					  "\n  (SELECT substring(pg_catalog.pg_get_expr(d.adbin, d.adrelid) for 128)"
					  "\n   FROM pg_catalog.pg_attrdef d"
					  "\n   WHERE d.adrelid = a.attrelid AND d.adnum = a.attnum AND a.atthasdef),"
					  "\n  a.attnotnull, a.attnum");
	if (verbose)
		appendPQExpBuffer(&buf, ", pg_catalog.col_description(a.attrelid, a.attnum)");
	appendPQExpBuffer(&buf, "\nFROM pg_catalog.pg_attribute a");
	if (tableinfo.relkind == 'i')
		appendPQExpBuffer(&buf, ", pg_catalog.pg_index i");
	appendPQExpBuffer(&buf, "\nWHERE a.attrelid = '%s' AND a.attnum > 0 AND NOT a.attisdropped", oid);
	if (tableinfo.relkind == 'i')
		appendPQExpBuffer(&buf, " AND a.attrelid = i.indexrelid");
	appendPQExpBuffer(&buf, "\nORDER BY a.attnum");

	res = PSQLexec(buf.data, false);
	if (!res)
		goto error_return;
	numrows = PQntuples(res);

	/* Check if table is a view */
	if (tableinfo.relkind == 'v')
	{
		PGresult   *result;

		printfPQExpBuffer(&buf, "SELECT pg_catalog.pg_get_viewdef('%s'::pg_catalog.oid, true)", oid);
		result = PSQLexec(buf.data, false);
		if (!result)
			goto error_return;

		if (PQntuples(result) > 0)
			view_def = pg_strdup(PQgetvalue(result, 0, 0));

		PQclear(result);
	}

	/* Generate table cells to be printed */
	/* note: initialize all cells[] to NULL in case of error exit */
	cells = pg_malloc_zero((numrows * cols + 1) * sizeof(*cells));

	for (i = 0; i < numrows; i++)
	{
		/* Name */
#ifdef WIN32
		cells[i * cols + 0] = mbvalidate(PQgetvalue(res, i, 0), myopt.encoding);
#else
		cells[i * cols + 0] = PQgetvalue(res, i, 0);	/* don't free this
														 * afterwards */
#endif

		/* Type */
#ifdef WIN32
		cells[i * cols + 1] = mbvalidate(PQgetvalue(res, i, 1), myopt.encoding);
#else
		cells[i * cols + 1] = PQgetvalue(res, i, 1);	/* don't free this
														 * either */
#endif

		/* Extra: not null and default */
		if (show_modifiers)
		{
			resetPQExpBuffer(&tmpbuf);
			if (strcmp(PQgetvalue(res, i, 3), "t") == 0)
				appendPQExpBufferStr(&tmpbuf, "not null");

			/* handle "default" here */
			/* (note: above we cut off the 'default' string at 128) */
			if (strlen(PQgetvalue(res, i, 2)) != 0)
			{
				if (tmpbuf.len > 0)
					appendPQExpBufferStr(&tmpbuf, " ");
				appendPQExpBuffer(&tmpbuf, "default %s",
								  PQgetvalue(res, i, 2));
			}

#ifdef WIN32
			cells[i * cols + 2] = pg_strdup(mbvalidate(tmpbuf.data, myopt.encoding));
#else
			cells[i * cols + 2] = pg_strdup(tmpbuf.data);
#endif
		}

		/* Description */
		if (verbose)
#ifdef WIN32
			cells[i * cols + cols - 1] = mbvalidate(PQgetvalue(res, i, 5), myopt.encoding);
#else
			cells[i * cols + cols - 1] = PQgetvalue(res, i, 5);
#endif
	}

	/* Make title */
	switch (tableinfo.relkind)
	{
		case 'r':
			printfPQExpBuffer(&title, _("Table \"%s.%s\""),
							  schemaname, relationname);
			break;
		case 'v':
			printfPQExpBuffer(&title, _("View \"%s.%s\""),
							  schemaname, relationname);
			break;
		case 'S':
			printfPQExpBuffer(&title, _("Sequence \"%s.%s\""),
							  schemaname, relationname);
			break;
		case 'i':
			printfPQExpBuffer(&title, _("Index \"%s.%s\""),
							  schemaname, relationname);
			break;
		case 's':
			/* not used as of 8.2, but keep it for backwards compatibility */
			printfPQExpBuffer(&title, _("Special relation \"%s.%s\""),
							  schemaname, relationname);
			break;
		case 't':
			printfPQExpBuffer(&title, _("TOAST table \"%s.%s\""),
							  schemaname, relationname);
			break;
		case 'c':
			printfPQExpBuffer(&title, _("Composite type \"%s.%s\""),
							  schemaname, relationname);
			break;
		default:
			printfPQExpBuffer(&title, _("?%c? \"%s.%s\""),
							  tableinfo.relkind, schemaname, relationname);
			break;
	}

	/* Make footers */
	if (tableinfo.relkind == 'i')
	{
		/* Footer information about an index */
		PGresult   *result;

		printfPQExpBuffer(&buf,
						  "SELECT i.indisunique, i.indisprimary, i.indisclustered, i.indisvalid, a.amname, c2.relname,\n"
					"  pg_catalog.pg_get_expr(i.indpred, i.indrelid, true)\n"
						  "FROM pg_catalog.pg_index i, pg_catalog.pg_class c, pg_catalog.pg_class c2, pg_catalog.pg_am a\n"
		  "WHERE i.indexrelid = c.oid AND c.oid = '%s' AND c.relam = a.oid\n"
						  "AND i.indrelid = c2.oid",
						  oid);

		result = PSQLexec(buf.data, false);
		if (!result)
			goto error_return;
		else if (PQntuples(result) != 1)
		{
			PQclear(result);
			goto error_return;
		}
		else
		{
			char	   *indisunique = PQgetvalue(result, 0, 0);
			char	   *indisprimary = PQgetvalue(result, 0, 1);
			char	   *indisclustered = PQgetvalue(result, 0, 2);
			char	   *indisvalid = PQgetvalue(result, 0, 3);
			char	   *indamname = PQgetvalue(result, 0, 4);
			char	   *indtable = PQgetvalue(result, 0, 5);
			char	   *indpred = PQgetvalue(result, 0, 6);
			int			count_footers = 0;

			if (strcmp(indisprimary, "t") == 0)
				printfPQExpBuffer(&tmpbuf, _("primary key, "));
			else if (strcmp(indisunique, "t") == 0)
				printfPQExpBuffer(&tmpbuf, _("unique, "));
			else
				resetPQExpBuffer(&tmpbuf);
			appendPQExpBuffer(&tmpbuf, "%s, ", indamname);

			/* we assume here that index and table are in same schema */
			appendPQExpBuffer(&tmpbuf, _("for table \"%s.%s\""),
							  schemaname, indtable);

			if (strlen(indpred))
				appendPQExpBuffer(&tmpbuf, _(", predicate (%s)"), indpred);

			if (strcmp(indisclustered, "t") == 0)
				appendPQExpBuffer(&tmpbuf, _(", clustered"));

			if (strcmp(indisvalid, "t") != 0)
				appendPQExpBuffer(&tmpbuf, _(", invalid"));

			footers = pg_malloc_zero(4 * sizeof(*footers));
			footers[count_footers++] = pg_strdup(tmpbuf.data);
			add_tablespace_footer(tableinfo.relkind, tableinfo.tablespace,
								  footers, &count_footers, tmpbuf, true);
			footers[count_footers] = NULL;

		}

		PQclear(result);
	}
	else if (view_def)
	{
		PGresult   *result = NULL;
		int			rule_count = 0;
		int			count_footers = 0;

		/* count rules other than the view rule */
		if (tableinfo.hasrules)
		{
			printfPQExpBuffer(&buf,
							  "SELECT r.rulename, trim(trailing ';' from pg_catalog.pg_get_ruledef(r.oid, true))\n"
							  "FROM pg_catalog.pg_rewrite r\n"
			"WHERE r.ev_class = '%s' AND r.rulename != '_RETURN' ORDER BY 1",
							  oid);
			result = PSQLexec(buf.data, false);
			if (!result)
				goto error_return;
			else
				rule_count = PQntuples(result);
		}

		/* Footer information about a view */
		footers = pg_malloc_zero((rule_count + 3) * sizeof(*footers));
		footers[count_footers] = pg_malloc(64 + strlen(view_def));
		snprintf(footers[count_footers], 64 + strlen(view_def),
				 _("View definition:\n%s"), view_def);
		count_footers++;

		/* print rules */
		if (rule_count > 0)
		{
			printfPQExpBuffer(&buf, _("Rules:"));
			footers[count_footers++] = pg_strdup(buf.data);
			for (i = 0; i < rule_count; i++)
			{
				const char *ruledef;

				/* Everything after "CREATE RULE" is echoed verbatim */
				ruledef = PQgetvalue(result, i, 1);
				ruledef += 12;

				printfPQExpBuffer(&buf, " %s", ruledef);

				footers[count_footers++] = pg_strdup(buf.data);
			}
			PQclear(result);
		}

		footers[count_footers] = NULL;

	}
	else if (tableinfo.relkind == 'r')
	{
		/* Footer information about a table */
		PGresult   *result1 = NULL,
				   *result2 = NULL,
				   *result3 = NULL,
				   *result4 = NULL,
				   *result5 = NULL,
				   *result6 = NULL;
		int			check_count = 0,
					index_count = 0,
					foreignkey_count = 0,
					rule_count = 0,
					trigger_count = 0,
					inherits_count = 0;
		int			count_footers = 0;

		/* count indexes */
		if (tableinfo.hasindex)
		{
			printfPQExpBuffer(&buf,
							  "SELECT c2.relname, i.indisprimary, i.indisunique, i.indisclustered, i.indisvalid, "
							  "pg_catalog.pg_get_indexdef(i.indexrelid, 0, true), c2.reltablespace\n"
							  "FROM pg_catalog.pg_class c, pg_catalog.pg_class c2, pg_catalog.pg_index i\n"
							  "WHERE c.oid = '%s' AND c.oid = i.indrelid AND i.indexrelid = c2.oid\n"
			  "ORDER BY i.indisprimary DESC, i.indisunique DESC, c2.relname",
							  oid);
			result1 = PSQLexec(buf.data, false);
			if (!result1)
				goto error_return;
			else
				index_count = PQntuples(result1);
		}

		/* count table (and column) check constraints */
		if (tableinfo.checks)
		{
			printfPQExpBuffer(&buf,
							  "SELECT r.conname, "
							  "pg_catalog.pg_get_constraintdef(r.oid, true)\n"
							  "FROM pg_catalog.pg_constraint r\n"
					"WHERE r.conrelid = '%s' AND r.contype = 'c' ORDER BY 1",
							  oid);
			result2 = PSQLexec(buf.data, false);
			if (!result2)
			{
				PQclear(result1);
				goto error_return;
			}
			else
				check_count = PQntuples(result2);
		}

		/* count rules */
		if (tableinfo.hasrules)
		{
			if (pset.sversion < 80300)
			{
				printfPQExpBuffer(&buf,
								  "SELECT r.rulename, trim(trailing ';' from pg_catalog.pg_get_ruledef(r.oid, true)), "
								  "'O'::char AS ev_enabled\n"
								  "FROM pg_catalog.pg_rewrite r\n"
								  "WHERE r.ev_class = '%s' ORDER BY 1",
								  oid);
			}
			else
			{
				printfPQExpBuffer(&buf,
								  "SELECT r.rulename, trim(trailing ';' from pg_catalog.pg_get_ruledef(r.oid, true)), "
								  "ev_enabled\n"
								  "FROM pg_catalog.pg_rewrite r\n"
								  "WHERE r.ev_class = '%s' ORDER BY 1",
								  oid);
			}
			result3 = PSQLexec(buf.data, false);
			if (!result3)
			{
				PQclear(result1);
				PQclear(result2);
				goto error_return;
			}
			else
				rule_count = PQntuples(result3);
		}

		/* count triggers (but ignore foreign-key triggers) */
		if (tableinfo.triggers)
		{
			printfPQExpBuffer(&buf,
					 "SELECT t.tgname, pg_catalog.pg_get_triggerdef(t.oid), "
							  "t.tgenabled\n"
							  "FROM pg_catalog.pg_trigger t\n"
							  "WHERE t.tgrelid = '%s' "
							  "AND t.tgconstraint = 0\n"
							  "ORDER BY 1",
							  oid);
			result4 = PSQLexec(buf.data, false);
			if (!result4)
			{
				PQclear(result1);
				PQclear(result2);
				PQclear(result3);
				goto error_return;
			}
			else
				trigger_count = PQntuples(result4);
		}

		/* count foreign-key constraints (there are none if no triggers) */
		if (tableinfo.triggers)
		{
			printfPQExpBuffer(&buf,
							  "SELECT conname,\n"
				   "  pg_catalog.pg_get_constraintdef(oid, true) as condef\n"
							  "FROM pg_catalog.pg_constraint r\n"
					"WHERE r.conrelid = '%s' AND r.contype = 'f' ORDER BY 1",
							  oid);
			result5 = PSQLexec(buf.data, false);
			if (!result5)
			{
				PQclear(result1);
				PQclear(result2);
				PQclear(result3);
				PQclear(result4);
				goto error_return;
			}
			else
				foreignkey_count = PQntuples(result5);
		}

		/* count inherited tables */
		printfPQExpBuffer(&buf, "SELECT c.oid::regclass FROM pg_catalog.pg_class c, pg_catalog.pg_inherits i WHERE c.oid=i.inhparent AND i.inhrelid = '%s' ORDER BY inhseqno ASC", oid);

		result6 = PSQLexec(buf.data, false);
		if (!result6)
		{
			PQclear(result1);
			PQclear(result2);
			PQclear(result3);
			PQclear(result4);
			PQclear(result5);
			goto error_return;
		}
		else
			inherits_count = PQntuples(result6);

		footers = pg_malloc_zero((index_count + check_count + rule_count + trigger_count + foreignkey_count + inherits_count + 7 + 1)
								 * sizeof(*footers));

		/* print indexes */
		if (index_count > 0)
		{
			printfPQExpBuffer(&buf, _("Indexes:"));
			footers[count_footers++] = pg_strdup(buf.data);
			for (i = 0; i < index_count; i++)
			{
				const char *indexdef;
				const char *usingpos;
				PQExpBufferData tmpbuf;

				/* Output index name */
				printfPQExpBuffer(&buf, _("    \"%s\""),
								  PQgetvalue(result1, i, 0));

				/* Label as primary key or unique (but not both) */
				appendPQExpBuffer(&buf,
								  strcmp(PQgetvalue(result1, i, 1), "t") == 0
								  ? " PRIMARY KEY," :
								  (strcmp(PQgetvalue(result1, i, 2), "t") == 0
								   ? " UNIQUE,"
								   : ""));
				/* Everything after "USING" is echoed verbatim */
				indexdef = PQgetvalue(result1, i, 5);
				usingpos = strstr(indexdef, " USING ");
				if (usingpos)
					indexdef = usingpos + 7;

				appendPQExpBuffer(&buf, " %s", indexdef);

				if (strcmp(PQgetvalue(result1, i, 3), "t") == 0)
					appendPQExpBuffer(&buf, " CLUSTER");

				if (strcmp(PQgetvalue(result1, i, 4), "t") != 0)
					appendPQExpBuffer(&buf, " INVALID");

				/* Print tablespace of the index on the same line */
				count_footers += 1;
				initPQExpBuffer(&tmpbuf);
				if (add_tablespace_footer('i',
										  atooid(PQgetvalue(result1, i, 6)),
									 footers, &count_footers, tmpbuf, false))
				{
					appendPQExpBuffer(&buf, ", ");
					appendPQExpBuffer(&buf, tmpbuf.data);

					count_footers -= 2;
				}
				else
					count_footers -= 1;
				termPQExpBuffer(&tmpbuf);

				footers[count_footers++] = pg_strdup(buf.data);
			}
		}

		/* print check constraints */
		if (check_count > 0)
		{
			printfPQExpBuffer(&buf, _("Check constraints:"));
			footers[count_footers++] = pg_strdup(buf.data);
			for (i = 0; i < check_count; i++)
			{
				printfPQExpBuffer(&buf, _("    \"%s\" %s"),
								  PQgetvalue(result2, i, 0),
								  PQgetvalue(result2, i, 1));

				footers[count_footers++] = pg_strdup(buf.data);
			}
		}

		/* print foreign key constraints */
		if (foreignkey_count > 0)
		{
			printfPQExpBuffer(&buf, _("Foreign-key constraints:"));
			footers[count_footers++] = pg_strdup(buf.data);
			for (i = 0; i < foreignkey_count; i++)
			{
				printfPQExpBuffer(&buf, _("    \"%s\" %s"),
								  PQgetvalue(result5, i, 0),
								  PQgetvalue(result5, i, 1));

				footers[count_footers++] = pg_strdup(buf.data);
			}
		}

		/* print rules */
		if (rule_count > 0)
		{
			bool		have_heading;
			int			category;

			for (category = 0; category < 4; category++)
			{
				have_heading = false;

				for (i = 0; i < rule_count; i++)
				{
					const char *ruledef;
					bool		list_rule = false;

					switch (category)
					{
						case 0:
							if (*PQgetvalue(result3, i, 2) == 'O')
								list_rule = true;
							break;
						case 1:
							if (*PQgetvalue(result3, i, 2) == 'D')
								list_rule = true;
							break;
						case 2:
							if (*PQgetvalue(result3, i, 2) == 'A')
								list_rule = true;
							break;
						case 3:
							if (*PQgetvalue(result3, i, 2) == 'R')
								list_rule = true;
							break;
					}
					if (!list_rule)
						continue;

					if (!have_heading)
					{
						switch (category)
						{
							case 0:
								printfPQExpBuffer(&buf, _("Rules:"));
								break;
							case 1:
								printfPQExpBuffer(&buf, _("Disabled rules:"));
								break;
							case 2:
								printfPQExpBuffer(&buf, _("Rules firing always:"));
								break;
							case 3:
								printfPQExpBuffer(&buf, _("Rules firing on replica only:"));
								break;
						}
						footers[count_footers++] = pg_strdup(buf.data);
						have_heading = true;
					}

					/* Everything after "CREATE RULE" is echoed verbatim */
					ruledef = PQgetvalue(result3, i, 1);
					ruledef += 12;
					printfPQExpBuffer(&buf, "    %s", ruledef);
					footers[count_footers++] = pg_strdup(buf.data);
				}
			}
		}

		/* print triggers */
		if (trigger_count > 0)
		{
			bool		have_heading;
			int			category;

			/*
			 * split the output into 4 different categories. Enabled triggers,
			 * disabled triggers and the two special ALWAYS and REPLICA
			 * configurations.
			 */
			for (category = 0; category < 4; category++)
			{
				have_heading = false;
				for (i = 0; i < trigger_count; i++)
				{
					bool		list_trigger;
					const char *tgdef;
					const char *usingpos;
					const char *tgenabled;

					/* Check if this trigger falls into the current category */
					tgenabled = PQgetvalue(result4, i, 2);
					list_trigger = false;
					switch (category)
					{
						case 0:
							if (*tgenabled == 'O' || *tgenabled == 't')
								list_trigger = true;
							break;
						case 1:
							if (*tgenabled == 'D' || *tgenabled == 'f')
								list_trigger = true;
							break;
						case 2:
							if (*tgenabled == 'A')
								list_trigger = true;
							break;
						case 3:
							if (*tgenabled == 'R')
								list_trigger = true;
							break;
					}
					if (list_trigger == false)
						continue;

					/* Print the category heading once */
					if (have_heading == false)
					{
						switch (category)
						{
							case 0:
								printfPQExpBuffer(&buf, _("Triggers:"));
								break;
							case 1:
								printfPQExpBuffer(&buf, _("Disabled triggers:"));
								break;
							case 2:
								printfPQExpBuffer(&buf, _("Triggers firing always:"));
								break;
							case 3:
								printfPQExpBuffer(&buf, _("Triggers firing on replica only:"));
								break;

						}
						footers[count_footers++] = pg_strdup(buf.data);
						have_heading = true;
					}

					/* Everything after "TRIGGER" is echoed verbatim */
					tgdef = PQgetvalue(result4, i, 1);
					usingpos = strstr(tgdef, " TRIGGER ");
					if (usingpos)
						tgdef = usingpos + 9;

					printfPQExpBuffer(&buf, "    %s", tgdef);
					footers[count_footers++] = pg_strdup(buf.data);
				}
			}
		}

		/* print inherits */
		for (i = 0; i < inherits_count; i++)
		{
			const char *s = _("Inherits");

			if (i == 0)
				printfPQExpBuffer(&buf, "%s: %s", s, PQgetvalue(result6, i, 0));
			else
				printfPQExpBuffer(&buf, "%*s  %s", (int) strlen(s), "", PQgetvalue(result6, i, 0));
			if (i < inherits_count - 1)
				appendPQExpBuffer(&buf, ",");

			footers[count_footers++] = pg_strdup(buf.data);
		}

		if (verbose)
		{
			const char *s = _("Has OIDs");

			printfPQExpBuffer(&buf, "%s: %s", s,
							  (tableinfo.hasoids ? _("yes") : _("no")));
			footers[count_footers++] = pg_strdup(buf.data);
		}

		add_tablespace_footer(tableinfo.relkind, tableinfo.tablespace,
							  footers, &count_footers, buf, true);
		/* end of list marker */
		footers[count_footers] = NULL;

		PQclear(result1);
		PQclear(result2);
		PQclear(result3);
		PQclear(result4);
		PQclear(result5);
		PQclear(result6);
	}

	printTable(title.data, headers,
			   (const char **) cells, (const char **) footers,
			   "llll", &myopt, pset.queryFout, pset.logfile);

	retval = true;

error_return:

	/* clean up */
	termPQExpBuffer(&buf);
	termPQExpBuffer(&title);
	termPQExpBuffer(&tmpbuf);

	if (cells)
	{
		for (i = 0; i < numrows; i++)
		{
			if (show_modifiers)
				free(cells[i * cols + 2]);
		}
		free(cells);
	}

	if (footers)
	{
		for (ptr = footers; *ptr; ptr++)
			free(*ptr);
		free(footers);
	}

	if (view_def)
		free(view_def);

	if (res)
		PQclear(res);

	return retval;
}


/*
 * Return true if the relation uses non default tablespace;
 * otherwise return false
 */
static bool
add_tablespace_footer(char relkind, Oid tablespace, char **footers,
					  int *count, PQExpBufferData buf, bool newline)
{
	/* relkinds for which we support tablespaces */
	if (relkind == 'r' || relkind == 'i')
	{
		/*
		 * We ignore the database default tablespace so that users not using
		 * tablespaces don't need to know about them.
		 */
		if (tablespace != 0)
		{
			PGresult   *result1 = NULL;

			printfPQExpBuffer(&buf, "SELECT spcname FROM pg_tablespace \n"
							  "WHERE oid = '%u';", tablespace);
			result1 = PSQLexec(buf.data, false);
			if (!result1)
				return false;
			/* Should always be the case, but.... */
			if (PQntuples(result1) > 0)
			{
				printfPQExpBuffer(&buf,
				  newline ? _("Tablespace: \"%s\"") : _("tablespace \"%s\""),
								  PQgetvalue(result1, 0, 0));

				footers[(*count)++] = pg_strdup(buf.data);
			}
			PQclear(result1);

			return true;
		}
	}

	return false;
}

/*
 * \du or \dg
 *
 * Describes roles.  Any schema portion of the pattern is ignored.
 */
bool
describeRoles(const char *pattern, bool verbose)
{
	PQExpBufferData buf;
	PGresult   *res;
	printQueryOpt myopt = pset.popt;
	static const bool trans_columns[] = {false, true, true, true, true, false, false};

	initPQExpBuffer(&buf);

	printfPQExpBuffer(&buf,
					  "SELECT r.rolname AS \"%s\",\n"
				"  CASE WHEN r.rolsuper THEN '%s' ELSE '%s' END AS \"%s\",\n"
		   "  CASE WHEN r.rolcreaterole THEN '%s' ELSE '%s' END AS \"%s\",\n"
			 "  CASE WHEN r.rolcreatedb THEN '%s' ELSE '%s' END AS \"%s\",\n"
		"  CASE WHEN r.rolconnlimit < 0 THEN CAST('%s' AS pg_catalog.text)\n"
					  "       ELSE CAST(r.rolconnlimit AS pg_catalog.text)\n"
					  "  END AS \"%s\", \n"
					  "  ARRAY(SELECT b.rolname FROM pg_catalog.pg_auth_members m JOIN pg_catalog.pg_roles b ON (m.roleid = b.oid) WHERE m.member = r.oid) as \"%s\"",
					  gettext_noop("Role name"),
					  gettext_noop("yes"), gettext_noop("no"),
					  gettext_noop("Superuser"),
					  gettext_noop("yes"), gettext_noop("no"),
					  gettext_noop("Create role"),
					  gettext_noop("yes"), gettext_noop("no"),
					  gettext_noop("Create DB"),
					  gettext_noop("no limit"),
					  gettext_noop("Connections"),
					  gettext_noop("Member of"));

	if (verbose)
		appendPQExpBuffer(&buf, "\n, pg_catalog.shobj_description(r.oid, 'pg_authid') AS \"%s\"",
						  gettext_noop("Description"));

	appendPQExpBuffer(&buf, "\nFROM pg_catalog.pg_roles r\n");

	processSQLNamePattern(pset.db, &buf, pattern, false, false,
						  NULL, "r.rolname", NULL, NULL);

	appendPQExpBuffer(&buf, "ORDER BY 1;");

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	myopt.nullPrint = NULL;
	myopt.title = _("List of roles");
	myopt.trans_headers = true;
	myopt.trans_columns = trans_columns;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	PQclear(res);
	return true;
}


/*
 * listTables()
 *
 * handler for \d, \dt, etc.
 *
 * tabtypes is an array of characters, specifying what info is desired:
 * t - tables
 * i - indexes
 * v - views
 * s - sequences
 * S - system tables (pg_catalog)
 * (any order of the above is fine)
 */
bool
listTables(const char *tabtypes, const char *pattern, bool verbose)
{
	bool		showTables = strchr(tabtypes, 't') != NULL;
	bool		showIndexes = strchr(tabtypes, 'i') != NULL;
	bool		showViews = strchr(tabtypes, 'v') != NULL;
	bool		showSeq = strchr(tabtypes, 's') != NULL;
	bool		showSystem = strchr(tabtypes, 'S') != NULL;

	PQExpBufferData buf;
	PGresult   *res;
	printQueryOpt myopt = pset.popt;
	static const bool trans_columns[] = {false, false, true, false, false, false};

	if (!(showTables || showIndexes || showViews || showSeq))
		showTables = showViews = showSeq = true;

	initPQExpBuffer(&buf);

	/*
	 * Note: as of Pg 8.2, we no longer use relkind 's', but we keep it here
	 * for backwards compatibility.
	 */
	printfPQExpBuffer(&buf,
					  "SELECT n.nspname as \"%s\",\n"
					  "  c.relname as \"%s\",\n"
					  "  CASE c.relkind WHEN 'r' THEN '%s' WHEN 'v' THEN '%s' WHEN 'i' THEN '%s' WHEN 'S' THEN '%s' WHEN 's' THEN '%s' END as \"%s\",\n"
					  "  r.rolname as \"%s\"",
					  gettext_noop("Schema"),
					  gettext_noop("Name"),
					  gettext_noop("table"), gettext_noop("view"), gettext_noop("index"), gettext_noop("sequence"), gettext_noop("special"),
					  gettext_noop("Type"),
					  gettext_noop("Owner"));

	if (showIndexes)
		appendPQExpBuffer(&buf,
						  ",\n c2.relname as \"%s\"",
						  gettext_noop("Table"));

	if (verbose)
		appendPQExpBuffer(&buf,
			  ",\n  pg_catalog.obj_description(c.oid, 'pg_class') as \"%s\"",
						  gettext_noop("Description"));

	appendPQExpBuffer(&buf,
					  "\nFROM pg_catalog.pg_class c"
					"\n     JOIN pg_catalog.pg_roles r ON r.oid = c.relowner"
	 "\n     LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace");
	if (showIndexes)
		appendPQExpBuffer(&buf,
			 "\n     LEFT JOIN pg_catalog.pg_index i ON i.indexrelid = c.oid"
		   "\n     LEFT JOIN pg_catalog.pg_class c2 ON i.indrelid = c2.oid");

	appendPQExpBuffer(&buf, "\nWHERE c.relkind IN (");
	if (showTables)
		appendPQExpBuffer(&buf, "'r',");
	if (showViews)
		appendPQExpBuffer(&buf, "'v',");
	if (showIndexes)
		appendPQExpBuffer(&buf, "'i',");
	if (showSeq)
		appendPQExpBuffer(&buf, "'S',");
	if (showSystem && showTables)
		appendPQExpBuffer(&buf, "'s',");
	appendPQExpBuffer(&buf, "''");		/* dummy */
	appendPQExpBuffer(&buf, ")\n");

	/*
	 * If showSystem is specified, show only system objects (those in
	 * pg_catalog).  Otherwise, suppress system objects, including those in
	 * pg_catalog and pg_toast.  (We don't want to hide temp tables though.)
	 */
	if (showSystem)
		appendPQExpBuffer(&buf,
						  "  AND n.nspname = 'pg_catalog'\n");
	else
		appendPQExpBuffer(&buf,
						  "  AND n.nspname <> 'pg_catalog'\n"
						  "  AND n.nspname !~ '^pg_toast'\n");

	processSQLNamePattern(pset.db, &buf, pattern, true, false,
						  "n.nspname", "c.relname", NULL,
						  "pg_catalog.pg_table_is_visible(c.oid)");

	appendPQExpBuffer(&buf, "ORDER BY 1,2;");

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	if (PQntuples(res) == 0 && !pset.quiet)
	{
		if (pattern)
			fprintf(pset.queryFout, _("No matching relations found.\n"));
		else
			fprintf(pset.queryFout, _("No relations found.\n"));
	}
	else
	{
		myopt.nullPrint = NULL;
		myopt.title = _("List of relations");
		myopt.trans_headers = true;
		myopt.trans_columns = trans_columns;

		printQuery(res, &myopt, pset.queryFout, pset.logfile);
	}

	PQclear(res);
	return true;
}


/*
 * \dD
 *
 * Describes domains.
 */
bool
listDomains(const char *pattern)
{
	PQExpBufferData buf;
	PGresult   *res;
	printQueryOpt myopt = pset.popt;

	initPQExpBuffer(&buf);

	printfPQExpBuffer(&buf,
					  "SELECT n.nspname as \"%s\",\n"
					  "       t.typname as \"%s\",\n"
	 "       pg_catalog.format_type(t.typbasetype, t.typtypmod) as \"%s\",\n"
					  "       CASE WHEN t.typnotnull AND t.typdefault IS NOT NULL THEN 'not null default '||t.typdefault\n"
	"            WHEN t.typnotnull AND t.typdefault IS NULL THEN 'not null'\n"
					  "            WHEN NOT t.typnotnull AND t.typdefault IS NOT NULL THEN 'default '||t.typdefault\n"
					  "            ELSE ''\n"
					  "       END as \"%s\",\n"
			"       pg_catalog.pg_get_constraintdef(r.oid, true) as \"%s\"\n"
					  "FROM pg_catalog.pg_type t\n"
	   "     LEFT JOIN pg_catalog.pg_namespace n ON n.oid = t.typnamespace\n"
		  "     LEFT JOIN pg_catalog.pg_constraint r ON t.oid = r.contypid\n"
					  "WHERE t.typtype = 'd'\n",
					  gettext_noop("Schema"),
					  gettext_noop("Name"),
					  gettext_noop("Type"),
					  gettext_noop("Modifier"),
					  gettext_noop("Check"));

	processSQLNamePattern(pset.db, &buf, pattern, true, false,
						  "n.nspname", "t.typname", NULL,
						  "pg_catalog.pg_type_is_visible(t.oid)");

	appendPQExpBuffer(&buf, "ORDER BY 1, 2;");

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	myopt.nullPrint = NULL;
	myopt.title = _("List of domains");
	myopt.trans_headers = true;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	PQclear(res);
	return true;
}

/*
 * \dc
 *
 * Describes conversions.
 */
bool
listConversions(const char *pattern)
{
	PQExpBufferData buf;
	PGresult   *res;
	printQueryOpt myopt = pset.popt;
	static const bool trans_columns[] = {false, false, false, false, true};

	initPQExpBuffer(&buf);

	printfPQExpBuffer(&buf,
					  "SELECT n.nspname AS \"%s\",\n"
					  "       c.conname AS \"%s\",\n"
	   "       pg_catalog.pg_encoding_to_char(c.conforencoding) AS \"%s\",\n"
		"       pg_catalog.pg_encoding_to_char(c.contoencoding) AS \"%s\",\n"
					  "       CASE WHEN c.condefault THEN '%s'\n"
					  "       ELSE '%s' END AS \"%s\"\n"
			   "FROM pg_catalog.pg_conversion c, pg_catalog.pg_namespace n\n"
					  "WHERE n.oid = c.connamespace\n",
					  gettext_noop("Schema"),
					  gettext_noop("Name"),
					  gettext_noop("Source"),
					  gettext_noop("Destination"),
					  gettext_noop("yes"), gettext_noop("no"),
					  gettext_noop("Default?"));

	processSQLNamePattern(pset.db, &buf, pattern, true, false,
						  "n.nspname", "c.conname", NULL,
						  "pg_catalog.pg_conversion_is_visible(c.oid)");

	appendPQExpBuffer(&buf, "ORDER BY 1, 2;");

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	myopt.nullPrint = NULL;
	myopt.title = _("List of conversions");
	myopt.trans_headers = true;
	myopt.trans_columns = trans_columns;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	PQclear(res);
	return true;
}

/*
 * \dC
 *
 * Describes casts.
 */
bool
listCasts(const char *pattern)
{
	PQExpBufferData buf;
	PGresult   *res;
	printQueryOpt myopt = pset.popt;
	static const bool trans_columns[] = {false, false, false, true};

	initPQExpBuffer(&buf);
	/*
	 * We need left join here for binary casts.  Also note that we don't
	 * attempt to localize '(binary compatible)', because there's too much
	 * risk of gettext translating a function name that happens to match
	 * some string in the PO database.
	 */
	printfPQExpBuffer(&buf,
			   "SELECT pg_catalog.format_type(castsource, NULL) AS \"%s\",\n"
			   "       pg_catalog.format_type(casttarget, NULL) AS \"%s\",\n"
					  "       CASE WHEN castfunc = 0 THEN '(binary compatible)'\n"
					  "            ELSE p.proname\n"
					  "       END as \"%s\",\n"
					  "       CASE WHEN c.castcontext = 'e' THEN '%s'\n"
					  "            WHEN c.castcontext = 'a' THEN '%s'\n"
					  "            ELSE '%s'\n"
					  "       END as \"%s\"\n"
				 "FROM pg_catalog.pg_cast c LEFT JOIN pg_catalog.pg_proc p\n"
					  "     ON c.castfunc = p.oid\n"
					  "ORDER BY 1, 2",
					  gettext_noop("Source type"),
					  gettext_noop("Target type"),
					  gettext_noop("Function"),
					  gettext_noop("no"), gettext_noop("in assignment"), gettext_noop("yes"),
					  gettext_noop("Implicit?"));

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	myopt.nullPrint = NULL;
	myopt.title = _("List of casts");
	myopt.trans_headers = true;
	myopt.trans_columns = trans_columns;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	PQclear(res);
	return true;
}

/*
 * \dn
 *
 * Describes schemas (namespaces)
 */
bool
listSchemas(const char *pattern, bool verbose)
{
	PQExpBufferData buf;
	PGresult   *res;
	printQueryOpt myopt = pset.popt;

	initPQExpBuffer(&buf);
	printfPQExpBuffer(&buf,
					  "SELECT n.nspname AS \"%s\",\n"
					  "       r.rolname AS \"%s\"",
					  gettext_noop("Name"),
					  gettext_noop("Owner"));

	if (verbose)
		appendPQExpBuffer(&buf,
						  ",\n  n.nspacl as \"%s\","
			 "  pg_catalog.obj_description(n.oid, 'pg_namespace') as \"%s\"",
						  gettext_noop("Access privileges"),
						  gettext_noop("Description"));

	appendPQExpBuffer(&buf,
			  "\nFROM pg_catalog.pg_namespace n JOIN pg_catalog.pg_roles r\n"
					  "       ON n.nspowner=r.oid\n"
					  "WHERE	(n.nspname !~ '^pg_temp_' OR\n"
		   "		 n.nspname = (pg_catalog.current_schemas(true))[1])\n");		/* temp schema is first */

	processSQLNamePattern(pset.db, &buf, pattern, true, false,
						  NULL, "n.nspname", NULL,
						  NULL);

	appendPQExpBuffer(&buf, "ORDER BY 1;");

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	myopt.nullPrint = NULL;
	myopt.title = _("List of schemas");
	myopt.trans_headers = true;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	PQclear(res);
	return true;
}


/*
 * \dFp
 * list text search parsers
 */
bool
listTSParsers(const char *pattern, bool verbose)
{
	PQExpBufferData buf;
	PGresult   *res;
	printQueryOpt myopt = pset.popt;

	if (verbose)
		return listTSParsersVerbose(pattern);

	initPQExpBuffer(&buf);

	printfPQExpBuffer(&buf,
					  "SELECT \n"
					  "  n.nspname as \"%s\",\n"
					  "  p.prsname as \"%s\",\n"
			"  pg_catalog.obj_description(p.oid, 'pg_ts_parser') as \"%s\"\n"
					  "FROM pg_catalog.pg_ts_parser p \n"
		   "LEFT JOIN pg_catalog.pg_namespace n ON n.oid = p.prsnamespace\n",
					  gettext_noop("Schema"),
					  gettext_noop("Name"),
					  gettext_noop("Description")
		);

	processSQLNamePattern(pset.db, &buf, pattern, false, false,
						  "n.nspname", "p.prsname", NULL,
						  "pg_catalog.pg_ts_parser_is_visible(p.oid)");

	appendPQExpBuffer(&buf, "ORDER BY 1, 2;");

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	myopt.nullPrint = NULL;
	myopt.title = _("List of text search parsers");
	myopt.trans_headers = true;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	PQclear(res);
	return true;
}

/*
 * full description of parsers
 */
static bool
listTSParsersVerbose(const char *pattern)
{
	PQExpBufferData buf;
	PGresult   *res;
	int			i;

	initPQExpBuffer(&buf);

	printfPQExpBuffer(&buf,
					  "SELECT p.oid, \n"
					  "  n.nspname, \n"
					  "  p.prsname \n"
					  "FROM pg_catalog.pg_ts_parser p\n"
			"LEFT JOIN pg_catalog.pg_namespace n ON n.oid = p.prsnamespace\n"
		);

	processSQLNamePattern(pset.db, &buf, pattern, false, false,
						  "n.nspname", "p.prsname", NULL,
						  "pg_catalog.pg_ts_parser_is_visible(p.oid)");

	appendPQExpBuffer(&buf, "ORDER BY 1, 2;");

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	if (PQntuples(res) == 0)
	{
		if (!pset.quiet)
			fprintf(stderr, _("Did not find any text search parser named \"%s\".\n"),
					pattern);
		PQclear(res);
		return false;
	}

	for (i = 0; i < PQntuples(res); i++)
	{
		const char *oid;
		const char *nspname = NULL;
		const char *prsname;

		oid = PQgetvalue(res, i, 0);
		if (!PQgetisnull(res, i, 1))
			nspname = PQgetvalue(res, i, 1);
		prsname = PQgetvalue(res, i, 2);

		if (!describeOneTSParser(oid, nspname, prsname))
		{
			PQclear(res);
			return false;
		}

		if (cancel_pressed)
		{
			PQclear(res);
			return false;
		}
	}

	PQclear(res);
	return true;
}

static bool
describeOneTSParser(const char *oid, const char *nspname, const char *prsname)
{
	PQExpBufferData buf;
	PGresult   *res;
	char		title[1024];
	printQueryOpt myopt = pset.popt;
	static const bool trans_columns[] = {true, false, false};

	initPQExpBuffer(&buf);

	printfPQExpBuffer(&buf,
					  "SELECT '%s' AS \"%s\", \n"
					  "   p.prsstart::pg_catalog.regproc AS \"%s\", \n"
		  "   pg_catalog.obj_description(p.prsstart, 'pg_proc') as \"%s\" \n"
					  " FROM pg_catalog.pg_ts_parser p \n"
					  " WHERE p.oid = '%s' \n"
					  "UNION ALL \n"
					  "SELECT '%s', \n"
					  "   p.prstoken::pg_catalog.regproc, \n"
					"   pg_catalog.obj_description(p.prstoken, 'pg_proc') \n"
					  " FROM pg_catalog.pg_ts_parser p \n"
					  " WHERE p.oid = '%s' \n"
					  "UNION ALL \n"
					  "SELECT '%s', \n"
					  "   p.prsend::pg_catalog.regproc, \n"
					  "   pg_catalog.obj_description(p.prsend, 'pg_proc') \n"
					  " FROM pg_catalog.pg_ts_parser p \n"
					  " WHERE p.oid = '%s' \n"
					  "UNION ALL \n"
					  "SELECT '%s', \n"
					  "   p.prsheadline::pg_catalog.regproc, \n"
				 "   pg_catalog.obj_description(p.prsheadline, 'pg_proc') \n"
					  " FROM pg_catalog.pg_ts_parser p \n"
					  " WHERE p.oid = '%s' \n"
					  "UNION ALL \n"
					  "SELECT '%s', \n"
					  "   p.prslextype::pg_catalog.regproc, \n"
				  "   pg_catalog.obj_description(p.prslextype, 'pg_proc') \n"
					  " FROM pg_catalog.pg_ts_parser p \n"
					  " WHERE p.oid = '%s' \n",
					  gettext_noop("Start parse"),
					  gettext_noop("Method"),
					  gettext_noop("Function"),
					  gettext_noop("Description"),
					  oid,
					  gettext_noop("Get next token"),
					  oid,
					  gettext_noop("End parse"),
					  oid,
					  gettext_noop("Get headline"),
					  oid,
					  gettext_noop("Get token types"),
					  oid);

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	myopt.nullPrint = NULL;
	if (nspname)
		sprintf(title, _("Text search parser \"%s.%s\""), nspname, prsname);
	else
		sprintf(title, _("Text search parser \"%s\""), prsname);
	myopt.title = title;
	myopt.footers = NULL;
	myopt.default_footer = false;
	myopt.trans_headers = true;
	myopt.trans_columns = trans_columns;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	PQclear(res);

	initPQExpBuffer(&buf);

	printfPQExpBuffer(&buf,
					  "SELECT t.alias as \"%s\", \n"
					  "  t.description as \"%s\" \n"
			  "FROM pg_catalog.ts_token_type( '%s'::pg_catalog.oid ) as t \n"
					  "ORDER BY 1;",
					  gettext_noop("Token name"),
					  gettext_noop("Description"),
					  oid);

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	myopt.nullPrint = NULL;
	if (nspname)
		sprintf(title, _("Token types for parser \"%s.%s\""), nspname, prsname);
	else
		sprintf(title, _("Token types for parser \"%s\""), prsname);
	myopt.title = title;
	myopt.footers = NULL;
	myopt.default_footer = true;
	myopt.trans_headers = true;
	myopt.trans_columns = NULL;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	PQclear(res);
	return true;
}


/*
 * \dFd
 * list text search dictionaries
 */
bool
listTSDictionaries(const char *pattern, bool verbose)
{
	PQExpBufferData buf;
	PGresult   *res;
	printQueryOpt myopt = pset.popt;

	initPQExpBuffer(&buf);

	printfPQExpBuffer(&buf,
					  "SELECT \n"
					  "  n.nspname as \"%s\",\n"
					  "  d.dictname as \"%s\",\n",
					  gettext_noop("Schema"),
					  gettext_noop("Name"));

	if (verbose)
	{
		appendPQExpBuffer(&buf,
						  "  ( SELECT COALESCE(nt.nspname, '(null)')::pg_catalog.text || '.' || t.tmplname FROM \n"
						  "    pg_catalog.pg_ts_template t \n"
						  "			 LEFT JOIN pg_catalog.pg_namespace nt ON nt.oid = t.tmplnamespace \n"
						  "			 WHERE d.dicttemplate = t.oid ) AS  \"%s\", \n"
						  "  d.dictinitoption as \"%s\", \n",
						  gettext_noop("Template"),
						  gettext_noop("Init options"));
	}

	appendPQExpBuffer(&buf,
			 "  pg_catalog.obj_description(d.oid, 'pg_ts_dict') as \"%s\"\n",
					  gettext_noop("Description"));

	appendPQExpBuffer(&buf, "FROM pg_catalog.pg_ts_dict d\n"
		 "LEFT JOIN pg_catalog.pg_namespace n ON n.oid = d.dictnamespace\n");

	processSQLNamePattern(pset.db, &buf, pattern, false, false,
						  "n.nspname", "d.dictname", NULL,
						  "pg_catalog.pg_ts_dict_is_visible(d.oid)");

	appendPQExpBuffer(&buf, "ORDER BY 1, 2;");

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	myopt.nullPrint = NULL;
	myopt.title = _("List of text search dictionaries");
	myopt.trans_headers = true;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	PQclear(res);
	return true;
}


/*
 * \dFt
 * list text search templates
 */
bool
listTSTemplates(const char *pattern, bool verbose)
{
	PQExpBufferData buf;
	PGresult   *res;
	printQueryOpt myopt = pset.popt;

	initPQExpBuffer(&buf);

	if (verbose)
		printfPQExpBuffer(&buf,
						  "SELECT \n"
						  "  n.nspname AS \"%s\",\n"
						  "  t.tmplname AS \"%s\",\n"
						  "  t.tmplinit::pg_catalog.regproc AS \"%s\",\n"
						  "  t.tmpllexize::pg_catalog.regproc AS \"%s\",\n"
		 "  pg_catalog.obj_description(t.oid, 'pg_ts_template') AS \"%s\"\n",
						  gettext_noop("Schema"),
						  gettext_noop("Name"),
						  gettext_noop("Init"),
						  gettext_noop("Lexize"),
						  gettext_noop("Description"));
	else
		printfPQExpBuffer(&buf,
						  "SELECT \n"
						  "  n.nspname AS \"%s\",\n"
						  "  t.tmplname AS \"%s\",\n"
		 "  pg_catalog.obj_description(t.oid, 'pg_ts_template') AS \"%s\"\n",
						  gettext_noop("Schema"),
						  gettext_noop("Name"),
						  gettext_noop("Description"));

	appendPQExpBuffer(&buf, "FROM pg_catalog.pg_ts_template t\n"
		 "LEFT JOIN pg_catalog.pg_namespace n ON n.oid = t.tmplnamespace\n");

	processSQLNamePattern(pset.db, &buf, pattern, false, false,
						  "n.nspname", "t.tmplname", NULL,
						  "pg_catalog.pg_ts_template_is_visible(t.oid)");

	appendPQExpBuffer(&buf, "ORDER BY 1, 2;");

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	myopt.nullPrint = NULL;
	myopt.title = _("List of text search templates");
	myopt.trans_headers = true;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	PQclear(res);
	return true;
}


/*
 * \dF
 * list text search configurations
 */
bool
listTSConfigs(const char *pattern, bool verbose)
{
	PQExpBufferData buf;
	PGresult   *res;
	printQueryOpt myopt = pset.popt;

	if (verbose)
		return listTSConfigsVerbose(pattern);

	initPQExpBuffer(&buf);

	printfPQExpBuffer(&buf,
					  "SELECT \n"
					  "   n.nspname as \"%s\",\n"
					  "   c.cfgname as \"%s\",\n"
		   "   pg_catalog.obj_description(c.oid, 'pg_ts_config') as \"%s\"\n"
					  "FROM pg_catalog.pg_ts_config c\n"
		  "LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.cfgnamespace \n",
					  gettext_noop("Schema"),
					  gettext_noop("Name"),
					  gettext_noop("Description")
		);

	processSQLNamePattern(pset.db, &buf, pattern, false, false,
						  "n.nspname", "c.cfgname", NULL,
						  "pg_catalog.pg_ts_config_is_visible(c.oid)");

	appendPQExpBuffer(&buf, "ORDER BY 1, 2;");

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	myopt.nullPrint = NULL;
	myopt.title = _("List of text search configurations");
	myopt.trans_headers = true;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	PQclear(res);
	return true;
}

static bool
listTSConfigsVerbose(const char *pattern)
{
	PQExpBufferData buf;
	PGresult   *res;
	int			i;

	initPQExpBuffer(&buf);

	printfPQExpBuffer(&buf,
					  "SELECT c.oid, c.cfgname,\n"
					  "   n.nspname, \n"
					  "   p.prsname, \n"
					  "   np.nspname as pnspname \n"
					  "FROM pg_catalog.pg_ts_config c \n"
	   "   LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.cfgnamespace, \n"
					  " pg_catalog.pg_ts_parser p \n"
	  "   LEFT JOIN pg_catalog.pg_namespace np ON np.oid = p.prsnamespace \n"
					  "WHERE  p.oid = c.cfgparser\n"
		);

	processSQLNamePattern(pset.db, &buf, pattern, true, false,
						  "n.nspname", "c.cfgname", NULL,
						  "pg_catalog.pg_ts_config_is_visible(c.oid)");

	appendPQExpBuffer(&buf, "ORDER BY 3, 2;");

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	if (PQntuples(res) == 0)
	{
		if (!pset.quiet)
			fprintf(stderr, _("Did not find any text search configuration named \"%s\".\n"),
					pattern);
		PQclear(res);
		return false;
	}

	for (i = 0; i < PQntuples(res); i++)
	{
		const char *oid;
		const char *cfgname;
		const char *nspname = NULL;
		const char *prsname;
		const char *pnspname = NULL;

		oid = PQgetvalue(res, i, 0);
		cfgname = PQgetvalue(res, i, 1);
		if (!PQgetisnull(res, i, 2))
			nspname = PQgetvalue(res, i, 2);
		prsname = PQgetvalue(res, i, 3);
		if (!PQgetisnull(res, i, 4))
			pnspname = PQgetvalue(res, i, 4);

		if (!describeOneTSConfig(oid, nspname, cfgname, pnspname, prsname))
		{
			PQclear(res);
			return false;
		}

		if (cancel_pressed)
		{
			PQclear(res);
			return false;
		}
	}

	PQclear(res);
	return true;
}

static bool
describeOneTSConfig(const char *oid, const char *nspname, const char *cfgname,
					const char *pnspname, const char *prsname)
{
	PQExpBufferData buf,
				title;
	PGresult   *res;
	printQueryOpt myopt = pset.popt;

	initPQExpBuffer(&buf);

	printfPQExpBuffer(&buf,
					  "SELECT \n"
					  "  ( SELECT t.alias FROM \n"
					  "    pg_catalog.ts_token_type(c.cfgparser) AS t \n"
					  "    WHERE t.tokid = m.maptokentype ) AS \"%s\", \n"
					  "  pg_catalog.btrim( \n"
				  "    ARRAY( SELECT mm.mapdict::pg_catalog.regdictionary \n"
					  "           FROM pg_catalog.pg_ts_config_map AS mm \n"
					  "           WHERE mm.mapcfg = m.mapcfg AND mm.maptokentype = m.maptokentype \n"
					  "           ORDER BY mapcfg, maptokentype, mapseqno \n"
					  "    ) :: pg_catalog.text , \n"
					  "  '{}') AS \"%s\" \n"
	 "FROM pg_catalog.pg_ts_config AS c, pg_catalog.pg_ts_config_map AS m \n"
					  "WHERE c.oid = '%s' AND m.mapcfg = c.oid \n"
					  "GROUP BY m.mapcfg, m.maptokentype, c.cfgparser \n"
					  "ORDER BY 1",
					  gettext_noop("Token"),
					  gettext_noop("Dictionaries"),
					  oid);

	res = PSQLexec(buf.data, false);
	termPQExpBuffer(&buf);
	if (!res)
		return false;

	initPQExpBuffer(&title);

	if (nspname)
		appendPQExpBuffer(&title, _("Text search configuration \"%s.%s\""),
						  nspname, cfgname);
	else
		appendPQExpBuffer(&title, _("Text search configuration \"%s\""),
						  cfgname);

	if (pnspname)
		appendPQExpBuffer(&title, _("\nParser: \"%s.%s\""),
						  pnspname, prsname);
	else
		appendPQExpBuffer(&title, _("\nParser: \"%s\""),
						  prsname);

	myopt.nullPrint = NULL;
	myopt.title = title.data;
	myopt.footers = NULL;
	myopt.default_footer = false;
	myopt.trans_headers = true;

	printQuery(res, &myopt, pset.queryFout, pset.logfile);

	termPQExpBuffer(&title);

	PQclear(res);
	return true;
}
