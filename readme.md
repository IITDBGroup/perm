# Build Status

[![Build Status](https://travis-ci.org/IITDBGroup/perm.svg?branch=master)](https://travis-ci.org/IITDBGroup/perm)

# Overview

Perm is an extended [PostgreSQL](https://www.postgresql.org/) server with support for capturing provenance of SQL queries on fly. Provenance is exposed though several SQL extensions. For an overview of the research behind Perm see [here](http://www.cs.iit.edu/%7edbgroup/research/perm.php#permstart).

# Usage

All ways of accessing PostgreSQL work for Perm as well. For instance, using the `psql` command line client. For example, to connect to a database `postgres` run:

~~~
psql postgres
~~~

Perm extends SQL with new language constructs for requesting provenance. If you add the `PROVENANCE` keyword after the `SELECT` keyword in a query than this instructs Perm to retrieve the provenance of this query, e.g.,

~~~
testdb=# SELECT * FROM x;
 a | b 
---+---
 1 | 1
 2 | 2
 3 | 3
 4 | 0
(4 rows)

testdb=# SELECT a FROM x;
 a 
---
 1
 2
 3
 4
(4 rows)

testdb=# SELECT PROVENANCE a FROM x;
 a | prov_public_x_a | prov_public_x_b 
---+-----------------+-----------------
 1 |               1 |               1
 2 |               2 |               2
 3 |               3 |               3
 4 |               4 |               0
(4 rows)

testdb=# 
~~~

# Docker

We provide a Perm installation as a [docker](https://www.docker.com/) container. To get this container run:

~~~
docker pull iitdbgroup/perm
~~~

To start the perm server and expose its port `5432` to the host:

~~~
docker run --name myperm -d -p 5432:5432 iitdbgroup/perm
~~~

either connect with any PostgreSQL client (if installed on the host) 

~~~
psql -U perm -d testdb -p 5432 -h localhost
~~~

or by running psql in the docker container itself:

~~~
docker exec -ti myperm /home/perm/install/bin/psql -U perm -d testdb
~~~



# Installation

We currently do not offer a precompiled binary. The following instructions explain how to install Perm from source. Perm is known to run on Linux, Mac OS X, and Windows. Here we just give an overview. For a more detailed version read the [INSTALL](https://github.com/IITDBGroup/perm/blob/master/INSTALL) file in the main source directory. Since Perm is a modified PostgreSQL 8.3 server, the installation process is almost the same as for PostgreSQL. This means the excellent [PostgreSQL documentation](https://www.postgresql.org/docs/8.3/static/index.html) is a good source for figuring out how to solve problems with the installation process and how to use the system.

## Required Packages

* Flex 2.5.4 or later
* Bison 1.875 or later
* libxslt
* libxml
* libz
* ISO/ANSI C Compiler preferably gcc
* GNU make



## Installation Steps

Clone the github repository

~~~
git clone https://github.com/IITDBGroup/perm.git
~~~

to install everything under `INSTALLDIR` and create a cluster (postgres term for a data directory) at `CLUSTERDIR` change to the main source code directory and run the following commands:

~~~
cd perm
~~~

~~~
./configure --with-libxml --with-libxslt --prefix=INSTALLDIR
make
make install
mkdir CLUSTERDIR
INSTALLDIR/bin/initdb -D CLUSTERDIR
INSTALLDIR/bin/postgres -D CLUSTERDIR >logfile 2>&1 &
INSTALLDIR/bin/createdb test
INSTALLDIR/bin/psql test
test=# CREATE LANGUAGE plpgsql;
test=# \i ./contrib/xml2/pgxml.sql
~~~

The first three steps configure, compile, and install the Perm binaries. The fourth and fifth step create a cluster (a place in the file system that postgres uses to store databases on disk). The sixth step starts the Perm server. The seventh and eighth step create a new database named `test` and connect to this database using the standard postgres client (`psql`). Finally, the last two steps install PL/PGSQL as a procedural language for this database and create several XML related functions.

