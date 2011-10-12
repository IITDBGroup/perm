#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for backend/provrewrite
#
# IDENTIFICATION
#    $PostgreSQL: pgsql/src/backend/provrewrite/Makefile,v 1.19 2008/04/22 21:34:11 bglav Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/provrewrite/main
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

OBJS = provrewrite.o prov_set.o prov_aggr.o prov_spj.o prov_adaptsuper.o prov_plan_all.o prov_nodes.o

all: SUBSYS.o

SUBSYS.o: $(OBJS)
	$(LD) $(LDREL) $(LDOUT) SUBSYS.o $(OBJS)

clean:
	rm -f SUBSYS.o $(OBJS)
