<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should not cause a deprecation warning to be thrown when passing null
to optional arguments for mysqli_stmt:__construct on PHP 8.1+
*/

/*SKIPIF
<?php require("skipif.inc");

if (version_compare(PHP_VERSION, "8.1", "<")) {
  die("skip: Deprecation warning PHP 8.1+ specific\n");
}
*/

/*INI
error_reporting = E_ALL
*/

/*EXPECT
STATISTICS
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

function test_stmt_prepare($link)
{
  $stmt = new mysqli_stmt($link, null);
  $name = 'STATISTICS';

  $query = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?";
  if (FALSE === $stmt->prepare($query) ||
      FALSE === $stmt->bind_param('s', $name) ||
      FALSE === $stmt->execute() ||
      FALSE === $stmt->bind_result($name)) {
    echo mysqli_stmt_error($stmt) . "\n";
    mysqli_stmt_close($stmt);
    return;
  }

  while (mysqli_stmt_fetch($stmt)) {
    echo $name . "\n";
  }

  mysqli_stmt_close($stmt);
}

$link = mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if (mysqli_connect_errno()) {
  echo mysqli_connect_error() . "\n";
  exit(1);
}

test_stmt_prepare($link);
mysqli_close($link);

