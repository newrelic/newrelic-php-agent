#
# Copyright 2020 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#

#
# The Makefile for the axiom test suite. This will work by itself if you've
# built the libaxiom.a target in axiom itself, but generally you'll want
# to call this via the targets that are forwarded from the axiom Makefile,
# which are:
#
# all:       Builds but does not run the tests.
# run_tests: Builds and runs the tests.
# valgrind:  Builds and runs the tests under valgrind.
#
# Useful variables over and above the axiom ones:
#
# VALGRIND:  The valgrind binary to use when running tests under valgrind.
#

#
# Variables. Most of these add to whatever's defined, since in normal operation
# these are imported from the axiom Makefile.
#
VALGRIND ?= valgrind

#
# Operating system detection.
#
include ../../make/config.mk
include ../../make/vendor.mk

CROSS_AGENT_DIR := $(CURDIR)/cross_agent_tests
REFERENCE_DIR := $(CURDIR)/reference

# These flags that are required to build the tests, and should be unioned with
# any flags inherited from the environment.
TEST_CFLAGS := -std=gnu99 -pthread

TEST_CFLAGS += -Wall
TEST_CFLAGS += -Werror
TEST_CFLAGS += -Wextra
TEST_CFLAGS += -Wbad-function-cast
TEST_CFLAGS += -Wcast-qual
TEST_CFLAGS += -Wdeclaration-after-statement
TEST_CFLAGS += -Wimplicit-function-declaration
TEST_CFLAGS += -Wmissing-declarations
TEST_CFLAGS += -Wmissing-prototypes
TEST_CFLAGS += -Wno-write-strings
TEST_CFLAGS += -Wpointer-arith
TEST_CFLAGS += -Wshadow
TEST_CFLAGS += -Wstrict-prototypes
TEST_CFLAGS += -Wswitch-enum

ifeq (1,$(HAVE_CLANG))
TEST_CFLAGS += -Wbool-conversion
TEST_CFLAGS += -Wempty-body
TEST_CFLAGS += -Wheader-hygiene
TEST_CFLAGS += -Wimplicit-fallthrough
TEST_CFLAGS += -Wlogical-op-parentheses
TEST_CFLAGS += -Wloop-analysis
TEST_CFLAGS += -Wsizeof-array-argument
TEST_CFLAGS += -Wstring-conversion
TEST_CFLAGS += -Wswitch
TEST_CFLAGS += -Wswitch-enum
TEST_CFLAGS += -Wuninitialized
TEST_CFLAGS += -Wunused-label
TEST_CFLAGS += -Wno-typedef-redefinition
endif

TEST_CPPFLAGS := -I.. -DCROSS_AGENT_TESTS_DIR="\"$(CROSS_AGENT_DIR)\"" -DREFERENCE_DIR="\"$(REFERENCE_DIR)\"" $(PLATFORM_DEFS)
TEST_LDFLAGS :=
TEST_LDLIBS := -L. -ltlib -L.. -laxiom

# -pthread must be passed to the compiler, but not the linker when using Clang.
ifneq (1,$(HAVE_CLANG))
TEST_LDFLAGS += -pthread
endif

ifeq (1,$(HAVE_LIBEXECINFO))
  TEST_LDLIBS += -lexecinfo
endif

# We have a few platform specific libraries that have to be linked in.
# GNU/Linux requires -ldl for dladdr(). Everything except macOS requires -lm 
# for pow().
#
# We'll test these with a set of simple one liners, in the same style as
# make/config.mk.
ifeq (Linux,$(UNAME))
  ifeq (1,$(shell (echo "int dladdr(void *, void *); int main() { dladdr(0, 0); return 0; }" | $(CC) -x c -o /dev/null - 1>/dev/null 2>/dev/null) && echo 0 || echo 1))
    TEST_LDLIBS += -ldl
  endif
  TEST_LDLIBS += -lm
