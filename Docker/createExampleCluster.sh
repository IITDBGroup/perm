#!/bin/bash
echo ---- Configure Perm installation and create TestDatabase
PGUSER=$1
####################
echo -Create cluster, DB user and testdb database
mkdir -p /perm/datadir
/perm/install/bin/initdb -D /perm/datadir
####################
echo -start server
/perm/install/bin/postmaster -D /perm/datadir > /perm/datadir/log.txt 2>&1 &
####################
echo -create user and testdb
/perm/install/bin/createuser -s -l -U $PGUSER postgres 
/perm/install/bin/psql -U postgres -c 'CREATE LANGUAGE plpgsql' template1
/perm/install/bin/psql -U postgres -c '\i /pathToPermCode/contrib/xml2/pgxml.sql' template1
/perm/install/bin/createdb -U $PGUSER testdb
