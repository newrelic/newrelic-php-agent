#
# Copyright 2020 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Dependency handling. This is much more complicated than in other parts of the
# agent because PHP's build system uses libtool, which means that the normal .d
# files that gcc/clang generate with -MMD -MP can't be used directly, as the
# rules they define target .libs/foo.o, whereas the Makefile rules libtool
# generates only builds those files as side effects of building foo.lo.
#
# To get around this, we'll take the .d files, use sed to transform the top
# level target each of them defines to match the libtool .lo target, and then
# include the generated file.
#
# We generate the file in .libs so that "make clean" will remove it
# automatically.

#
# When the silent flag (-s) is given, libtool should also be quiet.
#
ifneq (,$(findstring s,$(MAKEFLAGS)))
LIBTOOL += --silent
endif

#
# The rule to actually generate the deps.mk file. This depends on
# $(shared_objects_newrelic) to ensure that we run this after all .d files have
# been generated.
#
.libs/deps.mk: $(shared_objects_newrelic)
	cat .libs/*.d | $(SED) -e 's/^\.libs\/\(.*\)\.o:/\1.lo:/' > $@

#
# Make $(PHP_MODULES) depend on .libs/deps.mk, thereby ensuring that we rebuild
# deps.mk each time we build newrelic.so.
#
$(PHP_MODULES): .libs/deps.mk

#
# Include deps.mk if it exists. (If it doesn't, that's fine, since it means
# we're doing a full build anyway.)
#
-include .libs/deps.mk

#
# Add an explicit dependency on libaxiom.a so that if it changes, the agent
# gets relinked.
#
newrelic.la: $(PHP_AXIOM)/libaxiom.a

#
# The version number is needed by several source files as a static string literal,
# so it can be placed in the module entry.
#
include ../make/version.mk

php_newrelic.lo: CPPFLAGS += -DNR_VERSION="\"$(AGENT_VERSION)\""
php_newrelic.lo: ../VERSION

php_txn.lo: CPPFLAGS += -DNR_VERSION="\"$(AGENT_VERSION)\""
php_txn.lo: ../VERSION

#
# Unit tests!
#
# The top level target is "unit-tests", as PHP's build system defines a "test"
# target that we can't easily override or repurpose.
#

#
# The list of test binaries to be built. When adding a new unit test, you
# should be able to just add the binary name here, re-run phpize and configure
# (whether explicitly, or via "make agent-clean" from the top level php_agent
# Makefile) to regenerate the agent Makefile, and things should just work.
#
TEST_BINARIES = \
	tests/test_agent \
	tests/test_api_datastore \
	tests/test_api_distributed_trace \
	tests/test_api_internal \
	tests/test_api_metadata_dt_enabled \
	tests/test_api_metadata_dt_disabled \
	tests/test_call \
	tests/test_curl \
	tests/test_curl_md \
	tests/test_datastore \
	tests/test_environment \
	tests/test_fw_codeigniter \
	tests/test_fw_drupal \
	tests/test_fw_support \
	tests/test_fw_wordpress \
	tests/test_globals \
	tests/test_internal_instrument \
	tests/test_hash \
	tests/test_lib_aws_sdk_php \
        tests/test_lib_php_amqplib \
	tests/test_memcached \
	tests/test_mongodb \
	tests/test_monolog \
	tests/test_mysql \
	tests/test_mysqli \
	tests/test_output \
	tests/test_pdo \
	tests/test_pdo_mysql \
	tests/test_pdo_pgsql \
	tests/test_pgsql \
        tests/test_php_error \
	tests/test_php_execute \
	tests/test_php_minit \
	tests/test_php_stack \
	tests/test_php_stacked_segment \
	tests/test_php_txn \
	tests/test_php_wrapper \
	tests/test_predis \
	tests/test_redis \
	tests/test_txn \
	tests/test_txn_private \
	tests/test_user_instrument \
	tests/test_user_instrument_hashmap \
	tests/test_user_instrument_wraprec_hashmap \
	tests/test_zval

.PHONY: unit-tests
unit-tests: $(TEST_BINARIES)

#
# The order only dependency below means that we won't try to run a unit test
# until all binaries are built, which makes the output clearer at the expense
# of a little potential speed.
#
.PHONY: run-unit-tests
run-unit-tests: $(TEST_BINARIES:%=%.phony) | $(TEST_BINARIES)

#
# This is effectively a phony implicit rule, which isn't supported by make as
# such. We'll obviously never create a (for example) test_agent.phony file, but
# this means that make will run the test binary each time the run-unit-tests
# target above is invoked.
#
# The advantage to doing it this way rather than having run-unit-tests run each
# binary in a sh for loop is that make will detect non-zero return codes and
# abort immediately.
#
%.phony: %
	@$< $(TESTARGS)

#
# Valgrind support, starting with defining where to find valgrind and the
# scripts and suppressions we also use for the axiom test suite's valgrind
# support.
#
VALGRIND := valgrind
CHECK_VALGRIND_OUTPUT := $(CURDIR)/../make/check_valgrind_output.awk
VALGRIND_SUPPRESSIONS_AGENT := $(CURDIR)/tests/valgrind-suppressions
VALGRIND_SUPPRESSIONS_AXIOM := $(CURDIR)/../axiom/tests/valgrind-suppressions

#
# The exit code handling below is subtle. Valgrind returns the exit code of the
# called test, which (if it's non-zero) we need to detect to abort the for loop
# manually (by default, the exit code never makes it back to make, as the only
# exit code make sees is the eventual one from the for loop). So we grab it in
# $TEST_EXIT_CODE, then check that after we've finished reporting and exit if
# needed.
#
# We _also_ need to abort if Valgrind itself detected errors. You can't
# configure Valgrind to report a non-zero exit code for its own errors _and_
# the underlying program's exit code (it's one or the other), so we'll do this
# by post-processing the already post-processed output from
# CHECK_VALGRIND_OUTPUT. Ugly, but effective.
#
.PHONY: valgrind
valgrind: $(TEST_BINARIES)
	@for T in $(TEST_BINARIES); do \
	   USE_ZEND_ALLOC=0 ZEND_DONT_UNLOAD_MODULES=1 $(VALGRIND) --tool=memcheck --num-callers=30 --dsymutil=yes --leak-check=full --show-reachable=yes --suppressions=$(VALGRIND_SUPPRESSIONS_AGENT) --suppressions=$(VALGRIND_SUPPRESSIONS_AXIOM) --log-file=$$T.valgrind.log ./$$T $(TESTARGS); \
	   TEST_EXIT_CODE=$$?; \
	   VALGRIND_RESULTS=$$(VALGRIND_VERBOSE=1 $(CHECK_VALGRIND_OUTPUT) $$T.valgrind.log); \
	   printf "%42s with %s\n" ' ' "$$VALGRIND_RESULTS"; \
	   test "$$TEST_EXIT_CODE" -ne 0 && exit "$$TEST_EXIT_CODE"; \
(	   echo "$$VALGRIND_RESULTS" | grep -Eqv '^0 errors from 0 contexts') && exit 1; \
	done; \
	exit 0;

#
# Common object files to link into each test binary.
#
TLIB_OBJS = \
	tests/tlib_bool.o \
	tests/tlib_datastore.o \
	tests/tlib_exec.o \
	tests/tlib_files.o \
	tests/tlib_main.o \
	tests/tlib_php.o

EXPORT_DYNAMIC := -Wl,-export-dynamic

#
# We also need to manipulate $(NEWRELIC_SHARED_LIBADD) when this is
# build on OS X: its linker doesn't support allowing multiple definitions, and
# the PCRE symbols in the embed library collide with nrpcre-pic. It's a mess.
# Instead, we'll skip linking against nrpcre-pic, and use OS X's own PCRE
# shared library to fill in the blanks.
#
# If the build machine doesn't do builds on OS X, the following workaround
# is no longer necessary.
#
TEST_NEWRELIC_SHARED_LIBADD := $(NEWRELIC_SHARED_LIBADD)
ifeq (Darwin,$(shell uname))
	ifneq (,$(findstring -lnrpcre-pic,$(TEST_NEWRELIC_SHARED_LIBADD)))
		TEST_NEWRELIC_SHARED_LIBADD := $(filter-out -lnrpcre-pic,$(TEST_NEWRELIC_SHARED_LIBADD)) /usr/lib/libpcre.dylib
	endif
	EXPORT_DYNAMIC := -Wl,-export_dynamic
endif

#
# By default, the embed library is at $prefix/lib/libphp$major.{a,so}. We'll
# prefer the static version if it's available, but depending on how PHP was
# installed, we may only have the shared version.
#


# Update for PHP8:
# They no longer adhere to their prior versioning system.  The library is now
# simply $prefix/lib/libphp.{a,so}

major = $(shell $(PHP_CONFIG) --version | cut -d . -f 1)
ifeq ($(major), 8)
    major =
endif

ifneq (,$(wildcard $(libdir)/libphp$(major).a))
	PHP_EMBED_LIBRARY := $(libdir)/libphp$(major).a
else
	PHP_EMBED_LIBRARY := $(libdir)/libphp$(major).so
endif

#
# The following is a workaround to set $(PHP_EMBED_LIBRARY) correctly
# in cases where the build machine doesn't handle suffixes correctly 
# for embed SAPI builds.
#
ifeq (/opt/nr/lamp/lib,$(findstring /opt/nr/lamp/lib,$(PHP_EMBED_LIBRARY)))
	ifeq (no-zts,$(findstring no-zts,$(shell $(PHP_CONFIG) --include-dir)))
		PHP_EMBED_LIBRARY_SUFFIX = no-zts
	else
		PHP_EMBED_LIBRARY_SUFFIX = zts
	endif
	PHP_EMBED_LIBRARY := $(libdir)/libphp$(shell $(PHP_CONFIG) --version | cut -d . -f 1)-$(shell $(PHP_CONFIG) --version | cut -d . -f -2)-$(PHP_EMBED_LIBRARY_SUFFIX).a
        #
	# `libevent` has an issue with kqueues on macOS Sierra: this has been fixed
	# upstream (https://github.com/libevent/libevent/issues/376).  However,
	# for build machines using older versions of `libevent`, the
        # following will set an environment variable to hush it 
        # on all versions of OS X/macOS (at least
	# when the unit tests are being run via the Makefile, which is the normal
	# case).
        #
	ifeq (Darwin,$(shell uname))
		export EVENT_NOKQUEUE = 1
	endif
endif

#
# ZTS builds need -pthread specified again at the end of the build line, 
# so let's reuse TEST_NEWRELIC_SHARED_LIBADD to do that.
#
ifeq (/opt/nr/lamp/lib,$(findstring /opt/nr/lamp/lib,$(PHP_EMBED_LIBRARY)))
	ifneq (no-zts,$(findstring no-zts,$(shell $(PHP_CONFIG) --include-dir)))
		TEST_NEWRELIC_SHARED_LIBADD := $(TEST_NEWRELIC_SHARED_LIBADD) -pthread
	endif
endif

#
# Need agent version for test_txn
#
tests/test_txn.o: EXTRA_CFLAGS += -DNR_VERSION="\"$(AGENT_VERSION)\""
tests/test_txn.o: ../VERSION

#
# Used when linking test binaries.
#
TEST_LIBS := $(PHP_EMBED_LIBRARY) $(shell $(PHP_CONFIG) --libs)
TEST_LDFLAGS := $(shell $(PHP_CONFIG) --ldflags) $(EXPORT_DYNAMIC)
TEST_LDFLAGS += $(USER_LDFLAGS)
CROSS_AGENT_DIR := $(CURDIR)/../axiom/tests/cross_agent_tests
EXTRA_CFLAGS += -DCROSS_AGENT_TESTS_DIR="\"$(CROSS_AGENT_DIR)\""

#
# Implicit rule to build test object files with the appropriate flags.
#
tests/%.o: tests/%.c
	$(CC) $(NEWRELIC_CFLAGS) $(COMMON_FLAGS) $(CFLAGS_CLEAN) $(EXTRA_CFLAGS) -DDEBUG -MMD -MP -c $< -o $@

#
# Implicit rule to build test binaries.
#
tests/test_%: tests/test_%.o $(TLIB_OBJS) $(PHP_MODULES)
	$(CC) -o $@ $(LDFLAGS) $(TEST_LDFLAGS) $< $(PHP_EMBED_LIBRARY) $(TLIB_OBJS) $(wildcard .libs/*.o) $(LIBS) $(EXTRA_LIBS) $(TEST_LIBS) $(TEST_NEWRELIC_SHARED_LIBADD)

#
# Secondary declaration to prevent the intermediate .o files from being
# deleted, causing double compilation.
#
.SECONDARY: $(TLIB_OBJS) $(TEST_BINARIES:%=%.o)

#
# Include dependency files. See axiom/Makefile for a discussion of how this
# works.
#
-include $(TLIB_OBJS:.o=.d)
-include $(TEST_BINARIES:%=%.d)

#
# Getting "make clean" to remove the unit test build products is difficult, as
# PHP's build system uses single-colon rules for clean and friends. However, it
# does include a whole range of variables that have no meaning for single
# extension builds, so we'll redefine one of the variables to delete what we
# want deleted.
#
# This does mean that our extension can't be built within php-src's ext/
# directory structure. We've never supported (or done) that anyway, but now it
# really won't work.
#
OVERALL_TARGET := $(TEST_BINARIES) tests/*.o tests/*.d

# vim: set noet ts=2 sw=2:
