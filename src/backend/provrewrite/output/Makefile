#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for backend/provrewrite
#
# IDENTIFICATION
#    $PostgreSQL: pgsql/src/backend/provrewrite/Makefile,v 1.19 2008/04/22 21:34:11 bglav Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/provrewrite/output
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

OBJS = prov_log.o prov_dotnode.o parse_back_db2.o prov_parse_back_util.o

all: SUBSYS.o

SUBSYS.o: $(OBJS)
	$(LD) $(LDREL) $(LDOUT) SUBSYS.o $(OBJS)

clean: 
	rm -f SUBSYS.o $(OBJS) 

