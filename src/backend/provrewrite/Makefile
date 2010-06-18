#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for backend/provrewrite
#
# IDENTIFICATION
#    $PostgreSQL: pgsql/src/backend/provrewrite/Makefile,v 1.19 2008/04/22 21:34:11 bglav Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/provrewrite
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

SUBDIRS     = copysem output sublink main restr util transformationp adt
SUBDIROBJS  = $(SUBDIRS:%=%/SUBSYS.o)

all: SUBSYS.o

SUBSYS.o: $(SUBDIROBJS)
	$(LD) $(LDREL) $(LDOUT) $@ $^

$(SUBDIROBJS): $(SUBDIRS:%=%-recursive) ;

.PHONY: $(SUBDIRS:%=%-recursive)
$(SUBDIRS:%=%-recursive):
	$(MAKE) -C $(subst -recursive,,$@) SUBSYS.o

clean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir $@ || exit; done
	rm -f SUBSYS.o
