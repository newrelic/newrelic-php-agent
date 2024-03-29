<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

if (version_compare(PHP_VERSION, "8.2", ">=")) {
  die("skip: PHP >= 8.2 not supported\n");
}

require_once(realpath(dirname(__FILE__)) . '/../../../include/drupal.php');

if (!download_drupal(7)) {
    die("warn: drupal7 installation required\n");
}
