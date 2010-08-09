/*-------------------------------------------------------------------------
 *
 * provlog.h
 *		External interface to the provenance logging utils
 *
 *
 * Portions Copyright (c) 2008 Boris Glavic
 * $PostgreSQL: pgsql/src/include/provrewrite/provlog.h,v 1.29 2008/01/01 19:45:58 bglav Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef PROVLOG_H_
#define PROVLOG_H_

#include "utils/elog.h"
#include "utils/guc.h"
#include "utils/rel.h"
#include "nodes/parsenodes.h"

#define LOGDEBUG(message) \
		do { \
			if (log_min_messages <= DEBUG1) \
				logDebug(message); \
		} while (0)
#define LOGNOTICE(message) \
		do { \
			if (log_min_messages <= NOTICE) \
				logDebug(message); \
		} while (0)
#define LOGNODE(node,message) \
		do { \
			if (log_min_messages <= DEBUG1) \
				logNode(node, message); \
		} while (0)
#define LOGNODEXML(node) \
		do { \
			if (log_min_messages <= DEBUG1) \
				logNodeXml(node); \
		} while (0)

extern void logNode (void *node, char *message);
extern void logNodeXml (void *node);
extern void logDebug (char *message);
extern void logNotice (char *message);
extern void logPList (List *pList);
extern void logListLength (List *list);
extern void logCharList (List *list);
extern void logValueStringList (List *list);
extern void logIntList (List *list);
extern void logQuerySql (Query *query);
extern void logQuerySqlDb2 (Query *query);
extern char *bitsetToString (Datum set);

#endif /*PROVLOG_H_*/
