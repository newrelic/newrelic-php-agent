#
# Copyright 2020 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#

#
# The Makefile for axiom, a standalone library for communicating with New Relic
#
# Useful targets:
#
# all:       Builds libaxiom.a.
# clean:     Removes all build products.
# tests:     Builds but does not run the tests.
# run_tests: Builds and runs the tests.
# valgrind:  Builds and runs the tests under valgrind.
#
# Useful variables:
#
# AR:     The archiver used to build a static library (default: ar).
# CC:     The C compiler to use (default: gcc).
# CFLAGS: Flags to give the C compiler when building object files.
#

include ../make/config.mk
include ../make/vendor.mk
include ../make/version.mk

# These flags are required to build axiom, and should be unioned with any
# flags inherited from the environment.
AXIOM_CPPFLAGS := $(PLATFORM_DEFS)
AXIOM_CFLAGS := -std=gnu99 -fPIC -DPIC -pthread

AXIOM_CFLAGS += -Wall
AXIOM_CFLAGS += -Werror
AXIOM_CFLAGS += -Wextra
AXIOM_CFLAGS += -Wbad-function-cast
AXIOM_CFLAGS += -Wcast-qual
AXIOM_CFLAGS += -Wdeclaration-after-statement
AXIOM_CFLAGS += -Wformat
AXIOM_CFLAGS += -Wformat-security
AXIOM_CFLAGS += -Wimplicit-function-declaration
AXIOM_CFLAGS += -Wmissing-declarations
AXIOM_CFLAGS += -Wmissing-prototypes
AXIOM_CFLAGS += -Wno-write-strings
AXIOM_CFLAGS += -Wpointer-arith
AXIOM_CFLAGS += -Wshadow
AXIOM_CFLAGS += -Wstrict-prototypes
AXIOM_CFLAGS += -Wswitch-enum

ifeq (1,$(HAVE_CLANG))
AXIOM_CFLAGS += -Wbool-conversion
AXIOM_CFLAGS += -Wempty-body
AXIOM_CFLAGS += -Wheader-hygiene
AXIOM_CFLAGS += -Wimplicit-fallthrough
AXIOM_CFLAGS += -Wlogical-op-parentheses
AXIOM_CFLAGS += -Wloop-analysis
AXIOM_CFLAGS += -Wsizeof-array-argument
AXIOM_CFLAGS += -Wstring-conversion
AXIOM_CFLAGS += -Wswitch
AXIOM_CFLAGS += -Wswitch-enum
AXIOM_CFLAGS += -Wuninitialized
AXIOM_CFLAGS += -Wunused-label
AXIOM_CFLAGS += -Wno-typedef-redefinition
AXIOM_CFLAGS += -Wno-missing-field-initializers
endif

# OS X 10.11 (at least) does not provide pcre-config by default.
# Check whether it exists, and if not assume a sensible default.
PCRE_CFLAGS := $(shell pcre-config --cflags)

#
# Correct agent version handling.
#
VERSION_FLAGS := -DNR_VERSION=$(AGENT_VERSION) -DNR_COMMIT=$(GIT_COMMIT)

#
# Object files that we have to build. If you're adding a new C file to axiom,
# you need to add a corresponding line here.
#
# Note: v1.pb-c.o is built first in order to generate v1.pb-c.h which is needed
# by at least nr_span_encoding.c.
OBJS := \
	v1.pb-c.o \
	cmd_appinfo_transmit.o \
	cmd_span_batch_transmit.o \
	cmd_txndata_transmit.o \
	nr_agent.o \
	nr_analytics_events.o \
	nr_app.o \
	nr_app_harvest.o \
	nr_attributes.o \
	nr_banner.o \
	nr_configstrings.o \
	nr_custom_events.o \
	nr_daemon_spawn.o \
	nr_datastore.o \
	nr_datastore_instance.o \
	nr_distributed_trace.o \
	nr_errors.o \
	nr_exclusive_time.o \
	nr_explain.o \
	nr_file_naming.o \
	nr_guid.o \
	nr_header.o \
	nr_log_event.o \
	nr_log_events.o \
	nr_log_level.o \
	nr_mysqli_metadata.o \
	nr_postgres.o \
	nr_rules.o \
	nr_rum.o \
	nr_segment.o \
	nr_segment_children.o \
	nr_segment_datastore.o \
	nr_segment_external.o \
	nr_segment_private.o \
	nr_segment_terms.o \
	nr_segment_traces.o \
	nr_segment_tree.o \
	nr_slowsqls.o \
	nr_span_encoding.o \
	nr_span_event.o \
	nr_span_queue.o \
	nr_synthetics.o \
	nr_txn.o \
	nr_version.o \
	util_apdex.o \
	util_base64.o \
	util_buffer.o \
	util_cpu.o \
	util_errno.o \
	util_flatbuffers.o \
	util_hash.o \
	util_hashmap.o \
	util_json.o \
	util_logging.o \
	util_labels.o \
	util_matcher.o \
	util_md5.o \
	util_memory.o \
	util_metrics.o \
	util_minmax_heap.o \
	util_network.o \
	util_number_converter.o \
	util_obfuscate.o \
	util_object.o \
	util_random.o \
	util_regex.o \
	util_reply.o \
	util_serialize.o \
	util_set.o \
	util_signals.o \
	util_slab.o \
	util_sleep.o \
	util_sort.o \
	util_sql.o \
	util_stack.o \
	util_string_pool.o \
	util_strings.o \
	util_strings_bsd.o \
	util_syscalls.o \
	util_system.o \
	util_text.o \
	util_threads.o \
	util_url.o \
	util_vector.o

