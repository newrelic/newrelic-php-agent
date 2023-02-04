#
# Copyright 2020 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
#
# Variables for consuming vendored libraries.
#
# Note that this does _not_ provide build rules to actually build the
# libraries, just the variables to consume them.
#
# It is recommended to use the VENDOR_... variables.
#

# We need to find where the project's vendored dependencies live for these
# variables.
ifeq (0,$(HAVE_PROTOBUF_C))
VENDOR_PREFIX := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST))))../vendor/local)
endif

#
# protobuf-c
#
# Note that this does not require protobuf, which is a build time dependency
# only.
#
PROTOBUF_C_CFLAGS := -I$(VENDOR_PREFIX)/include
PROTOBUF_C_LDFLAGS := -L$(VENDOR_PREFIX)/lib
# Always link to static library
PROTOBUF_C_LDLIBS := -l:libprotobuf-c.a

#
# Aggregated flag variables for use in other Makefiles.
#
VENDOR_CFLAGS := $(PROTOBUF_C_CFLAGS)
VENDOR_LDFLAGS := $(PROTOBUF_C_LDFLAGS)
VENDOR_LDLIBS := $(PROTOBUF_C_LDLIBS)
