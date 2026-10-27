<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A statement prepared after an earlier statement was freed, and that took the
freed statement's object handle, SHALL be reported with its own SQL.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
newrelic.transaction_tracer.explain_enabled = false
*/

/*EXPECT
executed
the second statement took the handle of the first
executed
*/

/*EXPECT_METRICS_EXIST
Datastore/statement/MySQL/tables/select, 1
Datastore/statement/MySQL/columns/select, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/operation/MySQL/other
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
$columns = "SELECT COLUMN_NAME FROM information_schema.columns WHERE column_name = 'TABLE_NAME'";

$first = $link->prepare($tables);
$first_id = spl_object_id($first);
$first->execute();
echo "executed\n";
$first->close();
unset($first);

$second = $link->prepare($columns);
if (spl_object_id($second) === $first_id) {
  echo "the second statement took the handle of the first\n";
}
$second->execute();
echo "executed\n";
$second->close();
