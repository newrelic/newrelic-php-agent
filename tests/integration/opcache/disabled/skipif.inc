<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* Opcache only supported with PHP 7+ */
if (version_compare(PHP_VERSION, "7.0", "<")) {
  die("skip: PHP < 7.0.0 not supported\n");
}

/* Test should not run with opache */
if (extension_loaded('Zend OPcache')) {
  die("warn: Zend OPcache extension required\n");
}
