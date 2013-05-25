#!/bin/bash
cd $HOME
#################################################################
######### use apt get to fetch packages perm depends on #########
echo ---- Install necessary packages -----
sudo apt-get install subversion bison flex zlib1g-dev libreadline5-dev
#################################################################
######### fetch and build perm				#########
echo ---- Build Perm ------
echo -Checkout Perm code
svn checkout https://130.60.155.131/svn/postprov/postp $HOME/perm/permcode
echo -Call Perm configure
cd $HOME
./configure --prefix=$HOME/perm/install YACC='/usr/bin/bison -y'
echo -Call make clean and make all
make clean && make all
echo -Call make install
make install
#################################################################
######### fetch and build experimenter javabench	#########
echo ---- Get Experimenter application ----
echo -Check out Experimenter code
svn checkout https://130.60.155.131/svn/postprov/PermPerformanceTester $HOME/javabench
echo -Call Ant
cd $HOME/javabench
ant
#################################################################
######### create database cluster, perm shell scripts, ... ######
echo ---- Configure Perm installation and create TestDatabase
echo -Create cluster, DB user and testdb database
mkdir $HOME/perm/data
$HOME/perm/install/bin/initdb -D $HOME/perm/data
echo -start server
$HOME/perm/install/bin/postmaster -D $HOME/perm/data > $HOME/perm/data/log.txt 2>&1 &
echo -create user and testdb
$HOME/perm/install/bin/createuser -s -l -U $USER postgres 
$HOME/perm/install/bin/psql -U postgres -c 'CREATE LANGUAGE plpgsql' template1
$HOME/perm/install/bin/createdb -U $USER testdb
echo -Create perm convenience scripts for the user
if [ ! -d $HOME/bin ]
then
    mkdir $HOME/bin
fi
echo "#!/bin/bash" > $HOME/bin/permOn.sh
echo "$HOME/perm/install/bin/postmaster -D $HOME/perm/data > $HOME/perm/data/log.txt 2>&1 &" >> $HOME/bin/permOn.sh
chmod 700 $HOME/bin/permOn.sh

echo "#!/bin/bash" > $HOME/bin/permOff.sh
echo "head -n 1 /home/tas/perm/data/postmaster.pid | xargs kill" >> $HOME/bin/permOff.sh
chmod 700 $HOME/bin/permOff.sh

echo "#!/bin/bash" > $HOME/bin/permSQL.sh
echo "$HOME/perm/install/bin/psql -U postgres \$*" >> $HOME/bin/permSQL.sh
chmod 700 $HOME/bin/permSQL.sh

echo "#!/bin/bash" > $HOME/bin/isPerm.sh
echo "if [ -f $HOME/perm/data/postmaster.pid ]" >> $HOME/bin/isPerm.sh
echo "then" >> $HOME/bin/isPerm.sh
echo "	echo Perm server running" >> $HOME/bin/isPerm.sh
echo "else" >> $HOME/bin/isPerm.sh
echo "	echo Perm server stopped" >> $HOME/bin/isPerm.sh
echo "fi" >> $HOME/bin/isPerm.sh
chmod 700 $HOME/bin/isPerm.sh

#################################################################
######### load tpch benchmark database instances	#########
echo -Create empty databases
$HOME/perm/install/bin/createdb -U $USER tpch_0_001
$HOME/perm/install/bin/createdb -U $USER tpch_0_01
$HOME/perm/install/bin/createdb -U $USER tpch_0_1
$HOME/perm/install/bin/createdb -U $USER tpch_1
echo -Get TCP-H data files
scp -r -P 2222 dbadmin@130.60.155.131:/home/dbadmin/tpchdata $HOME/tpchdata
echo -modfiy scripts for current user
sudo ln -s $HOME /home/boris
echo -execute scripts
echo 1MB
permSQL.sh -f $HOME/tpchdata/ddl_0_001.sql tpch_0_001
echo 10MB
permSQL.sh -f $HOME/tpchdata/ddl_0_01.sql tpch_0_01
echo 100MB
permSQL.sh -f $HOME/tpchdata/ddl_0_1.sql tpch_0_1
echo 1000MB
permSQL.sh -f $HOME/tpchdata/ddl_1.sql tpch_1
sudo rm /home/boris
$/HOME/perm/install/bin/psql -c "VACUUM FULL ANALYZE;" -d tpch_0_001
$/HOME/perm/install/bin/psql -c "VACUUM FULL ANALYZE;" -d tpch_0_01
$/HOME/perm/install/bin/psql -c "VACUUM FULL ANALYZE;" -d tpch_0_1
$/HOME/perm/install/bin/psql -c "VACUUM FULL ANALYZE;" -d tpch_1

