<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore metrics for sqlite_unbuffered_query().
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*INI
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
*/

/*EXPECT
Array
(
    [0] => Array
        (
            [id] => 1
            [desc] => one
        )

    [1] => Array
        (
            [id] => 2
            [desc] => two
        )

    [2] => Array
        (
            [id] => 3
            [desc] => three
        )

)
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name": "Supportability/Logging/Forwarding/PHP/disabled"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                          [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                     [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/all"},                   [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/allOther"},              [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/create"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/drop"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/insert"},      [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/select"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/create"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/drop"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/insert"}, [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/select"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/create",
      "scope":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/drop",
      "scope":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/insert",
      "scope":"OtherTransaction/php__FILE__"},          [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/select",
      "scope":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

function test_sqlite() {
  $conn = sqlite_open(":memory:");
  sqlite_exec($conn, "CREATE TABLE test (id INT, desc VARCHAR(10));");

  sqlite_exec($conn, "INSERT INTO test VALUES (1, 'one');");
  sqlite_exec($conn, "INSERT INTO test VALUES (2, 'two');");
  sqlite_exec($conn, "INSERT INTO test VALUES (3, 'three');");

  $result = sqlite_unbuffered_query($conn, "SELECT * FROM test;");
  if (false === $result) {
    die("sqlite_query should have return a result");
  }

  print_r(sqlite_fetch_all($result, SQLITE_ASSOC));

  sqlite_exec($conn, "DROP TABLE test;");
  sqlite_close($conn);
}

test_sqlite();
