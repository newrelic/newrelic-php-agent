#
# Copyright 2020 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#

GIT ?= git

all: protobuf-c

.PHONY: clean
clean: protobuf-clean protobuf-c-clean
	rm -rf local

local:
	mkdir -p local

#
# protobuf
#
# This requires a C++ compiler, autoconf, and automake.
#
# We're only going to build the C++ components, since that's all we need.
#
.PHONY: protobuf-clean
protobuf: local/bin/protoc

local/bin/protoc: protobuf/Makefile
	$(MAKE) -C protobuf install

protobuf/configure: protobuf/autogen.sh protobuf/configure.ac protobuf/Makefile.am
	cd protobuf && ./autogen.sh

protobuf/Makefile: protobuf/configure
	cd protobuf && ./configure --prefix=`pwd`/../local --enable-static=yes --enable-shared=no --with-pic

protobuf-clean:
	cd protobuf && $(GIT) clean -fdx

#
# protobuf-c
#
# This requires a C++ compiler, autoconf, and automake.
#
.PHONY: protobuf-c-clean
protobuf-c: local/lib/libprotobuf-c.a

local/lib/libprotobuf-c.a: protobuf-c/Makefile local
	$(MAKE) -C protobuf-c install

protobuf-c/configure: protobuf-c/autogen.sh protobuf-c/configure.ac
	cd protobuf-c && ./autogen.sh

protobuf-c/Makefile: protobuf-c/configure protobuf
	cd protobuf-c && PKG_CONFIG_PATH=../local/lib/pkgconfig PROTOC=../local/bin/protoc ./configure --prefix=`pwd`/../local --enable-static=yes --enable-shared=no --disable-dependency-tracking --with-pic

protobuf-c-clean:
	cd protobuf-c && $(GIT) clean -fdx
