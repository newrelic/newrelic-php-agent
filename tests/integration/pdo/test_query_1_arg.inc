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
  It tests one argument form of PDO::query().
*/

require_once(dirname(__FILE__).'/../../include/tap.php');

function test_pdo_query($conn, $want_drop_table_result=1) {
  tap_equal(0, $conn->exec("CREATE TABLE test (id INT, description VARCHAR(10));"), 'create table');

  tap_equal(1, $conn->exec("INSERT INTO test VALUES (1, 'one');"), 'insert one');
  tap_equal(1, $conn->exec("INSERT INTO test VALUES (2, 'two');"), 'insert two');
  tap_equal(1, $conn->exec("INSERT INTO test VALUES (3, 'three');"), 'insert three');

  $expected = array(
    array('id' => 1, 'description' => 'one'),
    array('id' => 2, 'description' => 'two'),
    array('id' => 3, 'description' => 'three')
  );

  $result = $conn->query('SELECT * FROM test;');
  $actual = $result->fetchAll(PDO::FETCH_ASSOC);
  $result->closeCursor();
  // normalize id to int type
  array_walk($actual, function (&$row, $key) {
    $row['id'] = intval($row['id']);
  });
  tap_equal($expected, $actual, 'query (1-arg)');

  tap_equal($want_drop_table_result, $conn->exec("DROP TABLE test;"), 'drop table');
}
