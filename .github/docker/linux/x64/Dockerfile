#
# Copyright 2020 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#

#
# ARGs passed from GHA workflow.
#
ARG PHP_VER

FROM php:${PHP_VER}

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
 && apt-get -y install gcc git netcat \
 libpcre3 libpcre3-dev golang psmisc automake libtool \
 insserv procps vim ${PHP_USER_SPECIFIED_PACKAGES} \
 zlib1g-dev libmcrypt-dev

#
# Other tools
#
RUN apt-get install -y gdb valgrind libcurl4-openssl-dev pkg-config libpq-dev libedit-dev libreadline-dev git

#
# Install other packages.
#
RUN apt-get update && apt-get install -y \
  autoconf \
  autotools-dev \
  build-essential \
  bzip2 \
  ccache \
  curl \
  dnsutils \
  git \
  gyp \
  lcov \
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
  sqlite3 \
  libsqlite3-dev \
  openssl \
  libxml2 \
  libxml2-dev \
  libonig-dev \
  libssl-dev \
  unzip \
  wget \
  zip && apt-get clean

#
# If the debian version is jessie or stretch, we need to install go manually;
# otherwise, we just install the golang package from the repository.
# go 1.11.6 matches the version that buster uses.
#
RUN if [ -z "$(grep '^10\.' /etc/debian_version)" ]; then \
      wget --quiet https://golang.org/dl/go1.11.6.linux-amd64.tar.gz -O- | tar -C /usr/local -zxvf -; \
      export GOROOT=/usr/local/go; \
      exportGOPATH="${HOME}/go"; \
      exportPATH="${GOPATH}/bin:${GOROOT}/bin:${PATH}"; \
    else \
      apt-get install -y golang; \
    fi

#
# If the debian version is jessie, don't install argon2
#
RUN if [ -z "$(grep '^8\.' /etc/debian_version)" ]; then \
    apt-get install -y argon2 libghc-argon2-dev; \
    fi

#
# These args need to be repeated so we can propagate the VARS within this build context.
#
ARG PHP_VER
ARG ARCH
ARG BUILD_TYPE
ENV PHP_VER=${PHP_VER}
ENV ARCH=$ARCH
ENV BUILD_TYPE=$BUILD_TYPE

COPY /.github/docker/linux/${BUILD_TYPE}_build.sh /build.sh

ENTRYPOINT ["/build.sh"]
