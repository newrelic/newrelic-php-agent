<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
  This file contains code that runs the PDOStatement::execute() test on PDOStatement
  object prepared using a given connection object. It tests PDOStatement with values bound.
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

function test_prepared_stmt($conn, $query, $is_fiber=false) {
  if ($is_fiber) {
    env_var_for_expects("GUID_TEST_PREPARED_STMT", newrelic_get_linking_metadata()['span.id'] ?? '');
  }
  
  $stmt = $conn->prepare($query);
  if ($is_fiber) {
    Fiber::suspend();
  }
  $stmt->bindValue(1, "missing");
  if ($is_fiber) {
    Fiber::suspend();
  }
  tap_assert($stmt->execute(), 'execute prepared statement');
}