#################################################################
######### load mybench app				#########
echo ----- Install mybench app ----
svn checkout https://130.60.155.131/svn/postprov/mybenchpsql $HOME/mybench/mybenchcode
cd $HOME/mybench/mybenchcode/Debug
gcc -I$HOME/perm/install/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF src/mybench.d  -MT src/mybench.d  -o src/mybench.o  ../src/mybench.c
gcc -L$HOME/perm/install/lib -o mybenchpsql ./src/mybench.o -lpq
cp mybenchpsql $HOME/bin/mybenchpsql

#################################################################
######### load db2 install image			#########
echo -----  Install DB2 server ----
echo - Load install tarball
mkdir $HOME/db2inst
scp -P 2222 dbadmin@130.60.155.131:/home/dbadmin/db2exc_970_LNX_x86.tar.gz $HOME/db2inst/
tar -xvzf $HOME/db2inst/db2exc_970_LNX_x86.tar.gz
echo - Install required packages
sudo apt-get install libstdc++5 libaio-dev
echo - set SHMMAX and SHMALL
sudo cp /etc/sysctl.conf /etc/sysctl.conf.old
sudo cp /etc/sysctl.conf /etc/sysctl.conf.new
sudo chown $USER /etc/sysctl.conf.new
sudo echo "kernel.shmmax = 2147483648" >> /etc/sysctl.conf.new
sudo echo "kernel.shmall = 2097152" >> /etc/sysctl.conf.new
sudo cp /etc/sysctl.conf.new /etc/sysctl.conf
sudo sysctl -p
sudo rm /etc/sysctl.conf.new
echo "- Start installer manually und $HOME/db2inst/expc/ with sudo ./db2setup"

#################################################################
######### load db2 tpch					#########
echo "---- db2 postinstall steps ---"
echo "- set shell for db2 users"
sudo cp /etc/passwd /etc/passwd.old
sed -e '/d.*/s/\/bin\/sh/\/bin\/bash/g' < /etc/passwd > $HOME/passwd
sudo cp $HOME/passwd /etc/passwd
rm $HOME/passwd

echo "- enable TPCH access"
#todo check for entry in /etc/services
sudo -u db2inst1 db2 update dbm cfg using SVCENAME db2c_db2inst1
sudo -u db2inst1 db2set DB2COMM=tcpip
sudo -u db2inst1 db2stop
sudo -u db2inst1 db2start

echo "- create scripts for db2"

echo "#!/bin/bash" > $HOME/bin/isDB2.sh
echo 'test=`ps -Al | grep db2sys | wc -l`' >> $HOME/bin/isDB2.sh
echo "if [ \$test -gt 0 ] " >> $HOME/bin/isDB2.sh
echo "then" >> $HOME/bin/isDB2.sh
echo "	echo running" >> $HOME/bin/isDB2.sh
echo "else" >> $HOME/bin/isDB2.sh
echo "	echo finished" >> $HOME/bin/isDB2.sh
echo "fi" >> $HOME/bin/isDB2.sh
chmod 700 $HOME/bin/isDB2.sh

echo "- load tpch "
#su db2inst1
db2 "CREATE DATABASE tpch0001"
db2 "CREATE DATABASE tpch001"
db2 "CREATE DATABASE tpch01"
db2 "CREATE DATABASE tpch1"

