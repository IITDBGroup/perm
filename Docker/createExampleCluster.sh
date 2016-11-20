#!/bin/bash
echo ---- Configure Perm installation and create TestDatabase
PGUSER=$1
PERMHOME=/home/perm
SRCDIR=${PERMHOME}/src
INSTALLBINDIR=${PERMHOME}/install/bin
DATADIR=${PERMHOME}/datadir
####################
echo -Create cluster, DB user and testdb database
mkdir -p ${DATADIR}
${INSTALLBINDIR}/initdb -D ${DATADIR}
####################
echo -start server
${INSTALLBINDIR}/postgres -D ${DATADIR} -l ${DATADIR}/log.txt &
####################
echo -create user and testdb
${INSTALLBINDIR}/createuser -s -l -U $PGUSER postgres 
${INSTALLBINDIR}/psql -U postgres -c 'CREATE LANGUAGE plpgsql' template1
${INSTALLBINDIR}/psql -U postgres -c '\i /home/perm/src/contrib/xml2/pgxml.sql' template1
${INSTALLBINDIR}/createdb -U $PGUSER testdb
