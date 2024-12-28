<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

require_once(dirname(__FILE__).'/../../include/tap.php');

function test_instance_reporting($conn, $want_drop_table_result=1) {
  tap_equal(0, $conn->exec("CREATE TABLE test (id INT, description VARCHAR(10));"), 'create table');
  tap_equal($want_drop_table_result, $conn->exec("DROP TABLE test;"), 'drop table');
}
