<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A statement created with stmt_init() that took the object handle of a freed
statement, and that is executed without being prepared, SHALL NOT be reported
with the SQL of the freed statement.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (PHP_VERSION_ID < 80000) {
  die("skip: executing a statement that was not prepared throws from PHP 8.0\n");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = false
*/

/*EXPECT
executed
the second statement took the handle of the first
execute threw
*/

/*EXPECT_METRICS_EXIST
Datastore/statement/MySQL/tables/select, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/statement/MySQL/tables/select, 2
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/mysqli.inc');

mysqli_report(MYSQLI_REPORT_OFF);

$link = mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB,
                       $MYSQL_PORT, $MYSQL_SOCKET);
if (false === $link) {
  echo "connect failed: " . mysqli_connect_error() . "\n";
  exit(1);
}

$tables = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name = 'TRIGGERS'";

$first = $link->prepare($tables);
$first_id = spl_object_id($first);
$first->execute();
echo "executed\n";
$first->close();
unset($first);

$second = $link->stmt_init();
if (spl_object_id($second) === $first_id) {
  echo "the second statement took the handle of the first\n";
}

try {
  $second->execute();
  echo "execute did not throw\n";
} catch (Error $e) {
  echo "execute threw\n";
}
