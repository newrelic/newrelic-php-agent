<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore metrics for sqlite_exec(), and it should
gracefully handle the first two parameters occuring in either order.
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*INI
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name": "Supportability/Logging/Forwarding/PHP/disabled"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                          [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                     [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/all"},                   [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/allOther"},              [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/create"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/drop"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/insert"},      [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/create"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/drop"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/insert"}, [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/create",
      "scope":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/drop",
      "scope":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/insert",
      "scope":"OtherTransaction/php__FILE__"},          [3, "??", "??", "??", "??", "??"]]
  ]
]
*/

function test_sqlite() {
  $conn = sqlite_open(":memory:");
  sqlite_exec($conn, "CREATE TABLE test (id INT, desc VARCHAR(10));");

  /* Now try putting the resource second. */
  sqlite_exec("INSERT INTO test VALUES (1, 'one');", $conn);
  sqlite_exec("INSERT INTO test VALUES (2, 'two');", $conn);
  sqlite_exec("INSERT INTO test VALUES (3, 'three');", $conn);

  sqlite_exec($conn, "DROP TABLE test;");
  sqlite_close($conn);
}

test_sqlite();