db2 connect to tpch0001 && db2 -tvf /home/tas/tpchdata/ddl_0_001_db2.sql
db2 connect to tpch001 && db2 -tvf /home/tas/tpchdata/ddl_0_01_db2.sql
db2 connect to tpch01 && db2 -tvf /home/tas/tpchdata/ddl_0_1_db2.sql
db2 connect to tpch1 && db2 -tvf /home/tas/tpchdata/ddl_1_db2.sql

echo "- update db2 statistics"

db2 connect to tpch0001
db2 REORGCHK UPDATE STATISTICS on TABLE ALL
db2 RUNSTATS ON TABLE db2inst1.nation WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.region WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.orders WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.lineitem WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.customer WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.part WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.partsupp WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.supplier WITH DISTRIBUTION AND INDEXES ALL

db2 connect to tpch001
db2 REORGCHK UPDATE STATISTICS on TABLE ALL
db2 RUNSTATS ON TABLE db2inst1.nation WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.region WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.orders WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.lineitem WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.customer WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.part WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.partsupp WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.supplier WITH DISTRIBUTION AND INDEXES ALL

db2 connect to tpch01
db2 REORGCHK UPDATE STATISTICS on TABLE ALL
db2 RUNSTATS ON TABLE db2inst1.nation WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.region WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.orders WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.lineitem WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.customer WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.part WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.partsupp WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.supplier WITH DISTRIBUTION AND INDEXES ALL

db2 connect to tpch1
db2 REORGCHK UPDATE STATISTICS on TABLE ALL
db2 RUNSTATS ON TABLE db2inst1.nation WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.region WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.orders WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.lineitem WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.customer WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.part WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.partsupp WITH DISTRIBUTION AND INDEXES ALL
db2 RUNSTATS ON TABLE db2inst1.supplier WITH DISTRIBUTION AND INDEXES ALL
db2 disconnect tpch1

echo "- configure db2 parameter settings"
db2 connect to tpch0001
db2 autoconfigure using mem_percent 75 workload_type complex num_stmts 1 tpm 1 is_populated yes num_local_apps 2 num_remote_apps 2 admin_priority performance isolation UR bp_resizeable yes APPLY DB AND DBM
db2 connect to tpch001
db2 autoconfigure using mem_percent 75 workload_type complex num_stmts 1 tpm 1 is_populated yes num_local_apps 2 num_remote_apps 2 admin_priority performance isolation UR bp_resizeable yes APPLY DB AND DBM
db2 connect to tpch01
db2 autoconfigure using mem_percent 75 workload_type complex num_stmts 1 tpm 1 is_populated yes num_local_apps 2 num_remote_apps 2 admin_priority performance isolation UR bp_resizeable yes APPLY DB AND DBM
db2 connect to tpch1
db2 autoconfigure using mem_percent 75 workload_type complex num_stmts 1 tpm 1 is_populated yes num_local_apps 2 num_remote_apps 2 admin_priority performance isolation UR bp_resizeable yes APPLY DB AND DBM
db2 disconnect tpch1

echo "- disable auto maintainance"
db2 update dbm cfg using INTRA_PARALLEL YES
db2 update dbm cfg using MAX_QUERYDEGREE -1
db2 connect to tpch0001
db2 update db cfg using AUTO_MAINT OFF
db2 update db cfg using DFT_QUERYOPT 9
db2 connect to tpch001
db2 update db cfg using AUTO_MAINT OFF
db2 update db cfg using DFT_QUERYOPT 9
db2 connect to tpch01
db2 update db cfg using AUTO_MAINT OFF
db2 update db cfg using DFT_QUERYOPT 9
db2 connect to tpch1
db2 update db cfg using AUTO_MAINT OFF
db2 update db cfg using DFT_QUERYOPT 9
db2 disconnect tpch1

db2stop
db2start

#TODO add query opt

