<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should generate Database metrics for SQLite3::querysingle().
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*EXPECT
Array
(
    [id] => 1
    [desc] => one
)
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/SQLite/all"},                   [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/allOther"},              [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                          [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                     [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/insert"},      [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/other"},       [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/other",
      "scope":"OtherTransaction/php__FILE__"},          [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/select"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/insert"}, [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/insert",
      "scope":"OtherTransaction/php__FILE__"},          [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/select"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/select",
      "scope":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

function test_sqlite3() {
  $conn = new SQLite3(":memory:");
  $conn->exec("CREATE TABLE test (id INT, desc VARCHAR(10));");

  $conn->exec("INSERT INTO test VALUES (1, 'one');");
  $conn->exec("INSERT INTO test VALUES (2, 'two');");
  $conn->exec("INSERT INTO test VALUES (3, 'three');");

  print_r($conn->querysingle("SELECT * FROM test WHERE id = 1;", true));

  $conn->exec("DROP TABLE test;");
}

test_sqlite3();
