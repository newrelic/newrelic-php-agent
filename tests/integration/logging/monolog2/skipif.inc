<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

if (version_compare(PHP_VERSION, '7.2', '<')) {
  die("skip: PHP >= 7.2 required\n");
}

require_once(realpath(dirname(__FILE__)) . '/../../../include/monolog.php');

if (!unpack_monolog(2)) {
    die("skip: monolog2 installation required\n");
}