endif

ifeq (FreeBSD,$(UNAME))
  TEST_LDLIBS += -lm
endif

#
# Flags required to link PCRE.
#
PCRE_CFLAGS := -I$(PCRE_PREFIX)/include
ifeq (yes,$(PCRE_STATIC))
  ifneq (,$(findstring /opt/nr/camp,$(shell pcre-config --prefix)))
    # Special handling for legacy build environments.
    PCRE_LDLIBS := -L$(PCRE_PREFIX)/lib -lnrpcre-pic
  else
    PCRE_LDLIBS := -L$(PCRE_PREFIX)/lib -l:libpcre.a
  endif
else 
PCRE_LDLIBS := -L$(PCRE_PREFIX)/lib -lpcre
endif

all: tests

#
# Tests. Add a binary to this list to have it built by default. Note that the
# file name must start with test_.
#
TESTS := \
  test_agent \
  test_analytics_events \
  test_apdex \
  test_app \
  test_app_harvest \
  test_attributes \
  test_base64 \
  test_buffer \
  test_cmd_appinfo \
  test_cmd_span_batch \
  test_cmd_txndata \
  test_configstrings \
  test_custom_events \
  test_datastore \
  test_datastore_instance \
  test_daemon_spawn \
  test_distributed_trace \
  test_errors \
  test_exclusive_time \
  test_explain \
  test_file_naming \
  test_flatbuffers \
  test_guid \
  test_hash \
  test_hashmap \
  test_header \
  test_json \
  test_labels \
  test_log_event \
  test_log_events \
  test_log_level \
  test_logging \
  test_matcher \
  test_math \
  test_memory \
  test_metrics \
  test_minmax_heap \
  test_mysqli_metadata \
  test_network \
  test_number_converter \
  test_obfuscate \
  test_object \
  test_postgres \
  test_random \
  test_regex \
  test_reply \
  test_rules \
  test_rum \
  test_sampling \
  test_segment \
  test_segment_children \
  test_segment_datastore \
  test_segment_external \
  test_segment_private \
  test_segment_terms \
  test_segment_traces \
  test_segment_tree \
  test_serialize \
  test_set \
  test_signals \
  test_slab \
  test_slowsqls \
  test_sort \
  test_span_encoding \
  test_span_event \
  test_span_queue \
  test_sql \
  test_stack \
  test_string_pool \
  test_strings \
  test_synthetics \
  test_system \
  test_text \
  test_threads \
  test_time \
  test_txn \
  test_url \
  test_vector

#
# The list of tests to skip and tests to run.
#
SKIP_TESTS =
RUNNABLE_TESTS = $(filter-out $(SKIP_TESTS),$(TESTS))

