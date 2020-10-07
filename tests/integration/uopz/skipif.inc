<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

require __DIR__.'/load.inc';

if (!extension_loaded("uopz")) {
  die("skip: uopz extension required");
}

/* These tests are only for the PHP 7 version of uopz. */
if (version_compare(PHP_VERSION, '7.0.0', '<')) {
  die("skip: requires PHP 7.0+\n");
}
