<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A query that throws mysqli_sql_exception and is caught by the application SHALL
NOT crash the agent when explain plans are enabled.

On PHP 8.1 and later mysqli throws mysqli_sql_exception when a query fails. The
exception is still pending when the agent looks at the finished query, and while
an exception is pending no call into PHP runs. The agent therefore does not try
to explain a query that threw, and the application's exception is untouched.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (version_compare(PHP_VERSION, "8.1", "<")) {
  die("skip: PHP < 8.1.0 not supported\n");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
caught mysqli_sql_exception
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

$link = new mysqli($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB,
                   $MYSQL_PORT, $MYSQL_SOCKET);

try {
  $link->query("SELECT id FROM nr_no_such_table");
} catch (mysqli_sql_exception $e) {
  echo "caught " . get_class($e) . "\n";
}
