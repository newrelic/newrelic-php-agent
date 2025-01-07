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
*/

require_once(dirname(__FILE__).'/../../include/tap.php');

function test_prepared_stmt($conn, $query) {
  $stmt = $conn->prepare($query);
  $stmt->bindValue(1, "missing");
  tap_assert($stmt->execute(), 'execute prepared statement');
}
