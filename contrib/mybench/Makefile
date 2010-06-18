# $PostgreSQL: pgsql/contrib/mybench/Makefile,v 1.11 2010/03/16 glavicb $

# link against libpg
PG_LIBS = -L$(libdir) -lpq

# get libpg headers
PG_CPPFLAGS += -I$(top_srcdir)/src/interfaces/libpq/

# Objects
OBJS = mybench.o

# Program
PROGRAM = mybenchpsql

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/mybench
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
