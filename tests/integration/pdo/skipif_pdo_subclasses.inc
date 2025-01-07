<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

if (version_compare(PHP_VERSION, "8.4", "<")) {
  die("skip: PDO subclasses only supported on PHP >= 8.4\n");
}

