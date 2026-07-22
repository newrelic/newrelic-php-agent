<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
pgsql_last_conn and datastore_connections are per-request state, created in
rinit and torn down in rshutdown; they are NOT reset at the transaction
boundary. The agent SHALL retain pgsql_last_conn across a transaction restart,
so a connection-less pg_query($sql) issued in a later transaction is still
attributed to the connection opened in an earlier one. This is the same
survival as test_cross_transaction.php, but restarted via
newrelic_set_appname() (which itself ends + begins a transaction), mirroring
the predis suite's tests/integration/predis/test_cross_transaction.php.
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
ok - pg_query successful
*/

/*EXPECT_METRICS_EXIST
Datastore/instance/Postgres/ENV[PG_HOST]/ENV[PG_PORT], 1
Datastore/statement/Postgres/pg_user/select, 1
Datastore/operation/Postgres/select, 1
Supportability/api/set_appname/after, 1
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
      "peer.hostname": "ENV[PG_HOST]",
      "peer.address": "ENV[PG_HOST]:ENV[PG_PORT]",
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

function test_cross_transaction_set_appname() {
  global $PG_CONNECTION;

  /*
   * T1: open the connection. pg_connect() saves the instance keyed by the
   * connection and points pgsql_last_conn at it (php_pgsql.c).
   */
  $conn = pg_connect($PG_CONNECTION);
  tap_assert(false !== $conn, "connect successful");

  /*
   * Restart the transaction via newrelic_set_appname(), the same public API
   * the predis suite uses to pin this behavior. Internally it does the same
   * end + begin as test_cross_transaction.php (php_api.c), and with no
   * $xmit argument it discards (ignores) T1 rather than harvesting it.
   * pgsql_last_conn is per-request state torn down only in rshutdown, so it
   * survives this transactionrestart regardless.
   */
  newrelic_set_appname(ini_get("newrelic.appname"));

  /*
   * T2: a connection-less pg_query($sql) has no handle to key off of, so it
   * falls back to pgsql_last_conn. If that survived, the datastore node is
   * attributed to the real instance (postgres/5432); if it didn't, the agent
   * would synthesize the default (localhost//tmp) instance instead.
   *
   * The single-arg form's *userland* output (resource vs PgSql\Connection,
   * a possible deprecation notice) differs across the PHP 8.1 boundary, so
   * suppress it and let the instance metric name be the assertion.
   */
  $result = @pg_query("SELECT 1 FROM pg_user");
  $row = pg_fetch_row($result);
  tap_assert($row[0] == 1, "pg_query successful");
}

test_cross_transaction_set_appname();
