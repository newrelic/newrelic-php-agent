<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

if (version_compare(PHP_VERSION, '8.1', '<')) {
  die("skip: PHP >= 8.1 required\n");
}

require_once(realpath(dirname(__FILE__)) . '/../../../include/monolog.php');

if (!unpack_monolog(3)) {
    die("skip: monolog3 installation required\n");
}
