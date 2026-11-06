<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The skipif of the tests that need an Oracle Database (see
 * tests/include/config.php).
 *
 * A test is skipped only when it cannot run at all: the oci8 extension is not
 * loaded (it needs Oracle Instant Client, which requires glibc, so the musl
 * builder images never have it) or ORACLE_HOST is not set. When the host is
 * set but the database is not usable, this file exits with an error and the
 * runner fails the test with the reason, so that a broken test environment is
 * not mistaken for a pass.
 */
require_once(dirname(__FILE__) . '/../../include/config.php');

if (!extension_loaded('oci8')) {
  die("skip: the oci8 extension is not loaded\n");
}
if ('' == $ORACLE_HOST) {
  die("skip: ORACLE_HOST is not set\n");
}

$conn = @oci_connect($ORACLE_USER, $ORACLE_PASSWD, $ORACLE_CONNECT);
if (false === $conn) {
  $e = oci_error();
  echo "Oracle database not available at $ORACLE_CONNECT: "
       . (is_array($e) ? $e['message'] : 'unknown error') . "\n";
  exit(1);
}
oci_close($conn);
