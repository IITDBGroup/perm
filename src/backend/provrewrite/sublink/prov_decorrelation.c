/*-------------------------------------------------------------------------
 *
 * prov_sublink_util.c
 *	  POSTGRES C general rewrites for sublink decorrelation used to optimized provenance rewrite
 *
 * Portions Copyright (c) 2008 Boris Glavic
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/provrewrite/prov_decorrelation.c,v 1.542 2008/01/26 19:55:08 bglav Exp $
 *
 * NOTES
 *
 * 
 *-------------------------------------------------------------------------
 */

//TODO which rewrites mess up the provenance?