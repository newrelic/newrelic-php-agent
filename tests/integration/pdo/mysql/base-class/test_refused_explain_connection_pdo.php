<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A refused connection for the explain plan SHALL NOT break the application's
query.

To get an explain plan the agent opens a second connection with the
application's credentials. This test uses a MySQL user that is limited to one
connection, so the application's own connection takes the only slot and the
explain connection is refused.

PDO is not affected: the agent builds the explain connection with
nr_php_call_user_func_catch(), which catches and clears the exception, and the
later calls run in silent error mode. This test guards that behavior. Its
counterpart, test_refused_explain_connection_mysqli.php, shows the same scenario
with mysqli, which throws mysqli_sql_exception into the application's query
unless the agent clears it. The TLS setup is in tests/tls (the user is created by
init.sql); the test is skipped without it and on PHP below 7.4.
*/

/*SKIPIF
<?php
require(realpath (dirname ( __FILE__ )) . '/../../skipif_mysql.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../../include/config.php');
if (PHP_VERSION_ID < 70400) {
  die("skip: the test users need PHP 7.4 or later\n");
}
if ('' == $MYSQL_TLS_HOST) {
  die("skip: MYSQL_TLS_HOST is not set\n");
}
try {
  new PDO("mysql:host=$MYSQL_TLS_HOST;port=$MYSQL_TLS_PORT;dbname=$MYSQL_TLS_DB",
          'nr_one_conn', 'nr_one_conn');
} catch (Exception $e) {
  die("skip: limited MySQL user not available: " . $e->getMessage() . "\n");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
ok - query succeeded
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../../include/config.php');

$pdo = new PDO("mysql:host=$MYSQL_TLS_HOST;port=$MYSQL_TLS_PORT;dbname=$MYSQL_TLS_DB",
               'nr_one_conn', 'nr_one_conn',
               array(PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION));

try {
  $stmt = $pdo->prepare("SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='STATISTICS'");
  $stmt->execute();
  $stmt->fetchAll();
  tap_assert(true, 'query succeeded');
} catch (Throwable $e) {
  echo "exception thrown into the application: " . get_class($e) . ": " . $e->getMessage() . "\n";
}
