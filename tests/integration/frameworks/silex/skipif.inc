<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* PHP 5.3.3 is the oldest version Silex 1.0 supported. */
if (version_compare(PHP_VERSION, '5.3.3', '<')) {
  die('skip: PHP 5.3.3 or later required');
}
