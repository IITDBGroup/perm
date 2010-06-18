#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for backend/provrewrite
#
# IDENTIFICATION
#    $PostgreSQL: pgsql/src/backend/provrewrite/Makefile,v 1.19 2008/04/22 21:34:11 bglav Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/provrewrite/restr
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

OBJS = prov_restr_pushdown.o prov_restr_rewrite.o prov_restr_util.o prov_restr_ineq.o \
	prov_restr_scope.o prov_restr_final.o

all: SUBSYS.o

SUBSYS.o: $(OBJS)
	$(LD) $(LDREL) $(LDOUT) SUBSYS.o $(OBJS)

clean: 
	rm -f SUBSYS.o $(OBJS) 

