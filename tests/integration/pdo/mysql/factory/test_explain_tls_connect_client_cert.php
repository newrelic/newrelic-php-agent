<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A slow query on a PDO connection created with Pdo\Mysql::connect() to a MySQL
server that requires a client certificate SHALL get an explain plan.

The agent opens its own connection to run EXPLAIN, and the user for this test
requires a client certificate, so that connection is refused unless it gets the
key, certificate and CA that the application gave to Pdo\Mysql::connect(). The
connect() method of a driver subclass has its own record in the agent; this test
exercises the one for Pdo\Mysql, with the options of a client certificate. The
TLS setup is in tests/tls; the test is skipped without it and on PHP below 8.4.
*/

/*SKIPIF
<?php
require(realpath (dirname ( __FILE__ )) . '/../../skipif_mysql.inc');
require(realpath (dirname ( __FILE__ )) . '/../../skipif_pdo_subclasses.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../../include/config.php');
if ('' == $MYSQL_TLS_HOST) {
  die("skip: MYSQL_TLS_HOST is not set\n");
}
try {
  Pdo\Mysql::connect("mysql:host=$MYSQL_TLS_HOST;port=$MYSQL_TLS_PORT;dbname=$MYSQL_TLS_DB",
               'tls_clientcert', 'tls_clientcert',
               array(Pdo\Mysql::ATTR_SSL_CA => "$MYSQL_TLS_CERTS/ca.pem",
                     Pdo\Mysql::ATTR_SSL_KEY => "$MYSQL_TLS_CERTS/client-key.pem",
                     Pdo\Mysql::ATTR_SSL_CERT => "$MYSQL_TLS_CERTS/client-cert.pem"));
} catch (Exception $e) {
  die("skip: TLS MySQL server not available: " . $e->getMessage() . "\n");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
;comment=Disable instance and database name reporting (it is tested separately).
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT
ok - connection uses TLS
ok - query succeeded
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?",
      "Datastore/statement/MySQL/tables/select",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "explain_plan": "??",
        "backtrace": "??"
      }
    ]
  ]
]
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../../include/config.php');

$pdo = Pdo\Mysql::connect("mysql:host=$MYSQL_TLS_HOST;port=$MYSQL_TLS_PORT;dbname=$MYSQL_TLS_DB",
                    'tls_clientcert', 'tls_clientcert',
                    array(PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
                          Pdo\Mysql::ATTR_SSL_CA => "$MYSQL_TLS_CERTS/ca.pem",
                     Pdo\Mysql::ATTR_SSL_KEY => "$MYSQL_TLS_CERTS/client-key.pem",
                     Pdo\Mysql::ATTR_SSL_CERT => "$MYSQL_TLS_CERTS/client-cert.pem"));
/* The user requires a client certificate, so a connection that was made is a TLS connection with a certificate. */
tap_assert($pdo instanceof PDO, 'connection uses TLS');

$stmt = $pdo->prepare("SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='STATISTICS'");
$stmt->execute();
$stmt->fetchAll();
tap_assert(true, 'query succeeded');
