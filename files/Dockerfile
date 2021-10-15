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

# install composer
RUN php -r "copy('https://getcomposer.org/installer', 'composer-setup.php');"
RUN php -r "if (hash_file('sha384', 'composer-setup.php') === '906a84df04cea2aa72f40b5f787e49f22d4c2f19492ac310e8cba5b96ac8b64115ac402c8cd292b8a03482574915d1a8') { echo 'Installer verified'; } else { echo 'Installer corrupt'; unlink('composer-setup.php'); } echo PHP_EOL;"
RUN php composer-setup.php
RUN php -r "unlink('composer-setup.php');"

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

RUN docker-php-ext-install mysqli

# install predis
# installation will be in /usr/src/myapp/vendor/predis/predis
# which is value which should be used for PREDIS_HOME
RUN php composer.phar require "predis/predis"
RUN php composer.phar update

# redis
RUN pecl install redis && docker-php-ext-enable redis

# memcache
RUN pecl install memcache && docker-php-ext-enable memcache

# memcached
RUN apt-get install -y libmemcached-dev
RUN pecl install memcached && docker-php-ext-enable memcached

# mongo?
# this extension is no longer maintained and doesn't work with
# recent PHP releases
#RUN pecl install mongo && docker-php-ext-enable mongo

# mongodb - need tests written for this maintained mongo extension
#RUN pecl install mongodb && docker-php-ext-enable mongodb

# uopz
RUN pecl install uopz && docker-php-ext-enable uopz

# configure uopz to honor exit() and die() otherwise it just ignores these
RUN echo "uopz.exit=1" > /usr/local/etc/php/conf.d/uopz-enable-exit.ini

#
# These args need to be repeated so we can propagate the VARS within this build context.
#
ARG PHP_VER
ENV PHP_VER=${PHP_VER}
ARG PS1
ENV PS1="New Relic > "

WORKDIR /usr/src/myapp
CMD ["bash"]
