#!/bin/bash

# MAC OS X
if [[ $TRAVIS_OS_NAME == 'osx' ]]; then
	# Directory this script resides in
	pushd $(dirname "${0}") > /dev/null
	DIR=$(pwd -L)
	popd > /dev/null
	# install dependencies
	brew update
	brew outdated bison || brew upgrade bison
	brew outdated flex || brew upgrade flex
	brew outdated git || brew upgrade git
	brew outdated libxml2 || brew upgrade libxml2
	brew outdated libxslt || brew upgrade libxslt
	brew outdated readline || brew upgrade readline
# LINUX	
else
	sudo apt-get -qq update
	apt-get install -y \
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
			zlib1g-dev
fi
