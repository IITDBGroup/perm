#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for backend/metaq
#
# IDENTIFICATION
#    $PostgreSQL: pgsql/src/backend/metaq/Makefile,v 1.19 2008/04/22 21:34:11 bglav Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/metaq
top_builddir = ../../../
include $(top_builddir)/src/Makefile.global

OBJS = outxmlfuncs.o xmlqtree.o parse_metaq.o

all: SUBSYS.o

SUBSYS.o: $(OBJS)
	$(LD) $(LDREL) $(LDOUT) SUBSYS.o $(OBJS)

clean: 
	rm -f SUBSYS.o $(OBJS) 

