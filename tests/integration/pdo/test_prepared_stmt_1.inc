<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

require_once(dirname(__FILE__).'/../../include/tap.php');

function test_prepared_stmt($conn, $query) {
  $stmt = $conn->prepare($query);
  tap_assert($stmt->execute(), 'execute prepared statement');
}
