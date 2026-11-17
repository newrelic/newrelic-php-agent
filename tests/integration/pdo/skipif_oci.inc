<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The skipif of the PDO tests that need an Oracle Database (see
 * tests/include/config.php). A test is skipped only when it cannot run at all:
 * the pdo_oci extension is not loaded or ORACLE_HOST is not set. When the host
 * is set but the database is not usable, this file exits with an error and the
 * runner fails the test with the reason.
 */
require_once(dirname(__FILE__) . '/../../include/config.php');

if (!extension_loaded('pdo_oci')) {
  die("skip: the pdo_oci extension is not loaded\n");
}
if ('' == $ORACLE_HOST) {
  die("skip: ORACLE_HOST is not set\n");
}

try {
  new PDO('oci:dbname=//' . $ORACLE_CONNECT, $ORACLE_USER, $ORACLE_PASSWD);
} catch (PDOException $e) {
  echo "Oracle database not available at $ORACLE_CONNECT: "
       . $e->getMessage() . "\n";
  exit(1);
}
