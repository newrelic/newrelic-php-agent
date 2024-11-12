#
# Copyright 2020 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#

#
# The top level Makefile
#
GCOV  ?= gcov
SHELL = /bin/bash
GCOVR ?= gcovr
GIT   ?= git

include make/config.mk
include make/vendor.mk
include make/version.mk
include make/php_versions.mk

# Include the secrets file if it exists, but if it doesn't, that's OK too.
-include make/secrets.mk

GCOVRFLAGS += -e "agent/tests/*" -e "axiom/tests/*" -e ".*\.h" -o

#
# Uniformly apply compiler and linker flags for the target architecture.
# This includes ensuring these flags are picked up by the agent's configure
# script. Otherwise, the agent build may fail if the agent's target
# architecture does not match axiom.
#
CFLAGS += $(MODEL_FLAGS)
LDFLAGS += $(MODEL_FLAGS)

ifeq (1,$(OPTIMIZE))
  CFLAGS += -O3 -g1
  LDFLAGS += -O3 -g1
else
  CFLAGS += -O0 -g3 -DENABLE_TESTING_API
  LDFLAGS += -O0 -g3
endif

ifeq (1,$(ENABLE_LTO))
  CFLAGS += -flto
  LDFLAGS += -flto
endif

ifeq (1,$(ENABLE_MULDEFS))
  LDFLAGS += -Wl,--no-warn-search-mismatch -Wl,-z,muldefs
endif

#
# Sanitizers
#
# Support for sanitizers varies by compiler and platform. Generally, it's best
# to use these on Linux and with the latest version of Clang, but GCC 4.9 or
# newer works as well. Compiling with -O1 or -Og with full debug info is
# strongly recommended for usable stack traces. See the Clang user manual
# for more information and options.
#
# Common sanitizers: address, integer, memory, thread, undefined
#
ifneq (,$(SANITIZE))
  CFLAGS += -fsanitize=$(SANITIZE) -fno-omit-frame-pointer
  LDFLAGS += -fsanitize=$(SANITIZE)
endif

#
# When the silent flag (-s) is given, configure and libtool should
# also be quiet.
#
ifneq (,$(findstring s,$(MAKEFLAGS)))
SILENT := --silent
endif

#
# At this point the following variables should have their final values,
# and can be safely exported to recipe commands.
#
export AR CC CFLAGS CPPFLAGS LDFLAGS

.PHONY: all
all: agent daemon

#
# Print some of the build specific variables so they can be computed
# once in this file, but shared with our Jenkins build scripts. Values
# should be formatted in a shell compatible NAME=VALUE format so the
# build scripts can safely eval the output of this target. NOTE: the
# build scripts are sensitive to the variable names. If you need
# to print something that is not in this format, or should otherwise
# be ignored by the build scripts, prefix it with '#' so it will be
# treated as a comment.
#
.PHONY: echo_config
echo_config:
	@echo '# Ignore me, Makefile example comment'
	@echo VERSION=$(AGENT_VERSION)
	@echo GIT_COMMIT=$(GIT_COMMIT)

#
# Print information where do the build dependencies come from
#
.PHONY: show-vendors
show-vendors:
	@echo ""
	@echo -----------------------------------------------------------------------
	@echo "| Using pcre library from $(PCRE_PREFIX) (from $(origin PCRE_PREFIX))"
	@echo -n "| Link to "; [ $(PCRE_STATIC) = yes ] && echo -n "static" || echo -n "shared"; echo " libpcre";
	@echo "| Using protobuf-c library from $(PROTOBUF_C_PREFIX) (from $(origin PROTOBUF_C_PREFIX))"
	@echo -----------------------------------------------------------------------
	@echo ""
#
# Let's build an agent! Building an agent is a three step process: using phpize
# to build a configure script, using configure to build a Makefile, and then
# actually using that Makefile to build the agent extension.
#
PHPIZE := phpize
PHP_CONFIG := php-config

.PHONY: agent
agent: show-vendors agent/Makefile
	$(MAKE) -C agent

agent/configure: agent/config.m4 agent/Makefile.frag
	cd agent; $(PHPIZE) --clean && $(PHPIZE)

agent/Makefile: agent/configure | axiom
	cd agent; ./configure $(SILENT) --enable-newrelic --with-axiom=$(realpath axiom) --with-php-config=$(PHP_CONFIG) --with-protobuf-c=$(PROTOBUF_C_PREFIX) --with-pcre=$(PCRE_PREFIX) --with-pcre-static=$(PCRE_STATIC)

