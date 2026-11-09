<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
An oci statement SHALL keep its SQL after the transaction in which it was
parsed has ended. The statement is parsed in the first transaction and
executed in the second, as a long-running worker that prepares statements at
startup does.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
newrelic.transaction_tracer.record_sql = obfuscated
newrelic.transaction_tracer.explain_threshold = 0
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT
ok - connected
ok - statement parsed
ok - statement executed
*/

/*EXPECT_METRICS_EXIST
Datastore/statement/Oracle/dual/select, 1
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/Custom/after_restart",
      "<unknown>",
      "?? SQL id",
      "SELECT ? AS one FROM dual",
      "Datastore/statement/Oracle/dual/select",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      "?? params"
    ]
  ]
]
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
      "name": "Datastore\/statement\/Oracle\/dual\/select",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??",
      "span.kind": "client",
      "component": "Oracle"
    },
    {},
    {
      "peer.address": "unknown:unknown",
      "db.statement": "SELECT ? AS one FROM dual"
    }
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

$conn = oci_connect($ORACLE_USER, $ORACLE_PASSWD, $ORACLE_CONNECT);
tap_assert(false !== $conn, 'connected');

$stmt = oci_parse($conn, 'SELECT 1 AS one FROM dual');
tap_assert(false !== $stmt, 'statement parsed');

newrelic_end_transaction();
newrelic_start_transaction(ini_get("newrelic.appname"));

newrelic_name_transaction("after_restart");
tap_assert(oci_execute($stmt), 'statement executed');
oci_free_statement($stmt);
oci_close($conn);
