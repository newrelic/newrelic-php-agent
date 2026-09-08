<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
  This file contains code that executes SQL statements using a given connection object.
  It is used to test database instance reporting functionality which results in database
  instance metrics being created as well as database instance attributes being added to
  slow sql traces as well as span events.
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

function test_instance_reporting($conn, $want_drop_table_result=1, $is_fiber=false) {
  if ($is_fiber) {
    env_var_for_expects("GUID_TEST_INSTANCE_REPORTING", newrelic_get_linking_metadata()['span.id'] ?? '');
  }

  if ($is_fiber) {
    Fiber::suspend();
  }
  tap_equal(0, $conn->exec("CREATE TABLE test (id INT, description VARCHAR(10));"), 'create table');
  if ($is_fiber) {
    Fiber::suspend();
  }
  tap_equal($want_drop_table_result, $conn->exec("DROP TABLE test;"), 'drop table');
}