#
# Installs the agent into the extension directory of the appropriate PHP
# installation. By default this is directory name formed by evaluating
# $(INSTALL_ROOT)$(EXTENSION_DIR). The former is empty by default, so to
# install the agent into the directory of your choice run the following.
#
#   make agent-install EXTENSION_DIR=...
#
agent-install: agent
	$(MAKE) -C agent install

.PHONY: agent-clean
agent-clean:
	cd agent; [ -f Makefile ] && $(MAKE) clean; $(PHPIZE) --clean

#
# Agent tests. These just delegate to the agent Makefile: all the smarts are in
# agent/Makefile.frag.
#
.PHONY: agent-tests
agent-tests: agent/Makefile
	$(MAKE) -C agent unit-tests

.PHONY: agent-check agent-run-tests
agent-check agent-run-tests: agent/Makefile
	$(MAKE) -C agent run-unit-tests

.PHONY:
agent-valgrind: agent/Makefile
	$(MAKE) -C agent valgrind

#
# Daemon rules
# defers to behavior defined in daemon/Makefile once $GOBIN has been set
#

# Configure the target directory for go install
export GOBIN=$(CURDIR)/bin

.PHONY: daemon
daemon:
	$(MAKE) -C daemon

.PHONY: daemon_race
daemon_race:
	$(MAKE) -C daemon race

.PHONY: daemon_test
daemon_test:
	$(MAKE) -C daemon test

.PHONY: daemon_bench
daemon_bench:
	$(MAKE) -C daemon bench

.PHONY: daemon_integration
daemon_integration:
	$(MAKE) -C daemon integration

.PHONY: daemon_cover
daemon_cover:
	$(MAKE) -C daemon cover

bin/integration_runner:
	$(MAKE) -C daemon integration_runner

