<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

if (!extension_loaded('pdo')) {
  die("skip: pdo extension required\n");
}

if (!extension_loaded('pdo_pgsql')) {
  die("skip: pdo_pgsql extension required\n");
}

if (getenv('SKIP_SLOW_TESTS')) {
  die("skip: slow test\n");
}

/*
 * Disable reporting of warnings. Despite what the PHP manual says,
 * PDO::__construct seems to unconditionally trigger a warning on
 * connect failure. Setting the error mode to PDO::ERRMODE_SILENT or
 * PDO::ERRMODE_EXCEPTION only affects the behavior of the PDO object
 * after the connection is established. This is dumb. We need to suppress
 * the warning to ensure the skip message is the first line of output,
 * and it can be observed by the test runner.
 */
$level = error_reporting();
error_reporting($level & ~E_WARNING);

try {
  $conn = new PDO($PDO_PGSQL_DSN, $PG_USER, $PG_PW);
} catch (Exception $e) {
  die("skip: " . $e->getMessage() . "\n");
}
