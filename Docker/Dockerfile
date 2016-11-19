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
	libtool \
	make \
	libreadline6-dev \
	zlib1g-dev \
	&& rm -rf /var/lib/apt/lists/*
########################################
# Clone Perm github repository
########################################
RUN git clone https://github.com/IITDBGroup/perm.git /perm/src
########################################
# Build Perm
########################################
RUN cd /perm/src \
	&& ./configure --prefix=/perm/install \
	&& make \
	&& make install
########################################
# Create Data Directory
########################################

########################################
# Per default start the gprom command line interface
########################################
ENTRYPOINT ["/perm/install/postmaster"]
########################################
# Per default connect to the example SQLite database
########################################
CMD ["-Psqlserializer", "sqlite", "-Pmetadata", "sqlite", "-db", "/gprom/src/examples/test.db"]
