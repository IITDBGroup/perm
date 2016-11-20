#!/bin/bash
PGUSER=$1
PERMHOME=/home/perm
SRCDIR=${PERMHOME}/src
INSTALLBINDIR=${PERMHOME}/install/bin
DATADIR=${PERMHOME}/datadir
####################
echo ---- Configure Perm installation and create TestDatabase
echo -- for user ${PGUSER}
echo -Create cluster, DB user and testdb database
mkdir -p ${DATADIR}
${INSTALLBINDIR}/initdb -D ${DATADIR}
####################
echo -start server
#${INSTALLBINDIR}/postgres -D ${DATADIR} -l ${DATADIR}/log.txt
${INSTALLBINDIR}/pg_ctl -w start
${INSTALLBINDIR}/psql -h localhost -p 5432 -U perm -d template1 -c 'CREATE LANGUAGE plpgsql' 
${INSTALLBINDIR}/psql -h localhost -p 5432 -U perm -d template1 -c '\i /home/perm/src/contrib/xml2/pgxml.sql'
####################
echo -create user and testdb
${INSTALLBINDIR}/createuser -s -l -U $PGUSER postgres 
${INSTALLBINDIR}/createdb -U $PGUSER testdb
