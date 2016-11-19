########################################
# Derived from ubuntu distribution
########################################
FROM ubuntu:latest
MAINTAINER Boris Glavic <bglavic@iit.edu>
########################################
# Create working directories
########################################
WORKDIR /perm
WORKDIR datadir
########################################
# Install packages needed for building
########################################
RUN apt-get -y update && apt-get install -y \
	autotools-dev \
	autoconf \
	bison \
	flex \
	git \
	libsqlite3-dev \
	libtool \
	make \
	libreadline6 \
	libreadline6-dev \
	sqlite3 \
	gnuplot \
	&& rm -rf /var/lib/apt/lists/*
########################################
# Clone GProM github repository
########################################
RUN git clone https://github.com/IITDBGroup/perm.git /perm/src
########################################
# Build GProM
########################################
RUN cd /perm/src \
	&& ./configure --disable-oracle --disable-postgres --disable-java \
	&& make \
	&& make install
########################################
# Build GProM
########################################

########################################
# Per default start the gprom command line interface
########################################
ENTRYPOINT ["/usr/local/bin/gprom"]
########################################
# Per default connect to the example SQLite database
########################################
CMD ["-Psqlserializer", "sqlite", "-Pmetadata", "sqlite", "-db", "/gprom/src/examples/test.db"]
