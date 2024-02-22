<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* Opcache only supported with PHP 7+ */
if (version_compare(PHP_VERSION, "7.1", "<")) {
  die("skip: PHP < 7.1 not supported\n");
}
