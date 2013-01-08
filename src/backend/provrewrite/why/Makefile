#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for backend/provrewrite/wherecs
#
# IDENTIFICATION
#    $PostgreSQL: pgsql/src/backend/provrewrite/why/Makefile,v 1.19 2008/04/22 21:34:11 bglav Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/provrewrite/why
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

OBJS = prov_why_main.o

all: SUBSYS.o

SUBSYS.o: $(OBJS)
	$(LD) $(LDREL) $(LDOUT) SUBSYS.o $(OBJS)

clean: 
	rm -f SUBSYS.o $(OBJS) 

