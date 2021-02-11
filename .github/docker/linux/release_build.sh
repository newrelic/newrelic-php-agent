#!/bin/bash

#
# Copyright 2020 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#

echo php $PHP_VER
echo arch $ARCH
make -j $(nproc) clean
make -r -j $(nproc) release-${PHP_VER}-gha "OPTIMIZE=1" "ARCH=${ARCH}"
