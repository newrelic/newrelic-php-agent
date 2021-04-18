<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

if (!extension_loaded("mysqli")) {
  die("skip: mysqli extension required");
}

/*
 * Prevent triggered errors/warnings/notices from interfering with the
 * test runner's handling of SKIPIF, which requires the "skip: ..."
 * message to be the first line of output. This is trickier that it
 * ought to be. With the mysqlnd driver, it's necessary to set the
 * reporting mode to strict, AND to apply the error control operator '@'
 * the connect call.
 */

mysqli_report(MYSQLI_REPORT_STRICT);   // prevent warning if connect fails

try {
  $link = @mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
  mysqli_close($link);
} catch (Exception $e) {
  die("skip: " . $e->getMessage() . "\n");
}
