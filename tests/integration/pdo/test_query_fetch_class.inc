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

    NOTE: The function here can be used as a fiber.  
    The caller will need 2 fiber resumes to fully exercise the function.
*/

require_once(dirname(__FILE__).'/../../include/tap.php');
require_once(dirname(__FILE__).'/../../include/helpers.php');

class Row {
  public $id;
  public $description;
}

function test_pdo_query($conn, $want_drop_table_result=1, $is_fiber=false) {
  if ($is_fiber) {
    env_var_for_expects("GUID_TEST_PDO_QUERY", newrelic_get_linking_metadata()['span.id'] ?? '');
  }
  tap_equal(0, $conn->exec("CREATE TABLE test (id INT, description VARCHAR(10));"), 'create table');
  if ($is_fiber) {
    Fiber::suspend();
  }
  tap_equal(1, $conn->exec("INSERT INTO test VALUES (1, 'one');"), 'insert row');

  $expected = new Row();
  $expected->id = '1';
  $expected->description = 'one';

  $actual = $conn->query('SELECT * FROM test;', PDO::FETCH_CLASS, 'Row')->fetch();
  tap_assert($expected == $actual, 'fetch row as object');

  if ($is_fiber) {
    Fiber::suspend();
  }

  tap_equal($want_drop_table_result, $conn->exec("DROP TABLE test;"), 'drop table');
}
