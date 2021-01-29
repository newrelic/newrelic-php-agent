#
# Copyright 2020 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#

#
#Build args passed from GHA
#
ARG PHP_VER
ARG ARCH
ARG BUILD_TYPE

FROM i386/php:${PHP_VER}

RUN docker-php-source extract

#
# Uncomment deb-src lines for all enabled repos. First part of single-quoted
# string (up the the !) is the pattern of the lines that will be ignored.
# Needed for apt-get build-dep call later in script
#
RUN sed -Ei '/.*partner/! s/^# (deb-src .*)/\1/g' /etc/apt/sources.list

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get install -y build-essential 

#
# PHP dependencies
#
RUN apt-get update \
 && apt-get -y install gcc git netcat wget unzip \
 libpcre3 libpcre3-dev golang psmisc automake libtool \
 insserv procps vim ${PHP_USER_SPECIFIED_PACKAGES}

RUN apt-get install -y default-libmysqlclient-dev libmcrypt-dev

#
# Other tools
#

RUN apt-get install -y curl gdb libcurl4-openssl-dev pkg-config postgresql libpq-dev libedit-dev libreadline-dev git

#
# Install basic packages.
#
RUN apt-get update && apt-get install -y \
  autoconf \
  autotools-dev \
  golang \
  valgrind \
  libc6 \
  libc6-dbg \
  libc6-dev \
  libgtest-dev \
  libtool \
  make \
  perl \
  strace \
  python-dev \
  python-setuptools \
  python-yaml \
  python3-argon2 \
  sqlite3 \
  libsqlite3-dev \
  libghc-argon2-dev \openssl \
  libxml2 \
  libxml2-dev \
  libonig-dev \
  libssl-dev \
  zip && apt-get clean

#
# C++ dependencies
#
RUN apt-get update && apt-get -y install libgflags-dev libgtest-dev libc++-dev clang && apt-get clean

#
# Install packages for 32-bit compilation
#
RUN apt-get update && apt-get -y install gcc gcc-multilib g++ g++-multilib && apt-get clean

RUN apt-get update

RUN nproc

#
#These args need to be repeated so we can propogate the VARS within this build context.
#

ARG PHP_VER
ARG ARCH
ARG BUILD_TYPE
ENV PHP_VER=${PHP_VER}
ENV ARCH=$ARCH
ENV BUILD_TYPE=$BUILD_TYPE

COPY /.github/docker/linux/release_build.sh /release_build.sh
COPY /.github/docker/linux/pr_build.sh /pr_build.sh

#
#Use shell form of ENTRYPOINT to pass the env variable.
#
ENTRYPOINT /${BUILD_TYPE}_build.sh
