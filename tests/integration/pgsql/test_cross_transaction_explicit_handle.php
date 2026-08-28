<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The datastore_connections hashmap is per-request state, created in rinit and
torn down in rshutdown; it is NOT reset at the transaction boundary. The agent
SHALL retain the hashmap entry for a connection across a transaction restart,
so pg_query($conn, $sql) issued with an explicit handle in a later transaction
is still attributed to the instance recorded when that connection was opened.
Unlike test_cross_transaction.php, this does not depend on pgsql_last_conn at
all: the hashmap is keyed by the connection itself.
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
Supportability/api/start_transaction, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Supportability/api/end_transaction
Datastore/instance/Postgres/__HOST__//tmp
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

function test_cross_transaction_explicit_handle() {
  global $PG_CONNECTION;

  /*
   * T1: open the connection. pg_connect() saves the instance keyed by the
   * connection itself (php_pgsql.c), independent of pgsql_last_conn.
   */
  $conn = pg_connect($PG_CONNECTION);
  tap_assert(false !== $conn, "connect successful");

  /*
   * Restart the transaction the same way the FrankenPHP worker request
   * boundary does: fcall_end/fcall_begin call nr_php_txn_end/nr_php_txn_begin,
   * exactly like this explicit end+start pair. Ignore T1 (nothing but the
   * connect happened in it) so only T2's query is harvested.
   */
  newrelic_end_transaction(true);
  newrelic_start_transaction(ini_get("newrelic.appname"));

  /*
   * T2: pass the connection handle explicitly, so the datastore lookup keys
   * off $conn directly (nr_php_pgsql_retrieve_datastore_instance) rather than
   * pgsql_last_conn. The connection's PHP-level handle/object is unaffected
   * by the transaction restart, so the lookup must still hit and attribute
   * the node to the real instance (postgres/5432).
   */
  $result = pg_query($conn, "SELECT 1 FROM pg_user");
  $row = pg_fetch_row($result);
  tap_assert($row[0] == 1, "pg_query successful");
}

test_cross_transaction_explicit_handle();
