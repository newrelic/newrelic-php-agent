<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
pg_close() with no argument releases PHP's default link and leaves the
connection open. Queries on a variable that still holds the connection SHALL
keep being reported against the connection's host.
*/

/*SKIPIF
<?php
require("skipif.inc");

if (version_compare(PHP_VERSION, "8.1", "<")) {
  die("skip: PHP 8.1+ required (PgSql\\Connection objects)\n");
}

$ip = gethostbyname($PG_HOST);
if ($ip === $PG_HOST) {
  die("skip: PG_HOST must be a host name that resolves to an IP address\n");
}

// The agent reports all loopback names as the system hostname.
foreach (array($PG_HOST, $ip) as $h) {
  if (in_array($h, array("localhost", "127.0.0.1", "0.0.0.0", "::1"))) {
    die("skip: PG_HOST must not be a loopback address\n");
  }
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.datastore_tracer.instance_reporting.enabled = 1
newrelic.datastore_tracer.database_name_reporting.enabled = 0
*/

/*EXPECT
ok - query before pg_close()
ok - pg_close() with no argument
ok - query after pg_close()
*/

/*EXPECT_METRICS_EXIST
Datastore/instance/Postgres/ENV[TEST_PG_IP]/ENV[TEST_PG_PORT], 2
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');

function test_close_default_link_keeps_connection() {
  global $PG_HOST, $PG_PORT, $PG_USER, $PG_PW;

  $ip = gethostbyname($PG_HOST);
  env_var_for_expects("TEST_PG_IP", $ip);
  env_var_for_expects("TEST_PG_PORT", $PG_PORT);

  $params = "port=$PG_PORT user=$PG_USER password=$PG_PW connect_timeout=1";

  $conn = pg_connect("host=$ip $params", PGSQL_CONNECT_FORCE_NEW);
  tap_assert(false !== pg_query($conn, "SELECT 1"), "query before pg_close()");

  /* Releases the default link only; $conn stays open. */
  tap_assert(pg_close(), "pg_close() with no argument");

  tap_assert(false !== pg_query($conn, "SELECT 1"), "query after pg_close()");
}

test_close_default_link_keeps_connection();
