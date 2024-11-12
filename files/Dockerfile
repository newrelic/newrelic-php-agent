#
# Copyright 2020 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#

#
# ARGs passed from GHA workflow.
#

ARG PHP_VER

FROM php:${PHP_VER:-8.3}

RUN docker-php-source extract

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y build-essential

#
# PHP dependencies
#
RUN apt-get update \
 && apt-get -y install gcc git netcat-openbsd \
 libpcre3 libpcre3-dev psmisc automake libtool \
 insserv procps vim ${PHP_USER_SPECIFIED_PACKAGES} \
 zlib1g-dev libmcrypt-dev

# pgsql extension
RUN apt-get install -y libpq-dev
RUN docker-php-ext-install pgsql

#
# Other tools
#
RUN apt-get install -y gdb valgrind libcurl4-openssl-dev pkg-config libpq-dev libedit-dev libreadline-dev git

#
# Install other packages.
#
RUN apt-get update && apt-get install -y \
  autoconf \
  argon2 libghc-argon2-dev \
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
  locales \
  locales-all \
  make \
  perl \
  strace \
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

# install latest go to work with the daemon
COPY --from=golang /usr/local/go /usr/local/go
ENV PATH /usr/local/go/bin:$PATH

#
# If the debian version is buster, don't install python-dev-is-python3
#
RUN if [ -z "$(grep '^10\.' /etc/debian_version)" ]; then \
    apt-get install -y python-dev-is-python3; \
    fi

# install composer
COPY --from=composer ["/usr/bin/composer", "/usr/bin/composer"]

#
# The explain plan in the sql tests contain partition/filtered properties
# but the tests can't currently be updated  to reflect that since the sql
# database on our CI doesn't include those values.
#
# Further explanation:
#   mysql tests are written to work with mysql 5.6 (or earlier?) server
#   mysql server 5.7+ have additional columns (as mentioned above) which
#   will cause the current mysql tests to fail.
#
# arm64 notes:
#   mysql server 8.0 is currently the only version available for arm64
#   and 8.0 has problems with how the explanation for informational_schema
#   work (refer to bug https://bugs.mysql.com/bug.php?id=102536) so to run
#   the mysql tests a separate machine running mysql server 5.6 is required.
RUN docker-php-ext-install pdo pdo_mysql

# install redis extension required by test_redis:
RUN \
  php_cmp=$(php -r "echo version_compare(PHP_VERSION, '7.4.0', '>=');"); \
  if [ "$php_cmp" = 1 ]; then \
    # install latest redis for PHPs >= 7.4
    echo 'no' | pecl install redis; \
  else \
    # install redis-4.3.0 - last one with support for php 7.2
    echo 'no' | pecl install redis-4.3.0; \
  fi && docker-php-ext-enable redis

# memcache
# Pre 8.0 requires 4.0.5.2
RUN \
  php_cmp=$(php -r "echo version_compare(PHP_VERSION, '8.0.0', '>');"); \
  if [ "$php_cmp" = 1 ]; then \
    pecl install memcache && docker-php-ext-enable memcache; \
  else \
    pecl install memcache-4.0.5.2 && docker-php-ext-enable memcache; \
  fi

# memcached
RUN apt-get install -y libmemcached-dev
RUN pecl install memcached && docker-php-ext-enable memcached

# uopz
# Pre-8.0 requires 6.1.2
# uopz incompatible with JIT on PHP 8.0+
RUN \
  php_cmp=$(php -r "echo version_compare(PHP_VERSION, '8.0.0', '<');"); \
  if [ "$php_cmp" = 1 ]; then \
    pecl install uopz-6.1.2 && docker-php-ext-enable uopz; \
    # configure uopz to honor exit() and die() otherwise it just ignores these
    echo "uopz.exit=1" > /usr/local/etc/php/conf.d/uopz-enable-exit.ini; \
  fi

# install predis
# installation will be in /usr/src/vendor/predis/predis
# which is value which should be used for PREDIS_HOME
WORKDIR /usr/src
RUN composer require "predis/predis"
RUN composer update

#
# These args need to be repeated so we can propagate the VARS within this build context.
#
ARG PHP_VER
ENV PHP_VER=${PHP_VER}
ARG PS1
ENV PS1="New Relic > "

# QOL aliases
# `rebuild` - make clean + make agent + make tests
# `integ` - run all integration tests
RUN echo 'alias integ="/usr/src/myapp/bin/integration_runner -agent /usr/src/myapp/agent/.libs/newrelic.so"' >> ~/.bashrc \
  && echo 'alias rebuild="make -C agent clean && rm agent/Makefile && make && make tests"' >> ~/.bashrc

WORKDIR /usr/src/myapp
CMD ["bash"]
