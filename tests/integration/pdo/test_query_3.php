<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should record database metrics for the FETCH_CLASS variant of
PDO::query().
*/

/*SKIPIF
<?php require('skipif_sqlite.inc');
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT
ok - create table
ok - insert row
ok - fetch row as object
ok - drop table
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/all"},                          [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                     [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/all"},                   [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/allOther"},              [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/create"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/drop"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/insert"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/select"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/create"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/drop"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/insert"}, [1, "??", "??", "??", "??", "??"]],
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
      "scope":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/select",
      "scope":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(dirname(__FILE__).'/../../include/tap.php');

class Row {
  public $id;
  public $desc;
}

function test_pdo_query() {
  $conn = new PDO('sqlite::memory:');
  tap_equal(0, $conn->exec("CREATE TABLE test (id INT, desc VARCHAR(10));"), 'create table');
  tap_equal(1, $conn->exec("INSERT INTO test VALUES (1, 'one');"), 'insert row');

  $expected = new Row();
  $expected->id = '1';
  $expected->desc = 'one';

  $actual = $conn->query('SELECT * FROM test;', PDO::FETCH_CLASS, 'Row')->fetch();
  tap_assert($expected == $actual, 'fetch row as object');

  tap_equal(1, $conn->exec("DROP TABLE test;"), 'drop table');
}

test_pdo_query();
