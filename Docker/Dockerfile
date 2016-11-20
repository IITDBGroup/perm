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
	libxml2 \
	libxml2-dev \
	libxslt1-dev \
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
	&& ./configure --prefix=/perm/install --enable-thread-safety --with-libxml --with-libxslt  CFLAGS='-O0' \
	&& make all \
	&& make install
########################################
# Create Data Directory
########################################
RUN /perm/src/Docker/createExampleCluster.sh root
########################################
# Expose postmaster port
########################################
EXPOSE 5432
########################################
# Per default start the gprom command line interface
########################################
ENTRYPOINT ["/perm/install/bin/postmaster"]
########################################
# Per default connect to the example SQLite database
########################################
CMD ["-D", "/perm/datadir"]
