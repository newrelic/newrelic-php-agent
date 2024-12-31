<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
  This file contains code that runs the PDO::query test using given connection object.
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
  It tests the FETCH_COLUMN variant of PDO::query().
*/

require_once(dirname(__FILE__).'/../../include/tap.php');

function test_pdo_query($conn, $want_drop_table_result=1) {
  tap_equal(0, $conn->exec("CREATE TABLE test (id INT, description VARCHAR(10));"), 'create table');

  tap_equal(1, $conn->exec("INSERT INTO test VALUES (1, 'one');"), 'insert one');
  tap_equal(1, $conn->exec("INSERT INTO test VALUES (2, 'two');"), 'insert two');
  tap_equal(1, $conn->exec("INSERT INTO test VALUES (3, 'three');"), 'insert three');

  $result = $conn->query('SELECT * FROM test;', PDO::FETCH_COLUMN, 1);
  $actual = $result->fetchAll(PDO::FETCH_ASSOC);
  $result->closeCursor();
  tap_equal(3, count($actual), 'fetch column');

  tap_equal($want_drop_table_result, $conn->exec("DROP TABLE test;"), 'drop table');
}
