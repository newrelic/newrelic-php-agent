#
# Copyright 2020 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#

#
# ARGs passed from GHA workflow.
#

ARG PHP_VER

FROM php:${PHP_VER:-8.0}

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
  python3-yaml \
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
      export GOPATH="${HOME}/go"; \
      export PATH="${GOPATH}/bin:${GOROOT}/bin:${PATH}"; \
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
# The explain plan in the sql tests contain partition/filtered properties
# but the tests can't currently be updated  to reflect that since the sql
# database on our CI doesn't include those values.
# RUN docker-php-ext-install pdo pdo_mysql
#

RUN docker-php-ext-install mysqli

# redis
RUN pecl install redis && docker-php-ext-enable redis

# predis
#ENV PREDIS_VERSION=1.1.8
#RUN docker-php-source extract \
#  && mkdir -p /usr/src/php/ext/predis \
#  && curl -fsSL https://github.com/predis/predis/archive/refs/tags/v${PREDIS_VERSION}.tar.gz | tar xvz -C /usr/src/php/ext/predis --strip 1 \
#  && docker-php-ext-install predis \
#  && docker-php-source delete

# memcache
RUN pecl install memcache && docker-php-ext-enable memcache

# memcached
RUN apt-get install -y libmemcached-dev
RUN pecl install memcached && docker-php-ext-enable memcached

# mongoclient now known as mongo?
#RUN pecl install mongo && docker-php-ext-enable mongo

# mongodb
#RUN pecl install mongodb && docker-php-ext-enable mongodb

#
# These args need to be repeated so we can propagate the VARS within this build context.
#
ARG PHP_VER
ENV PHP_VER=${PHP_VER}
ARG PS1
ENV PS1="New Relic > "

WORKDIR /usr/src/myapp
CMD ["bash"]
