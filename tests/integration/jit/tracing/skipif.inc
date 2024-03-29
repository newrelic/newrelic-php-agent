<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */


/* JIT only supported with OAPI with PHP 8+ */
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0.0 not supported\n");
}

/* Test cannot run without opcache */
if (!extension_loaded('Zend OPcache')) {
  die("warn: Zend OPcache extension required\n");
}
