<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

if (version_compare(PHP_VERSION, '5.5', '<')) {
    die("skip: PHP >= 5.5 required\n");
}

if (version_compare(PHP_VERSION, '7.4', '>=')) {
  die("skip: PHP < 7.4 required\n");
}

require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');

if (!unpack_guzzle(5)) {
    die("warn: guzzle 5 installation required\n");
}
