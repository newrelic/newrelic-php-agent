#
# Copyright 2020 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#

#
# Build recipes for constructing an agent release with build machines.
#
# Note: The recipes in this file assume the current working directory is
# the top-level of the project.
#

RELEASE_OS := $(shell uname -s | tr '[:upper:]' '[:lower:]')
RELEASE_OS := $(RELEASE_OS:darwin=osx)
RELEASE_OS := $(RELEASE_OS:sunos=solaris)

# Detect whether this build will link against musl libc. For now, this
# check assumes musl is the only libc that does not define a special
# symbol to uniquely identify it. If necessary, this check can be easily
# expanded to cover other C standard library implementations such as
# uclibc or dietlibc.
ifeq (linux,$(RELEASE_OS))
  RELEASE_LIBC := $(shell $(CC) -x c -E make/detect-linux-libc.env | grep -E '^LIBC.*=')
  ifeq (gnu,$(findstring gnu,$(RELEASE_LIBC)))
    RELEASE_OS := $(RELEASE_OS)
  else ifeq (musl,$(findstring musl,$(RELEASE_LIBC)))
    RELEASE_OS := $(RELEASE_OS)-musl
  else
    $(error Cannot detect the C library in use; exiting)
  endif
endif

RELEASE_ARCH := $(ARCH)

# Darwin uses a custom architecture name. This is a remnant of the switch
# from universal to x86_64 only binaries.
ifeq (osx,$(RELEASE_OS))
  ifeq (x64,$(ARCH))
    RELEASE_ARCH := x86_64
  endif
endif

.PHONY: release
release: Makefile release-version release-installer release-agent release-docs release-scripts | releases/$(RELEASE_OS)/

release-version: releases/$(RELEASE_OS)/
	printf '%s\n' "$(AGENT_VERSION)" > releases/$(RELEASE_OS)/VERSION
	printf '%s\n' "$(GIT_COMMIT)" > releases/$(RELEASE_OS)/COMMIT

release-daemon: Makefile daemon | releases/$(RELEASE_OS)/daemon/
	cp bin/daemon releases/$(RELEASE_OS)/daemon/newrelic-daemon.$(RELEASE_ARCH)

.PHONY: release-installer
release-installer: Makefile release-installer-script release-installer-iutil

release-installer-script: bin/newrelic-install | releases/$(RELEASE_OS)/
	cp bin/newrelic-install releases/$(RELEASE_OS)

release-installer-iutil: bin/newrelic-iutil | releases/$(RELEASE_OS)/scripts/
	cp bin/newrelic-iutil   releases/$(RELEASE_OS)/scripts/newrelic-iutil.$(RELEASE_ARCH)

release-docs: Makefile | releases/$(RELEASE_OS)/
	cp agent/README.txt LICENSE releases/$(RELEASE_OS)

release-scripts: Makefile | releases/$(RELEASE_OS)/scripts/
	cp agent/scripts/init.alpine               releases/$(RELEASE_OS)/scripts
	cp agent/scripts/init.darwin               releases/$(RELEASE_OS)/scripts
	cp agent/scripts/init.debian               releases/$(RELEASE_OS)/scripts
	cp agent/scripts/init.freebsd              releases/$(RELEASE_OS)/scripts
	cp agent/scripts/init.generic              releases/$(RELEASE_OS)/scripts
	cp agent/scripts/init.rhel                 releases/$(RELEASE_OS)/scripts
	cp agent/scripts/init.solaris              releases/$(RELEASE_OS)/scripts
	cp agent/scripts/newrelic-daemon.logrotate releases/$(RELEASE_OS)/scripts
	cp agent/scripts/newrelic-daemon.service   releases/$(RELEASE_OS)/scripts
	cp agent/scripts/newrelic-php5.logrotate   releases/$(RELEASE_OS)/scripts
	cp agent/scripts/newrelic.cfg.template     releases/$(RELEASE_OS)/scripts
	cp agent/scripts/newrelic.ini.template     releases/$(RELEASE_OS)/scripts
	cp agent/scripts/newrelic.sysconfig        releases/$(RELEASE_OS)/scripts
	cp agent/scripts/newrelic.xml              releases/$(RELEASE_OS)/scripts

#
# GitHub Actions release target - release-agent. GitHub Actions release
# workflow builds extension binaries in parallel with dedicated build
# service for each supported PHP version.
#
release-agent: Makefile | releases/$(RELEASE_OS)/agent/$(RELEASE_ARCH)/
	@$(MAKE) agent-clean && $(MAKE) agent-for-release

# Older versions of GNU Make had a bug where "#" in a function invocation
# such as $(shell ...) was treated as a make comment. This makefile needs
# to be compatible with older versions of GNU Make, so we need to use
# a workaround by assigning "#" to a variable and using that variable in
# the function invocation.
H := \#

# Target for building the agent for a given PHP version. Works well
# when building agent using containers. This is useful not only in
# GitHub Actions workflows, but also in a day to day development,
# because it allows to preserve agent between PHP version switches.
agent-for-release: PHP_API_VERSION=$(shell awk '/^$(H)define[[:space:]]+PHP_API_VERSION/ {print $$3}' "$(shell $(PHP_CONFIG) --include-dir)/main/php.h")
agent-for-release: PHP_ZTS=$(shell awk '/^$(H)define[[:space:]]+ZTS/ {print "-zts"}' "$(shell $(PHP_CONFIG) --include-dir)/main/php_config.h")
agent-for-release: Makefile agent | releases/$(RELEASE_OS)/agent/$(RELEASE_ARCH)/
	@test -n "$(PHP_API_VERSION)" || { echo "ERROR: Could not detect PHP_API_VERSION"; exit 1; }
	@echo "PHP API version detected: [$(PHP_API_VERSION)]"
	@echo "PHP variant detected: [$(PHP_ZTS)]"
	@cp -v agent/modules/newrelic.so "releases/$(RELEASE_OS)/agent/$(RELEASE_ARCH)/newrelic-$(PHP_API_VERSION)$(PHP_ZTS).so"
	@test -e agent/newrelic.map && cp -v agent/newrelic.map "releases/$(RELEASE_OS)/agent/$(RELEASE_ARCH)/newrelic-$(PHP_API_VERSION)$(PHP_ZTS).map" || true

#
# Release directories
#

releases/:
	mkdir releases

releases/$(RELEASE_OS)/: Makefile | releases/
	mkdir releases/$(RELEASE_OS)

releases/$(RELEASE_OS)/agent/: Makefile | releases/$(RELEASE_OS)/
	mkdir releases/$(RELEASE_OS)/agent

releases/$(RELEASE_OS)/agent/$(RELEASE_ARCH)/: Makefile | releases/$(RELEASE_OS)/agent/
	mkdir releases/$(RELEASE_OS)/agent/$(RELEASE_ARCH)

releases/$(RELEASE_OS)/daemon/: Makefile | releases/$(RELEASE_OS)/
	mkdir releases/$(RELEASE_OS)/daemon

releases/$(RELEASE_OS)/scripts/: Makefile | releases/$(RELEASE_OS)/
	mkdir releases/$(RELEASE_OS)/scripts