#
# Implicit rule to build object files. We define them as precious to avoid
# recompilation; we're figuring out dependencies anyway, so we'll want to keep
# them.
#
.PRECIOUS: %.o
%.o: %.c Makefile .deps/compile_flags
	$(CC) $(TEST_CPPFLAGS) $(CPPFLAGS) $(TEST_CFLAGS) $(PCRE_CFLAGS) $(VENDOR_CFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

#
# Objects that will be built into the libtlib.a utility library that all tests
# link against.
#
TLIB_OBJS := \
	tlib_bool.o \
	tlib_exec.o \
	tlib_files.o \
	tlib_main.o

#
# The rule to actually build the library.
#
libtlib.a: $(TLIB_OBJS) Makefile
	$(AR) rcs $@ $^

#
# The top level phony rule to build the tests.
#
.PHONY: tests
tests: $(TESTS)

#
# All test binaries depend on libtlib.a, and assume that libaxiom.a exists
# in the directory above.
#
test_%: test_%.o libtlib.a ../libaxiom.a Makefile .deps/link_flags
	$(CC) $(TEST_LDFLAGS) $(LDFLAGS) -o $@ $< $(TEST_LDLIBS) $(PCRE_LDLIBS) $(VENDOR_LDFLAGS) $(VENDOR_LDLIBS) $(LDLIBS)

#
# The top level rule to run the tests.
#
.PHONY: check run_tests
check run_tests: $(RUNNABLE_TESTS:%=%.phony) Makefile | tests

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
	@./$< $(TESTARGS)

#
# Valgrind variables. These aren't defined at the top due to the extreme
# unlikeliness that we'll ever want to override them at runtime, but can be
# overridden if you really want to.
#
CHECK_VALGRIND_OUTPUT ?= $(CURDIR)/../../make/check_valgrind_output.awk
VALGRIND_SUPPRESSIONS ?= $(CURDIR)/valgrind-suppressions

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
# CHECK_VALGRIND_OUTPUT.
#
.PHONY: valgrind
valgrind: tests Makefile
	@for T in $(RUNNABLE_TESTS); do \
	   $(VALGRIND) --tool=memcheck --num-callers=30 --dsymutil=yes --leak-check=full --show-reachable=yes --suppressions=$(VALGRIND_SUPPRESSIONS) --log-file=$$T.valgrind.log ./$$T $(TESTARGS); \
	   TEST_EXIT_CODE=$$?; \
	   VALGRIND_RESULTS=$$(VALGRIND_VERBOSE=1 $(CHECK_VALGRIND_OUTPUT) $$T.valgrind.log); \
	   printf "%42s with %s\n" ' ' "$$VALGRIND_RESULTS"; \
	   test "$$TEST_EXIT_CODE" -ne 0 && exit "$$TEST_EXIT_CODE"; \
(	   echo "$$VALGRIND_RESULTS" | grep -Eqv '^0 errors from 0 contexts') && exit 1; \
	done; \
	exit 0;

#
# Track the flags passed to the compiler to force a rebuild when they change.
# These rules must kept in sync with the pattern rules used to perform the
# compilation and linking.
#
# The trick here is forcing the .deps/*_flags targets to be re-evaluated for
# each build while ensuring they are only out of date if their contents need
# to be updated. We use a PHONY dependency to do so.
#
.PHONY: force
.deps/compile_flags: force | .deps/
	@echo '$(TEST_CPPFLAGS) $(CPPFLAGS) $(TEST_CFLAGS) $(PCRE_CFLAGS) $(VENDOR_CFLAGS) $(CFLAGS) -MMD -MP' | cmp -s - $@ || echo '$(TEST_CPPFLAGS) $(CPPFLAGS) $(TEST_CFLAGS) $(PCRE_CFLAGS) $(VENDOR_CFLAGS) $(CFLAGS) -MMD -MP' > $@

.deps/link_flags: force | .deps/
	@echo '$(TEST_LDFLAGS) $(LDFLAGS) $(VENDOR_LDFLAGS) $(VENDOR_LDLIBS) $(TEST_LDLIBS) $(PCRE_LDLIBS) $(LDLIBS)' | cmp -s - $@ || echo '$(TEST_LDFLAGS) $(LDFLAGS) $(VENDOR_LDFLAGS) $(VENDOR_LDLIBS) $(TEST_LDLIBS) $(PCRE_LDLIBS) $(LDLIBS)' > $@

.deps/:
	@mkdir .deps

#
# Clean up build products: that's object files, dependency files, libtlib.a,
# and any output files.
#
clean:
	rm -f *.gcov *.gcno *.gcda
	rm -f libtlib.a *.d *.o *.valgrind.log $(TESTS)
	rm -rf .deps *.dSYM

#
# Dependency handling. When we build a .o file, we also build a .d file
# containing that module's dependencies using -MM. Those files are in Makefile
# format, so we include them here to define dependency rules: this means that
# if we change a header, all affected modules will be recompiled automatically.
#
-include $(TLIB_OBJS:.o=.d)
-include $(TESTS:%=%.d)