#
# The rule to actually build the library.
#
libaxiom.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

#
# Implicit build rule for .o files. Only the first line actually builds the .o
# file; the remainder are entirely used for generating dependencies and
# ensuring that the rules are correct (that the specific .o file depends on all
# .c and .h files).
#
%.o: %.c Makefile .deps/compile_flags
	$(CC) $(AXIOM_CPPFLAGS) $(CPPFLAGS) $(AXIOM_CFLAGS) $(PCRE_CFLAGS) $(VENDOR_CFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

#
# Handle nr_version.o separately to minimize the recompilation necessary
# when the version number or commit SHA changes.
#
nr_version.o: nr_version.c Makefile .deps/compile_flags .deps/version_flags
	$(CC) $(AXIOM_CPPFLAGS) $(VERSION_FLAGS) $(CPPFLAGS) $(AXIOM_CFLAGS) $(PCRE_CFLAGS) $(VENDOR_CFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

nr_config.h: configure Makefile
	./configure > $@

#
# Build the 8T protobuf code.
#
v1.pb-c.c: v1.proto
	$(PROTOBUF_C_PREFIX)/bin/protoc-c --c_out=. $<

#
# The generated code is not warning-free with current compilers, so we'll
# override the implicit build rule to disable whatever warnings we need to.
#
v1.pb-c.o: v1.pb-c.c
	$(CC) $(AXIOM_CPPFLAGS) $(CPPFLAGS) $(AXIOM_CFLAGS) $(PCRE_CFLAGS) $(VENDOR_CFLAGS) $(CFLAGS) -MMD -MP -Wno-cast-qual -c $< -o $@

#
# Track the flags passed to the compiler to force a rebuild when they change.
# This ensures a rebuild occurs when the version number or commit are updated.
# These rules must kept in sync with the pattern rules used to perform the
# actual compilation.
#
# The trick here is forcing the .deps/*_flags targets to be re-evaluated for
# each build while ensuring they are only out of date if their contents need
# to be updated. We use a PHONY dependency to do so.
#
.PHONY: force
.deps/compile_flags: force | .deps/
	@echo '$(AXIOM_CPPFLAGS) $(CPPFLAGS) $(AXIOM_CFLAGS) $(PCRE_CFLAGS) $(VENDOR_CFLAGS) $(CFLAGS)' | cmp -s - $@ || echo '$(AXIOM_CPPFLAGS) $(CPPFLAGS) $(AXIOM_CFLAGS) $(PCRE_CFLAGS) $(VENDOR_CFLAGS) $(CFLAGS)' > $@

.deps/version_flags: force | .deps/
	@echo '$(VERSION_FLAGS)' | cmp -s - $@ || echo '$(VERSION_FLAGS)' > $@

.deps/:
	@mkdir -p .deps

#
# Clean up build products: that's object files, dependency files, and the
# static library itself.
#
.PHONY: clean
clean:
	rm -f *.o *.d libaxiom.a nr_config.h *.pb-c.[ch]
	rm -f *.gcov *.gcno *.gcda
	rm -rf .deps
	$(MAKE) -C tests clean

#
# Targets that we want tests/Makefile to handle.
#
export CC CFLAGS CPPFLAGS LDFLAGS

.PHONY: tests
tests: libaxiom.a
	$(MAKE) -C tests

.PHONY: check run_tests
check run_tests: libaxiom.a
	$(MAKE) -C tests $@

.PHONY: valgrind
valgrind: libaxiom.a
	$(MAKE) -C tests valgrind

#
# Dependency handling. When we build a .o file, we also build a .d file
# containing that module's dependencies using -MM. Those files are in Makefile
# format, so we include them here to define dependency rules: this means that
# if we change a header, all affected modules will be recompiled automatically.
#
-include $(OBJS:.o=.d)

# vim: set noet ts=2 sw=2:
