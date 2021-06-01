#!/bin/bash

#
# Copyright 2021 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#

#
# Ensure correct paths.
#

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
