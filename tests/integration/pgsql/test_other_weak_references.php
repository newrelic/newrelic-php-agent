<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent keeps each pgsql connection's datastore instance in a table tied to
the connection object through the engine's weak references. A connection that
also has other weak references (a WeakReference, a WeakMap entry) is tracked by
the engine in a per-object table, and is unregistered through a different path.

A. The agent removing its entry (a reconnect that returns the same object,
   pg_close()) SHALL leave the application's WeakReference and WeakMap entry
   intact, and the queries SHALL be reported against the right host.
B. A connection freed while it has the agent's entry and the application's weak
   references SHALL clear all of them, and a later connection SHALL be reported
   against its own host.

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
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.datastore_tracer.instance_reporting.enabled = 1
newrelic.datastore_tracer.database_name_reporting.enabled = 0
*/

/*EXPECT
ok - A: query 1
ok - A: a reconnect returns the same connection object
ok - A: WeakReference intact after the agent replaced its entry
ok - A: WeakMap entry intact after the agent replaced its entry
ok - A: query 2
ok - A: pg_close()
ok - A: WeakReference intact after the agent removed its entry
ok - A: WeakMap entry intact after the agent removed its entry
ok - A: WeakReference cleared when the connection is freed
ok - A: WeakMap entry cleared when the connection is freed
ok - B: query 3
ok - B: WeakReference cleared when the connection is freed
ok - B: WeakMap entry cleared when the connection is freed
ok - B: query 4
ok - B: query 5
*/

/*EXPECT_METRICS_EXIST
Datastore/instance/Postgres/ENV[TEST_PG_NAME]/ENV[TEST_PG_PORT], 3
Datastore/instance/Postgres/ENV[TEST_PG_IP]/ENV[TEST_PG_PORT], 2
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');

function test_other_weak_references() {
  global $PG_HOST, $PG_PORT, $PG_USER, $PG_PW;

  $name = $PG_HOST;
  $ip = gethostbyname($PG_HOST);
  env_var_for_expects("TEST_PG_NAME", $name);
  env_var_for_expects("TEST_PG_IP", $ip);
  env_var_for_expects("TEST_PG_PORT", $PG_PORT);

  $params = "port=$PG_PORT user=$PG_USER password=$PG_PW connect_timeout=1";

  /* A: the agent removes its entry while the application's weak refs remain. */
  $c = pg_connect("host=$name $params");
  $c_id = spl_object_id($c);
  $wr = WeakReference::create($c);
  $wm = new WeakMap();
  $wm[$c] = true;
  tap_assert(false !== pg_query($c, "SELECT 1"), "A: query 1");

  /* Same connection string, no PGSQL_CONNECT_FORCE_NEW: PHP returns $c. */
  $again = pg_connect("host=$name $params");
  tap_equal($c_id, spl_object_id($again),
            "A: a reconnect returns the same connection object");
  unset($again);
  tap_assert($wr->get() === $c,
             "A: WeakReference intact after the agent replaced its entry");
  tap_assert(count($wm) === 1 && isset($wm[$c]),
             "A: WeakMap entry intact after the agent replaced its entry");
  tap_assert(false !== pg_query($c, "SELECT 1"), "A: query 2");

  tap_assert(pg_close($c), "A: pg_close()");
  tap_assert($wr->get() === $c,
             "A: WeakReference intact after the agent removed its entry");
  tap_assert(count($wm) === 1,
             "A: WeakMap entry intact after the agent removed its entry");

  unset($c);
  tap_assert(null === $wr->get(),
             "A: WeakReference cleared when the connection is freed");
  tap_assert(0 === count($wm),
             "A: WeakMap entry cleared when the connection is freed");

  /* B: freed while the agent's entry and the application's weak refs exist. */
  $d = pg_connect("host=$ip $params", PGSQL_CONNECT_FORCE_NEW);
  $wr2 = WeakReference::create($d);
  $wm2 = new WeakMap();
  $wm2[$d] = true;
  tap_assert(false !== pg_query($d, "SELECT 1"), "B: query 3");

  /* Move the default link away from $d, so unset() frees it. */
  $e = pg_connect("host=$name $params", PGSQL_CONNECT_FORCE_NEW);
  unset($d);
  tap_assert(null === $wr2->get(),
             "B: WeakReference cleared when the connection is freed");
  tap_assert(0 === count($wm2),
             "B: WeakMap entry cleared when the connection is freed");

  /* Probably takes $d's freed handle; reported against its own host either way. */
  $f = pg_connect("host=$ip $params", PGSQL_CONNECT_FORCE_NEW);
  tap_assert(false !== pg_query($f, "SELECT 1"), "B: query 4");
  tap_assert(false !== pg_query($e, "SELECT 1"), "B: query 5");
}

test_other_weak_references();
