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
RUN apt-get update
RUN apt-get install -y build-essential

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
  python-dev-is-python3 \
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
# Download and install Go
#
RUN arch=$(arch | sed s/aarch64/arm64/ | sed s/x86_64/amd64/) && \
    wget https://go.dev/dl/go1.21.1.linux-${arch}.tar.gz -O- | tar -C /usr/local -zxvf -;

RUN ln -s /usr/local/go/bin/go /usr/bin/go

#
# If the debian version is jessie, don't install argon2
#
RUN if [ -z "$(grep '^8\.' /etc/debian_version)" ]; then \
    apt-get install -y argon2 libghc-argon2-dev; \
    fi

# install composer
WORKDIR /usr/src

# based on https://getcomposer.org/doc/faqs/how-to-install-composer-programmatically.md
RUN \
 EXPECTED_CHECKSUM="$(php -r 'copy("https://composer.github.io/installer.sig", "php://stdout");')" \
 && php -r "copy('https://getcomposer.org/installer', 'composer-setup.php');" \
 && ACTUAL_CHECKSUM="$(php -r "echo hash_file('sha384', 'composer-setup.php');")" \
 && if [ "$EXPECTED_CHECKSUM" != "$ACTUAL_CHECKSUM" ]; \
  then \
    >&2 echo 'ERROR: Invalid installer checksum'; \
    rm composer-setup.php; \
    exit 1; \
  fi \
 && php composer-setup.php \
 && php -r "unlink('composer-setup.php');"

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

# redis
RUN pecl install redis && docker-php-ext-enable redis

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
RUN php composer.phar require "predis/predis"
RUN php composer.phar update

#
# install composer and make executable so it can be used in dev env
#
RUN cp composer.phar /usr/local/bin/composer && chmod +x /usr/local/bin/composer

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
