<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* JIT only supported with OAPI with PHP 8+ */
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0.0 not supported\n");
}

/* Test cannot run without opache */
/* This should be changed to a 'warn' result when supported */
if (!extension_loaded('Zend OPcache')) {
  die("skip: Zend OPcache extension required\n");
}
