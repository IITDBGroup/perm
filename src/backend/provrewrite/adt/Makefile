#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for backend/provrewrite/adt
#
# IDENTIFICATION
#    $PostgreSQL: pgsql/src/backend/provrewrite/adt/Makefile,v 1.19 2008/08/31 16:34:11 bglav Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/provrewrite/adt
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

OBJS = prov_trans_bitset.o

all: SUBSYS.o

SUBSYS.o: $(OBJS)
	$(LD) $(LDREL) $(LDOUT) SUBSYS.o $(OBJS)

clean: 
	rm -f SUBSYS.o $(OBJS) 

