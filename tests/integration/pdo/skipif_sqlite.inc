<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

if (!extension_loaded('pdo')) {
  die("skip: pdo extension required\n");
}

if (!extension_loaded('pdo_sqlite')) {
  die("skip: pdo_sqlite extension required\n");
}
