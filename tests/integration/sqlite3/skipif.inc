<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

if (!extension_loaded("sqlite3")) {
  die("skip: sqlite3 extension required.");
}
