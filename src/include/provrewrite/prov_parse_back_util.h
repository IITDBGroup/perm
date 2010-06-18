/*-------------------------------------------------------------------------
 *
 * prov_parse_back_util.h
 *		 :
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: /postgres/src/include/provrewrite/prov_parse_back_util.h,v 1.29 06.10.2009 14:02:56 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROV_PARSE_BACK_UTIL_H_
#define PROV_PARSE_BACK_UTIL_H_

#include "nodes/parsenodes.h"
#include "lib/stringinfo.h"
#include "nodes/plannodes.h"

/* data types */
/*
 * Each level of query context around a subtree needs a level of Var namespace.
 * A Var having varlevelsup=N refers to the N'th item (counting from 0) in
 * the current context's namespaces list.
 *
 * The rangetable is the list of actual RTEs from the query tree.
 *
 * For deparsing plan trees, we provide for outer and inner subplan nodes.
 * The tlists of these nodes are used to resolve OUTER and INNER varnos.
 */
typedef struct
{
	List	   *rtable;			/* List of RangeTblEntry nodes */
	Plan	   *outer_plan;		/* OUTER subplan, or NULL if none */
	Plan	   *inner_plan;		/* INNER subplan, or NULL if none */
} deparse_namespace;

/* consts and macros */

#define only_marker(rte)  ((rte)->inh ? "" : "ONLY ")

/* ----------
 * Pretty formatting constants
 * ----------
 */

/* Indent counts */
#define PRETTYINDENT_STD		8
#define PRETTYINDENT_JOIN	   13
#define PRETTYINDENT_JOIN_ON	(PRETTYINDENT_JOIN-PRETTYINDENT_STD)
#define PRETTYINDENT_VAR		4

/* Pretty flags */
#define PRETTYFLAG_PAREN		1
#define PRETTYFLAG_INDENT		2

/* macro to test if pretty action needed */
#define PRETTY_PAREN(context)	((context)->prettyFlags & PRETTYFLAG_PAREN)
#define PRETTY_INDENT(context)	((context)->prettyFlags & PRETTYFLAG_INDENT)

/* functions */
extern char *generate_function_name(Oid funcid, int nargs, Oid *argtypes);
extern char *generate_operator_name(Oid operid, Oid arg1, Oid arg2);
extern char *generate_relation_name(Oid relid);
extern const char *get_simple_binary_op_name(OpExpr *expr);
extern bool isSimpleNode(Node *node, Node *parentNode, int prettyFlags);
extern bool replaceUnnamedColumnsWalker (Node *node, void *context);
extern RangeTblEntry *find_rte_by_refname(const char *refname, List *namespaces);

#endif /* PROV_PARSE_BACK_UTIL_H_ */