# Note that this rule does not require the Go binary, and therefore doesn't
# depend on go-minimum-version.
.PHONY: daemon-clean
daemon-clean:
	rm -f $(DAEMON_TARGETS)
	rm -rf pkg/*

#
# Build the installer plus support utility. The support utility is run by
# the installer to provide some helpful operations missing from bash or
# missing from the $PATH.
#

installer: bin/newrelic-install bin/newrelic-iutil | bin/

bin/newrelic-install: agent/newrelic-install.sh Makefile VERSION | bin/
	sed -e "/nrversion:=/s,UNSET,$(AGENT_VERSION)," $< > $@
	chmod 755 $@

bin/newrelic-iutil: agent/install-util.c Makefile VERSION | bin/
	$(CC) -DNR_VERSION="\"$(AGENT_VERSION)\"" $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS)


#
# Directories required during builds.
#

bin/:
	mkdir bin

#
# Build axiom and the axiom tests
#

.PHONY: axiom
axiom: vendor
	$(MAKE) -C axiom

#
# TESTARGS is passed to every invocation of a test program.
# If you want to run each tests with 16 way thread parallelism, you would do:
#   make TESTARGS=-j16 run_tests
#
# TESTARGS =

.PHONY: axiom-tests
axiom-tests: vendor
	$(MAKE) -C axiom tests

.PHONY: axiom-check axiom-run-tests
axiom-check axiom-run-tests: vendor axiom/tests/cross_agent_tests
	$(MAKE) -C axiom run_tests

.PHONY: axiom-valgrind
axiom-valgrind: vendor axiom/tests/cross_agent_tests
	$(MAKE) -C axiom valgrind

.PHONY: tests
tests: agent-tests axiom-tests

.PHONY: check run_tests
check run_tests: agent-run-tests axiom-run-tests

.PHONY: valgrind
valgrind: agent-valgrind axiom-valgrind

axiom/tests/cross_agent_tests:
	$(error Please run "git submodule update --init" to install the cross agent tests.)

.PHONY: axiom-clean
axiom-clean:
	$(MAKE) -C axiom clean

#
# Go protocol buffers
#
# Run this target to compile the protocol buffers for Go. The Go protocol
# buffer plugin used for this is not vendored. It can be installed via
#
#        go install google.golang.org/protobuf/cmd/protoc-gen-go
#
# Before running this script, make sure the location of protoc-gen-go is in
# your $PATH.
.PHONY: daemon-protobuf
daemon-protobuf: daemon/internal/newrelic/infinite_tracing/com_newrelic_trace_v1/v1.pb.go

daemon/internal/newrelic/infinite_tracing/com_newrelic_trace_v1/v1.pb.go: protocol/infinite_tracing/v1.proto
	$(MAKE) vendor # Only build vendor stuff if v1.proto has changed. Otherwise
	               # this rule will be triggered every time the daemon is built.
	$(VENDOR_PREFIX)/bin/protoc \
	    -I=./protocol/infinite_tracing \
	    --go_out="paths=source_relative,plugins=grpc:daemon/internal/newrelic/infinite_tracing/com_newrelic_trace_v1" \
	    protocol/infinite_tracing/v1.proto

#
# Agent integration testing
#

.PHONY: integration
integration: Makefile integration-tests lasp-test-all integration-events-limits

.PHONY: integration-tests
integration-tests: bin/integration_runner
	@for PHP in $(PHP_VERSION_LIST); do \
          echo; echo "# PHP=$${PHP}"; \
	  env NRLAMP_PHP=$${PHP} bin/integration_runner $(INTEGRATION_ARGS) || exit 1; \
	  echo "# PHP=$${PHP}"; \
	done

#
# Agent event limits integration testing
#
# Because of how the integration_runner connects to the collector (once before
# running any tests) we have to tell the runner the value the agent would
# have passed to it for each test.  This means these tests are not testing
# the agent <-> daemon communcations of the agent's requested custom event limit
# via the daemon to the collector and daemon sending back the collectors harvest
# limit value.
#

.PHONY: integration-events-limits
integration-events-limits: bin/integration_runner
	@# create array with the collector response for each agent requested custom events max samples
	@# currently based on fast harvest cycle being 5 seconds so ratio is 12:1
	@declare -A custom_limits_tests; \
	custom_limits_tests[240]=20; \
	custom_limits_tests[7000]=583; \
	custom_limits_tests[30000]=2500; \
	custom_limits_tests[100000]=8333; \
	for PHP in $(PHP_VERSION_LIST); do \
          echo; echo "# PHP=$${PHP}"; \
	      for custom_max in "$${!custom_limits_tests[@]}"; do \
	          collector_limit=$${custom_limits_tests[$$custom_max]}; \
			  php_test_file="tests/event_limits/custom/test_custom_events_max_samples_stored_$${custom_max}_limit.php"; \
	          env NRLAMP_PHP=$${PHP} bin/integration_runner $(INTEGRATION_ARGS) \
	            -max_custom_events $${custom_max} $${php_test_file} || exit 1; \
	      done; \
	      echo "# PHP=$${PHP}"; \
	done;

	@# test for invalid value (-1) and (1000000)
	@# Should use default (30000) for -1 and max (100000) for 1000000
	@for PHP in $(PHP_VERSION_LIST); do \
	    env NRLAMP_PHP=$${PHP} bin/integration_runner $(INTEGRATION_ARGS) \
	        -max_custom_events 100000 \
	        tests/event_limits/custom/test_custom_events_max_samples_stored_invalid_toolarge_limit.php || exit 1; \
	    env NRLAMP_PHP=$${PHP} bin/integration_runner $(INTEGRATION_ARGS) \
	        -max_custom_events 30000 \
	        tests/event_limits/custom/test_custom_events_max_samples_stored_invalid_toosmall_limit.php || exit 1; \
	    echo "# PHP=$${PHP}"; \
	done;	

	@# also run a test where limit is set to 0
	@# default value is used
	@for PHP in $(PHP_VERSION_LIST); do \
	    env NRLAMP_PHP=$${PHP} bin/integration_runner $(INTEGRATION_ARGS) \
	        -max_custom_events 0 \
	        tests/event_limits/custom/test_custom_events_max_samples_stored_0_limit.php || exit 1; \
	    echo "# PHP=$${PHP}"; \
	done;

	@# also run a test where no agent custom event limit is specified and verify
	@# default value is used
	@for PHP in $(PHP_VERSION_LIST); do \
	    env NRLAMP_PHP=$${PHP} bin/integration_runner $(INTEGRATION_ARGS) \
	        -max_custom_events 30000 \
	        tests/event_limits/custom/test_custom_events_max_samples_stored_not_specified.php || exit 1; \
	    echo "# PHP=$${PHP}"; \
	done;

#
# Code profiling
#

.PHONY: gcov
gcov: Makefile
	cd agent; $(GCOV) $(GCOV_FLAGS) *.gcno
	cd agent; $(GCOV) -o .libs $(GCOV_FLAGS) .libs/*.gcno
	cd axiom; $(GCOV) $(GCOV_FLAGS) *.gcno
	cd axiom/tests; $(GCOV) $(GCOV_FLAGS) *.gcno

# Reset code coverage line counts.
.PHONY: coverage-clean
coverage-clean:
	find . -name \*.gcda | xargs rm -f
	find . -name \*coverage-report*.html | xargs rm -f
	find . -name \*coverage-report*.xml | xargs rm -f

# Creates a human readable coverage report, the name of the file will be coverage-report.html.
.PHONY: coverage-report-html
coverage-report-html:
	$(GCOVR) -r . --html-details $(GCOVRFLAGS) coverage-report.html;

# Create and xml code coverage report. The xml report is intended for jenkins.
.PHONY: coverage-report-xml
coverage-report-xml:
	$(GCOVR) -r . --xml $(GCOVRFLAGS) coverage-report.xml || true

# Compile all tests with the coverage flag and run them. Then create the html coverage report.
.PHONY: coverage
coverage:
	$(MAKE) ENABLE_COVERAGE=1
	.\/bin\/integration_runner
	$(MAKE) ENABLE_COVERAGE=1 run_tests
	$(MAKE) coverage-report-html

#
# Clean up
#

.PHONY: clean
clean: agent-clean axiom-clean daemon-clean package-clean coverage-clean vendor-clean
	rm -rf releases
	rm -f agent/newrelic.map agent/LicenseData/license_errors.txt

.PHONY: package-clean
package-clean:
	rm -rf releases/debian releases/*.deb
	rm -rf releases/redhat releases/*.rpm
	rm -rf releases/newrelic-php5-*.tar.gz

#
# Testing the Language Agent Security Policy (LASP) feature.
#
.PHONY: lasp-test
lasp-test: bin/integration_runner
	@if [ ! $(SUITE_LASP) ]; then echo "USAGE: make lasp-test SUITE_LASP=suite-most-secure"; exit 1; fi
	@if [ "$(LICENSE_lasp_$(subst -,_,$(SUITE_LASP)))" = "" ] ; then echo "Missing license for $(SUITE_LASP)"; exit 1; fi
	@if [ ! -d "tests/lasp/$(SUITE_LASP)" ]; then echo "No such suite in tests/lasp folder"; exit 1; fi
	@for PHP in $(PHP_VERSION_LIST); do \
          echo; echo "# PHP=$${PHP}"; \
          NRLAMP_PHP=$${PHP} bin/integration_runner $(INTEGRATION_ARGS) -loglevel debug \
        -license $(LICENSE_lasp_$(subst -,_,$(SUITE_LASP))) \
        -security_token @tests/lasp/$(SUITE_LASP)/security-token.txt \
        -supported_policies @tests/lasp/$(SUITE_LASP)/securityPolicyAgent.json tests/lasp/$(SUITE_LASP) || exit 1; \
	  echo "# PHP=$${PHP}"; \
	done



.PHONY: lasp-test-all
lasp-test-all:
	$(MAKE) lasp-test SUITE_LASP=suite-most-secure
	$(MAKE) lasp-test SUITE_LASP=suite-least-secure
	$(MAKE) lasp-test SUITE_LASP=suite-random-1
	$(MAKE) lasp-test SUITE_LASP=suite-random-2
	$(MAKE) lasp-test SUITE_LASP=suite-random-3

#
# Vendored libraries
#
export GIT

.PHONY: vendor vendor-clean
ifeq (0,$(HAVE_PROTOBUF_C))
vendor:
	$(MAKE) -C vendor all

vendor-clean:
	$(MAKE) -C vendor clean
else
vendor: ;

vendor-clean: ;
endif

#
# Extras
#

include make/release.mk

test-services-start:
	docker compose --profile test pull $(SERVICES)
	docker compose --profile test up --wait --remove-orphans -d $(SERVICES)

test-services-stop:
	docker compose --profile test stop

#
# Docker Development Environment
#

devenv-image:
	@docker compose --profile dev build devenv

dev-shell: devenv-image
	docker compose --profile dev up --pull missing --remove-orphans -d
	docker compose exec -it devenv bash -c "sh files/set_path.sh ; bash"

dev-build: devenv-image
	docker compose --profile dev up --pull missing --remove-orphans -d
	docker compose exec -it devenv bash -c "sh files/set_path.sh ; make -j4 all"

dev-unit-tests: devenv-image
	docker compose --profile dev up --pull missing --remove-orphans -d
	docker compose exec -it devenv bash -c "sh files/set_path.sh ; make -j4 valgrind"

dev-integration-tests: devenv-image
	docker compose --profile dev up --pull missing --remove-orphans -d
	docker compose exec -it devenv bash -c "sh files/set_path.sh ; ./bin/integration_runner -agent ./agent/.libs/newrelic.so"

dev-all: devenv-image
	docker compose --profile dev up --pull missing --remove-orphans -d
	docker compose exec -it devenv bash -c "sh files/set_path.sh ; make -j4 all valgrind; ./bin/integration_runner -agent ./agent/.libs/newrelic.so"

dev-stop:
	docker compose --profile dev stop

# vim: set noet ts=2 sw=2:
