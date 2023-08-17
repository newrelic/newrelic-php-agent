<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

if (version_compare(PHP_VERSION, '5.5', '<')) {
    die("skip: PHP >= 5.5 required\n");
}

if (version_compare(PHP_VERSION, '8.0', '>=')) {
  die("skip: PHP < 8.0 required\n");
}

require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');

if (!unpack_guzzle(6)) {
    die("warn: guzzle 6 installation required\n");
}
