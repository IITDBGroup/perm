#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for backend/provrewrite/wherecs
#
# IDENTIFICATION
#    $PostgreSQL: pgsql/src/backend/provrewrite/wherecs/Makefile,v 1.19 2008/04/22 21:34:11 bglav Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/provrewrite/wherecs
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

OBJS = prov_where_map.o prov_where_main.o prov_where_spj.o prov_where_set.o \
	 	prov_where_aggfunc.o prov_where_util.o

all: SUBSYS.o

SUBSYS.o: $(OBJS)
	$(LD) $(LDREL) $(LDOUT) SUBSYS.o $(OBJS)

clean: 
	rm -f SUBSYS.o $(OBJS) 

