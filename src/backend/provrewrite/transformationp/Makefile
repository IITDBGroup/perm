#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for backend/provrewrite/transprovp
#
# IDENTIFICATION
#    $PostgreSQL: pgsql/src/backend/provrewrite/transprovp/Makefile,v 1.19 2009/07/30 21:34:11 bglav Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/provrewrite/transformationp
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

OBJS = prov_trans_main.o prov_trans_staticana.o prov_trans_set.o prov_trans_aggr.o \
	prov_trans_util.o prov_trans_parse_back.o prov_trans_parse_back_xml.o \
	prov_trans_map.o

all: SUBSYS.o

SUBSYS.o: $(OBJS)
	$(LD) $(LDREL) $(LDOUT) SUBSYS.o $(OBJS)

clean: 
	rm -f SUBSYS.o $(OBJS) 

