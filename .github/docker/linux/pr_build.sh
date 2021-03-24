#!/bin/bash

#
# Copyright 2020 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#

#
# Build the daemon, agent and axiom tests in accordance  with our pull request
# guidelines, which are documented in the README.md file. Additionally, run
# the axiom tests.
#
# This script is meant to run in GHA  with the GitHub Pull Request
# action which handles reporting the outcome to GitHub for us.
#

set -e
set -u

die() {
  echo
  echo >&2 "FATAL: $*"
  echo
  exit 1
}

#
# Ensure /usr/local/bin should be in the PATH.
#
case ":$PATH:" in
  *:/usr/local/bin:*) ;;
  *) PATH=/usr/local/bin:$PATH
esac

case ":$PATH:" in
  *:/usr/local/go/bin:*) ;;
  *) PATH=/usr/local/go/bin:$PATH
esac

export PATH


#
# Set LD_LIBRARY_PATH
#
LD_LIBRARY_PATH=/usr/local/lib
export LD_LIBRARY_PATH

#
# Get PHPS from the environment.
#

PHPS=${PHP_VER}

printf \\n
printf 'running daemon tests\n'
make -r -s daemon daemon_integration  "ARCH=${ARCH}"

#
# Limit valgrind to just the Linux builders. On Alpine Linux, valgrind
# appears to alter the results of floating point to string conversions
# causing spurious test failures.
#

PHP_SAPIS=$(php-config --php-sapis)

case $PHP_SAPIS in
  *embed*)
    PHP_SAPIS_EMBED=1
    ;;
  *)
    PHP_SAPIS_EMBED=0
    ;;
esac

if [ "$(uname)" = Linux ] && [ ! -e /etc/alpine-release ] && [ $PHP_SAPIS_EMBED ]; then
  do_valgrind=yes
  printf \\n
  printf 'grinding axiom tests\n'
  make -r -s -j $(nproc) axiom-valgrind "ARCH=${ARCH}"
else
  do_valgrind=
  printf \\n
  printf 'running axiom tests\n'
  make -r -s -j $(nproc) axiom-run-tests "ARCH=${ARCH}"
fi

#
# Run the agent integration tests without requiring any changes to the
# PHP installation, and (ideally) without the tests being negatively
# affected by any existing INI settings. For example, a pre-existing
# "extension = newrelic.so". For each version of PHP, override the INI
# file, INI directory and the extension directory using environment
# variables. 
#

INTEGRATION_DIR="${PWD}/integration.tmp"
if [ ! -d "$INTEGRATION_DIR" ]; then
  mkdir "$INTEGRATION_DIR"
  mkdir "${INTEGRATION_DIR}/etc"
fi

rm -rf "${INTEGRATION_DIR:?}"/*

export PHPRC="${INTEGRATION_DIR}/php.ini"
export PHP_INI_SCAN_DIR="${INTEGRATION_DIR}/etc"

cat <<EOF >"$PHPRC"
date.timezone = "America/Los_Angeles"
extension_dir = "${PWD}/agent/modules"
extension = "newrelic.so"
newrelic.loglevel = "verbosedebug"
EOF

#
# Build a specific version of PHP and run unit and integration tests.
#
# If PHP with thread safety (ZTS) is enabled, build to ensure
# it compiles cleanly, but don't run the integration tests because
# (empirically) some PHP extensions are inconsistent with ZTS enabled leading
# to spurious failures that are not agent bugs.
#

  if [ -n "${LD_LIBRARY_PATH-}" ]; then
    export LD_LIBRARY_PATH
  fi

  printf \\n
  printf "building agent (PHP=%s)\n" "$PHPS"
  make agent-clean
  make -r -s -j $(nproc) agent "ARCH=${ARCH}"


  printf \\n
  case "$PHPS" in 
  *8.0*)
    printf "Skipping integration tests on PHP=%s while tests are under construction\n" "$PHPS"
    ;;
  *zts*)
    printf "Skipping integration tests on ZTS (PHP=%s ZTS=enabled)\n" "$PHPS"
    ;;
  *)
    printf "Running agent integration tests (PHP=%s ZTS=disabled)\n" "$PHPS"
    printf "Temporarily disable integration tests in GHA while determining what to do with private keys.\n"
    #make integration PHPS="$PHPS" "ARCH=${ARCH}" INTEGRATION_ARGS="--retry=1"
    ;;
  esac
  printf \\n

  #
  # Run the agent unit tests (just on Linux).
  #
  if [ "$(uname -s)" = 'Linux' ]; then
    PHP_PREFIX=$(php-config --prefix)

    case $PHP_SAPIS in
      *embed*)
        if [ -n "$do_valgrind" ]; then
          printf 'grinding agent unit tests\n'
          make -r -s -j $(nproc) agent-valgrind "ARCH=${ARCH}"
        else
          printf 'running agent unit tests\n'
          make -r -s -j $(nproc) agent-check "ARCH=${ARCH}"
        fi
	;;
      *)
        printf 'skipping agent unit tests - embed SAPI not present\n'
        ;;
    esac
  else
    printf 'skipping agent unit tests - not Linux\n'
  fi

  printf 'creating build artifacts'
  make -r -j $(nproc) release-${PHP_VER}-gha "OPTIMIZE=1" "ARCH=${ARCH}"

  printf \\n   # put a blank line
