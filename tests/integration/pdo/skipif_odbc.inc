<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The skipif of the PDO tests that use the odbc driver with the PostgreSQL ODBC
 * driver. A test is skipped only when it cannot run at all: the pdo_odbc
 * extension is not loaded or the PostgreSQL ODBC driver is not installed. When
 * the driver is installed but the database is not usable, this file exits with
 * an error and the runner fails the test with the reason.
 */
require_once(dirname(__FILE__) . '/../../include/config.php');

if (!extension_loaded('pdo_odbc')) {
  die("skip: the pdo_odbc extension is not loaded\n");
}
if (!in_array('odbc', PDO::getAvailableDrivers())) {
  die("skip: the odbc driver is not available\n");
}

$driver = 'PostgreSQL Unicode';
if (!file_exists('/etc/odbcinst.ini')
    || false === strpos(file_get_contents('/etc/odbcinst.ini'), "[$driver]")) {
  die("skip: the PostgreSQL ODBC driver is not installed\n");
}

try {
  new PDO("odbc:Driver={{$driver}};Server=$PG_HOST;Port=$PG_PORT;Database=postgres",
          $PG_USER, $PG_PW);
} catch (PDOException $e) {
  echo "PostgreSQL not available through ODBC: " . $e->getMessage() . "\n";
  exit(1);
}
