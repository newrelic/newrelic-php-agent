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

include make/php_versions.mk

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

# Set the PHP versions we support on 64-bit OSs.
SUPPORTED_64=$(PHP_VERSION_LIST)
# Set the PHP versions we support on 32-bit OSs.
SUPPORTED_32=
# Of the PHP versions we want to build, determine which are supported on the detected OS.
ifeq (x64,$(ARCH))
  SUPPORTED_PHP = $(filter $(SUPPORTED_64),$(PHP_VERSION_LIST))
else
  SUPPORTED_PHP = $(filter $(SUPPORTED_32),$(PHP_VERSION_LIST))
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

# Build the agent sequentially for each version of PHP. This is necessary
# because the PHP build process only supports in-tree builds.
release-agent: Makefile | releases/$(RELEASE_OS)/agent/$(RELEASE_ARCH)/

#
# First build non-ZTS binaries of the PHP versions requested that are supported
# on this OS.
#
	for PHP in $(SUPPORTED_PHP) ; do \
		$(MAKE) agent-clean; $(MAKE) release-$$PHP-no-zts; \
        done
#
# Next build ZTS binaries of the PHP versions requested that are supported
# on this OS.
#
	for PHP in $(SUPPORTED_PHP) ; do \
		$(MAKE) agent-clean; $(MAKE) release-$$PHP-zts; \
	done


#
# Add a new target to build the agent against build machines.
#
#   $1 - PHP version in MAJOR.MINOR format (e.g. 5.6)
#   $2 - Zend module API number (e.g. 20131226)
#
define RELEASE_AGENT_TARGET

#
# Target for building the agent for a given PHP version. Works well
# when building agent using containers. This is useful not only in 
# GitHub Actions workflows, but also in a day to day development,
# because it allows to preserve agent between PHP version switches.
#
agent-for-php-$1: PHP_CONFIG := /usr/local/bin/php-config
agent-for-php-$1: Makefile agent | releases/$$(RELEASE_OS)/agent/$$(RELEASE_ARCH)/
	@cp agent/modules/newrelic.so "releases/$$(RELEASE_OS)/agent/$$(RELEASE_ARCH)/newrelic-$2.so"
	@test -e agent/newrelic.map && cp agent/newrelic.map "releases/$$(RELEASE_OS)/agent/$$(RELEASE_ARCH)/newrelic-$2.map" || true

#
# Target for non-zts GHA releases.
#
release-$1-gha: PHPIZE := /usr/local/bin/phpize
release-$1-gha: PHP_CONFIG := /usr/local/bin/php-config
release-$1-gha: Makefile agent | releases/$$(RELEASE_OS)/agent/$$(RELEASE_ARCH)/
	@cp agent/modules/newrelic.so "releases/$$(RELEASE_OS)/agent/$$(RELEASE_ARCH)/newrelic-$2.so"
	@test -e agent/newrelic.map && cp agent/newrelic.map "releases/$$(RELEASE_OS)/agent/$$(RELEASE_ARCH)/newrelic-$2.map" || true

#
# Target for zts GHA releases.
#
release-$1-zts-gha: PHPIZE := /usr/local/bin/phpize
release-$1-zts-gha: PHP_CONFIG := /usr/local/bin/php-config
release-$1-zts-gha: Makefile agent | releases/$$(RELEASE_OS)/agent/$$(RELEASE_ARCH)/
	@cp agent/modules/newrelic.so "releases/$$(RELEASE_OS)/agent/$$(RELEASE_ARCH)/newrelic-$2-zts.so"
	 @test -e agent/newrelic.map && cp agent/newrelic.map "releases/$$(RELEASE_OS)/agent/$$(RELEASE_ARCH)/newrelic-$2-zts.map" || true

release-$1-no-zts: PHPIZE := /opt/nr/lamp/bin/phpize-$1-no-zts
release-$1-no-zts: PHP_CONFIG := /opt/nr/lamp/bin/php-config-$1-no-zts
release-$1-no-zts: Makefile agent | releases/$$(RELEASE_OS)/agent/$$(RELEASE_ARCH)/
	@cp agent/modules/newrelic.so "releases/$$(RELEASE_OS)/agent/$$(RELEASE_ARCH)/newrelic-$2.so"
	@test -e agent/newrelic.map && cp agent/newrelic.map "releases/$$(RELEASE_OS)/agent/$$(RELEASE_ARCH)/newrelic-$2.map" || true

release-$1-zts: PHPIZE := /opt/nr/lamp/bin/phpize-$1-zts
release-$1-zts: PHP_CONFIG := /opt/nr/lamp/bin/php-config-$1-zts
release-$1-zts: Makefile agent | releases/$$(RELEASE_OS)/agent/$$(RELEASE_ARCH)/
	@cp agent/modules/newrelic.so "releases/$$(RELEASE_OS)/agent/$$(RELEASE_ARCH)/newrelic-$2-zts.so"
	@test -e agent/newrelic.map && cp agent/newrelic.map "releases/$$(RELEASE_OS)/agent/$$(RELEASE_ARCH)/newrelic-$2-zts.map" || true

endef

$(eval $(call RELEASE_AGENT_TARGET,8.4,20240924))
$(eval $(call RELEASE_AGENT_TARGET,8.3,20230831))
$(eval $(call RELEASE_AGENT_TARGET,8.2,20220829))
$(eval $(call RELEASE_AGENT_TARGET,8.1,20210902))
$(eval $(call RELEASE_AGENT_TARGET,8.0,20200930))
$(eval $(call RELEASE_AGENT_TARGET,7.4,20190902))
$(eval $(call RELEASE_AGENT_TARGET,7.3,20180731))
$(eval $(call RELEASE_AGENT_TARGET,7.2,20170718))


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
