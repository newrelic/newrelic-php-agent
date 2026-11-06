<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A mysqli::execute_query() faster than the explain threshold opens no explain
connection. The server's Connections counter is read through PDO before and
after the call.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (version_compare(PHP_VERSION, "8.2", "<")) {
  die("skip: mysqli_execute_query() exists from PHP 8.2\n");
}
if (!extension_loaded("pdo_mysql")) {
  die("skip: pdo_mysql is needed to read the counter\n");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 600000
newrelic.transaction_tracer.record_sql = obfuscated
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.datastore_tracer.database_name_reporting.enabled = 0
*/

/*EXPECT
explain connections: 0
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

/*
 * The Connections counter is global to the server. This test assumes the
 * integration runner runs one test at a time (its default, -threads 1) and
 * that nothing else uses the server. With -threads above 1, or a server shared
 * with other clients, a connection opened elsewhere between the two reads
 * would be counted and the test would fail.
 */
$dsn = ("" != $MYSQL_SOCKET)
    ? "mysql:unix_socket=$MYSQL_SOCKET;dbname=$MYSQL_DB"
    : "mysql:host=$MYSQL_HOST;port=$MYSQL_PORT;dbname=$MYSQL_DB";
$pdo = new PDO($dsn, $MYSQL_USER, $MYSQL_PASSWD);

function connections($pdo) {
  $row = $pdo->query("SHOW GLOBAL STATUS LIKE 'Connections'")->fetch(PDO::FETCH_NUM);
  return (int)$row[1];
}

$link = new mysqli($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB,
                   $MYSQL_PORT, $MYSQL_SOCKET);
$before = connections($pdo);
$link->execute_query(
    "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name = ?",
    ['STATISTICS']);
$after = connections($pdo);
echo "explain connections: " . ($after - $before) . "\n";
$link->close();
