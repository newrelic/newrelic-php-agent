#
# Copyright 2020 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#

#
# Platform specific configuration.
#

#
# OS-specific defines that axiom needs. These are:
#
# NR_SYSTEM_*: The operating system being compiled on.
#

UNAME := $(shell uname)

#
# Detect platform specific features. Autotools is the right way to do
# this, but our needs are simple and the variation amoung our supported
# platforms is narrow.
#

# Whether the compiler is Clang/LLVM.
HAVE_CLANG := $(shell echo __clang__ | $(CC) -E - | tail -1)

# Whether you have alloca.h
HAVE_ALLOCA_H := $(shell test -e /usr/include/alloca.h && echo 1 || echo 0)

# Whether you have /dev/fd
HAVE_DEV_FD := $(shell test -d /dev/fd && echo 1 || echo 0)

# Whether you have /proc/self/fd
HAVE_PROC_SELF_FD := $(shell test -d /proc/self/fd && echo 1 || echo 0)

# Whether you have closefrom
HAVE_CLOSEFROM := 0

# Whether you have backtrace and backtrace_symbols_fd
HAVE_BACKTRACE := $(shell $(CC) $(dir $(abspath $(lastword $(MAKEFILE_LIST))))backtrace_test.c -o /dev/null 2>&1 1>/dev/null && echo 1 || echo 0)

# Whether you have libexecinfo
HAVE_LIBEXECINFO := $(shell test -e /usr/lib/libexecinfo.so -o -e /usr/lib/libexecinfo.a && echo 1 || echo 0)

# Whether you have protoc-c and libprotobuf-c.a
# Look in /usr by default but can be overriden from environment)
VENDOR_PREFIX ?= /usr
HAVE_PROTOBUF_C := $(shell test -x $(VENDOR_PREFIX)/bin/protoc-c && find $(VENDOR_PREFIX)/lib -name 'libprotobuf-c.a' | grep -q 'libprotobuf-c.a' && echo 1 || echo 0)

# Whether you have PTHREAD_MUTEX_ERRORCHECK
#
# The interesting dir/abspath/lastword/$(MAKEFILE_LIST) construct is required to
# get the path to the directory this Makefile lives in, since that's where
# pthread_test.c lives, and it is included from a variety of working directories
# around this project.
#
# Note that there is no way to actually get errors out of this if there's a
# problem besides the one we're looking for (a missing constant): all errors
# will be incorporated into this variable, which is then unused besides testing
# it against the literal string "1". We are pushing make in some interesting
# directions here. If this is acting funny, the first thing you should try is
# adding a $(info $(HAVE_PTHREAD_MUTEX_ERRORCHECK)) line immediately under the
# line that sets $(HAVE_PTHREAD_MUTEX_ERRORCHECK) below to see what output
# actually came back from the C compiler.
#
# This continues to tie us ever closer to GNU make; neither the POSIX
# specification nor BSD implementation support any of these functions.
HAVE_PTHREAD_MUTEX_ERRORCHECK := $(shell $(CC) $(dir $(abspath $(lastword $(MAKEFILE_LIST))))pthread_test.c -o /dev/null -pthread 2>&1 1>/dev/null && echo 1 || echo 0)

# Whether reallocarray() is available from the standard library.
#
# The same notes above apply to this check.
HAVE_REALLOCARRAY := $(shell $(CC) $(dir $(abspath $(lastword $(MAKEFILE_LIST))))reallocarray_test.c -o /dev/null 2>&1 1>/dev/null && echo 1 || echo 0)

ifeq (Darwin,$(UNAME))
  OS := Darwin
  ARCH := $(shell uname -m | sed -e 's/i386/x86/' -e 's/x86_64/x64/')
  PLATFORM_DEFS := -DNR_SYSTEM_DARWIN=1
endif

ifeq (FreeBSD,$(UNAME))
  OS := FreeBSD
  ARCH := $(shell uname -p | sed -e 's/i386/x86/' -e 's/amd64/x64/')
  PLATFORM_DEFS := -DNR_SYSTEM_FREEBSD=1
  HAVE_CLOSEFROM := 1
endif

ifeq (Linux,$(UNAME))
  OS := Linux
  ARCH := $(shell uname -m | sed -e 's/i[3456]86/x86/' -e 's/x86_64/x64/')
  PLATFORM_DEFS := -DNR_SYSTEM_LINUX=1
endif

ifeq (x86,$(ARCH))
  # Minimum CPU requirement for the agent on x86 is SSE2.
  MODEL_FLAGS := -m32 -msse2 -mfpmath=sse

  ifeq (Darwin,$(UNAME))
    MODEL_FLAGS += -arch i386
  endif
endif

ifeq (x64,$(ARCH))
  MODEL_FLAGS := -m64

  ifeq (Darwin,$(UNAME))
    MODEL_FLAGS += -arch x86_64
  endif
endif

ifeq (1,$(HAVE_ALLOCA_H))
  PLATFORM_DEFS += -DHAVE_ALLOCA_H=1
endif

ifeq (1,$(HAVE_BACKTRACE))
  PLATFORM_DEFS += -DHAVE_BACKTRACE=1
endif

ifeq (1,$(HAVE_CLOSEFROM))
  PLATFORM_DEFS += -DHAVE_CLOSEFROM=1
endif

ifeq (1,$(HAVE_DEV_FD))
  PLATFORM_DEFS += -DHAVE_DEV_FD=1
endif

ifeq (1,$(HAVE_PROC_SELF_FD))
  PLATFORM_DEFS += -DHAVE_PROC_SELF_FD=1
endif

ifeq (1,$(HAVE_PTHREAD_MUTEX_ERRORCHECK))
  PLATFORM_DEFS += -DHAVE_PTHREAD_MUTEX_ERRORCHECK=1
endif

ifeq (1,$(HAVE_REALLOCARRAY))
  PLATFORM_DEFS += -DHAVE_REALLOCARRAY=1
endif

#
# Code coverage
#
ifeq (1,$(ENABLE_COVERAGE))
  CPPFLAGS += -DDO_GCOV
  CFLAGS += -fprofile-arcs -ftest-coverage
  LDFLAGS += --coverage
endif

#
# Conditionally compile Go files to use the system certs.
#
ifeq (1,$(USE_SYSTEM_CERTS))
	GO_TAGS += use_system_certs
endif

#
# Conditionally compile Go files for integration tests.
#
ifeq (1,$(INTEGRATION_TAGS))
	GO_TAGS += integration
endif

#
# If Go build tags exist prepend the tags flag.
#
ifneq (,$(GO_TAGS))
	GO_TAGS := -tags='$(GO_TAGS)'
endif
