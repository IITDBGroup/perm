#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for backend/provrewrite/copysem
#
# IDENTIFICATION
#    $PostgreSQL: pgsql/src/backend/provrewrite/Makefile,v 1.19 2008/04/22 21:34:11 bglav Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/provrewrite/copysem
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

OBJS = prov_copy_spj.o prov_copy_map.o prov_copy_util.o prov_copy_agg.o \
		prov_copy_set.o prov_copy_inclattr.o

all: SUBSYS.o

SUBSYS.o: $(OBJS)
	$(LD) $(LDREL) $(LDOUT) SUBSYS.o $(OBJS)

clean: 
	rm -f SUBSYS.o $(OBJS) 

