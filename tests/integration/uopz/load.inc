<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

if (!extension_loaded("uopz")) {
  // Attempt to load uopz.
  $result = @dl('uopz.so');
}
