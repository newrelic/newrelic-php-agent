<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

if (!extension_loaded("mysql")) {
  die("skip: mysql extension required");
}

$link = @mysql_connect($MYSQL_SERVER, $MYSQL_USER, $MYSQL_PASSWD);
if (FALSE === $link) {
  die("skip: " . mysql_error() . "\n");
}

if (!empty($MYSQL_DB)) {
  if (FALSE === mysql_select_db($MYSQL_DB, $link)) {
    die("skip: " . mysql_error() . "\n");
  }
}

mysql_close($link);
