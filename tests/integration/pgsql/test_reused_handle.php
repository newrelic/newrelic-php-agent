<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent caches each pgsql connection's datastore instance keyed by the
connection object's handle. When a connection is released without pg_close()
and a later connection to a different host reuses its object handle, the agent
SHALL report the later connection's queries against the later connection's
host, not the earlier connection's host.

The two hosts are two names for the same server: PG_HOST and its IP address.
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
newrelic.datastore_tracer.instance_reporting.enabled = 1
newrelic.datastore_tracer.database_name_reporting.enabled = 0
*/

/*EXPECT
ok - query on first connection (host name)
ok - query on second connection (IP)
ok - third connection reuses the first connection's handle
ok - query on third connection (IP)
*/

/*EXPECT_METRICS_EXIST
Datastore/instance/Postgres/ENV[TEST_PG_NAME]/ENV[TEST_PG_PORT], 1
Datastore/instance/Postgres/ENV[TEST_PG_IP]/ENV[TEST_PG_PORT], 2
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');

function test_reused_handle() {
  global $PG_HOST, $PG_PORT, $PG_USER, $PG_PW;

  $name = $PG_HOST;
  $ip = gethostbyname($PG_HOST);
  env_var_for_expects("TEST_PG_NAME", $name);
  env_var_for_expects("TEST_PG_IP", $ip);
  env_var_for_expects("TEST_PG_PORT", $PG_PORT);

  $params = "port=$PG_PORT user=$PG_USER password=$PG_PW connect_timeout=1";

  /* First connection: saved as the host name. */
  $conn = pg_connect("host=$name $params", PGSQL_CONNECT_FORCE_NEW);
  $first_handle = spl_object_id($conn);
  tap_assert(false !== pg_query($conn, "SELECT 1"),
             "query on first connection (host name)");

  /*
   * Second connection: gets a new handle and is saved as the IP. pgsql's
   * default link holds a reference to the most recent connection, so the
   * first connection is freed only now, when neither $conn nor the default
   * link refers to it. Its entry stays in the agent's map.
   */
  $conn = pg_connect("host=$ip $params", PGSQL_CONNECT_FORCE_NEW);
  tap_assert(false !== pg_query($conn, "SELECT 1"),
             "query on second connection (IP)");

  /*
   * Third connection: takes the first connection's freed handle. The agent
   * finds the stale entry for that handle and skips saving the IP.
   */
  $conn = pg_connect("host=$ip $params", PGSQL_CONNECT_FORCE_NEW);
  tap_equal($first_handle, spl_object_id($conn),
            "third connection reuses the first connection's handle");
  tap_assert(false !== pg_query($conn, "SELECT 1"),
             "query on third connection (IP)");
}

test_reused_handle();
