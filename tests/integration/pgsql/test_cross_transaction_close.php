<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
pg_close() frees pgsql_last_conn when the closed connection's key matches it
(php_pgsql.c). So, unlike test_cross_transaction.php, once the only connection
is closed there is nothing for a later connection-less pg_query($sql) to fall
back on: the agent SHALL synthesize the default instance
(nr_php_pgsql_retrieve_datastore_instance(NULL) with no pgsql_last_conn) rather
than carry stale attribution to the real postgres/5432 instance. At the PHP
level the query itself fails (no connection is open), but the agent still
records a datastore node for the attempt.
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.instance_reporting.enabled = 1
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
ok - connect successful
ok - close successful
ok - pg_query failed without an open connection
*/

/*EXPECT_METRICS_EXIST
Datastore/statement/Postgres/pg_user/select, 1
Datastore/operation/Postgres/select, 1
Supportability/api/start_transaction, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/instance/Postgres/ENV[PG_HOST]/ENV[PG_PORT]
Supportability/api/end_transaction
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore\/statement\/Postgres\/pg_user\/select",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??",
      "span.kind": "client",
      "component": "Postgres"
    },
    {},
    {
      "peer.hostname": "??",
      "peer.address": "??",
      "db.statement": "SELECT ? FROM pg_user"
    }
  ]
]
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');

function test_cross_transaction_close() {
  global $PG_CONNECTION;

  /*
   * T1: open the connection, then close it. Closing frees pgsql_last_conn
   * because its key matches the connection being closed (php_pgsql.c), so no
   * connection remains for a later connection-less lookup to fall back on.
   */
  $conn = pg_connect($PG_CONNECTION);
  tap_assert(false !== $conn, "connect successful");

  tap_assert(pg_close($conn), "close successful");

  /*
   * Restart the transaction the same way the FrankenPHP worker request
   * boundary does: fcall_end/fcall_begin call nr_php_txn_end/nr_php_txn_begin,
   * exactly like this explicit end+start pair. Ignore T1 (nothing but the
   * connect/close happened in it) so only T2's query is harvested.
   */
  newrelic_end_transaction(true);
  newrelic_start_transaction(ini_get("newrelic.appname"));

  /*
   * T2: with no connection open and pgsql_last_conn cleared, the
   * connection-less pg_query($sql) fails at the PHP level (warns + returns
   * false) -- suppress the warning. The agent still records a datastore node
   * for the attempt, synthesizing a default instance rather than carrying
   * stale postgres/5432 attribution. The default instance's exact host/port
   * depend on the container's libpq env (PGHOST/PGPORT, which the php
   * container does not set), so the lock here is the *absence* of the real
   * instance metric, not a specific default string.
   */
  try {
    $result = @pg_query("SELECT 1 FROM pg_user");
    // PHP 7.x return false on failure, so assert that the query failed.
    tap_assert(false === $result, "pg_query failed without an open connection");
  }
  catch (\Throwable $e) {
    // PHP 8.0+ throws a fatal error instead returning false, so
    // catch it and assert that the query failed.
    tap_ok("pg_query failed without an open connection");
  }
}

test_cross_transaction_close();
