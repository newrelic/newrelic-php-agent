<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

if (version_compare(PHP_VERSION, "7.4", ">")) {
  die("skip: PHP > 7.4.0 not supported\n");
}

require_once(realpath(dirname(__FILE__)) . '/../../../include/drupal.php');

if (!download_drupal(6)) {
    die("warn: drupal6 installation required\n");
}
