<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
  This file contains code that runs the PDO::query test using given connection object. It tests the FETCH_CLASS variant of PDO::query().
  It is common for all database driver specific tests that prepare connection objects
  using different methods:
  - ./mysql:
    - ./base-class
    - ./constructor
    - ./factory
  - ./pgsql:
    - ./base-class
    - ./constructor
    - ./factory
  - ./sqlite:
    - ./base-class
    - ./constructor
    - ./factory
*/

require_once(dirname(__FILE__).'/../../include/tap.php');

class Row {
  public $id;
  public $description;
}

function test_pdo_query($conn, $want_drop_table_result=1) {
  tap_equal(0, $conn->exec("CREATE TABLE test (id INT, description VARCHAR(10));"), 'create table');
  tap_equal(1, $conn->exec("INSERT INTO test VALUES (1, 'one');"), 'insert row');

  $expected = new Row();
  $expected->id = '1';
  $expected->description = 'one';

  $actual = $conn->query('SELECT * FROM test;', PDO::FETCH_CLASS, 'Row')->fetch();
  tap_assert($expected == $actual, 'fetch row as object');

  tap_equal($want_drop_table_result, $conn->exec("DROP TABLE test;"), 'drop table');
}
