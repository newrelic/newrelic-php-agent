<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A transaction that the __toString() method of a bound parameter ends and starts
again while mysqli_stmt_execute() runs SHALL NOT break the agent, and the
statement SHALL NOT be recorded in either transaction.

The use of freed memory this guards against is only detected under valgrind.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
transaction restarted
done
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/operation/MySQL/select
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

class EndsTransaction {
  function __toString() {
    newrelic_end_transaction();
    newrelic_start_transaction(ini_get("newrelic.appname"));
    echo "transaction restarted\n";
    return "1";
  }
}

mysqli_report(MYSQLI_REPORT_OFF);
$link = new mysqli($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB,
                   $MYSQL_PORT, $MYSQL_SOCKET);
$stmt = $link->prepare('SELECT ?');
$param = new EndsTransaction();
$stmt->bind_param('s', $param);
$stmt->execute();
echo "done\n";
$stmt->close();
$link->close();
