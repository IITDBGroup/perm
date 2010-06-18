#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for backend/provrewrite
#
# IDENTIFICATION
#    $PostgreSQL: pgsql/src/backend/provrewrite/Makefile,v 1.19 2008/04/22 21:34:11 bglav Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/provrewrite/sublink
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

OBJS = prov_sublink.o  prov_sublink_keepcor.o prov_sublink_agg.o \
	prov_sublink_leftjoin.o prov_sublink_totarget.o prov_sublink_unnest.o prov_sublink_unn.o \
	prov_sublink_util_analyze.o prov_sublink_util_search.o prov_sublink_util_mutate.o

all: SUBSYS.o

SUBSYS.o: $(OBJS)
	$(LD) $(LDREL) $(LDOUT) SUBSYS.o $(OBJS)

clean: 
	rm -f SUBSYS.o $(OBJS) 

