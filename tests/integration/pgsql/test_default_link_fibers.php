<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
pgsql default link and Fibers.

How PHP behaves (PHP 8.1+): every successful pg_connect() / pg_pconnect() makes
the new connection the pgsql default link. The default link is stored in the
extension's module globals, which a Fiber switch does not save or swap (a Fiber
switch only swaps the VM stack and a few executor fields). So the pgsql default
link is currently shared by all Fibers on the thread: it is the most recently
opened connection, whichever Fiber opened it. This is a property of the pgsql
extension, not a guarantee of Fibers, and pgsql is the only database extension
the agent instruments that still has a link-less form on PHP 8.x. A pg_query() without a connection argument uses the
default link; that form is deprecated since PHP 8.1 and raises E_DEPRECATED, so
this test turns E_DEPRECATED off to keep the output stable. A query that names
its connection always uses that connection.

What this test checks:
1. Fiber one connects (application_name=fiber_one) and suspends. Fiber two
   connects (application_name=fiber_two) and suspends. Fiber one resumes and runs
   two queries: one with its own connection and one without a connection argument.
2. PHP behavior, checked through the server's application_name: the explicit query
   SHALL run on fiber one's connection, and the link-less query SHALL run on fiber
   two's connection. If the second assertion ever fails, PHP no longer shares the
   default link across Fibers, and the agent's handling of the default link
   (a single shared "last connection" for the thread) needs to be revisited.
3. Agent behavior, checked through the instance metrics: the two connections are
   two names for the same server (PG_HOST and its IP address), so the agent SHALL
   report the explicit query against the host name and the link-less query
   against the IP address, i.e. against the connection PHP used and not the
   connection the calling Fiber opened. If only the metrics fail, the agent
   attributes the link-less query to the wrong connection.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.1", "<")) {
  die("skip: PHP 8.1+ required (Fibers, PgSql\\Connection objects)\n");
}
require("skipif.inc");
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
newrelic.fibers.disabled = false
newrelic.datastore_tracer.instance_reporting.enabled = 1
newrelic.datastore_tracer.database_name_reporting.enabled = 0
error_reporting = E_ALL & ~E_DEPRECATED
*/

/*EXPECT
ok - explicit link in fiber one ran on fiber one's connection
ok - link-less query in fiber one ran on fiber two's connection (shared default link)
*/

/*EXPECT_METRICS_EXIST
Datastore/instance/Postgres/ENV[TEST_PG_NAME]/ENV[TEST_PG_PORT], 1
Datastore/instance/Postgres/ENV[TEST_PG_IP]/ENV[TEST_PG_PORT], 1
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');

/*
 * Returns the application_name of the connection that ran the query. With a
 * connection, the query names it; without one, PHP uses its default link (the
 * form is deprecated, hence the error_reporting setting above).
 */
function app_name($conn = null) {
  $sql = "SELECT current_setting('application_name')";
  $res = (null === $conn) ? pg_query($sql) : pg_query($conn, $sql);
  return pg_fetch_result($res, 0, 0);
}

function test_default_link_fibers() {
  global $PG_HOST, $PG_PORT, $PG_USER, $PG_PW;
  $name = $PG_HOST;
  $ip = gethostbyname($PG_HOST);
  env_var_for_expects("TEST_PG_NAME", $name);
  env_var_for_expects("TEST_PG_IP", $ip);
  env_var_for_expects("TEST_PG_PORT", $PG_PORT);

  $params = "port=$PG_PORT user=$PG_USER password=$PG_PW connect_timeout=1";

  $fiber_one = new Fiber(function () use ($name, $params) {
    $conn = pg_connect("host=$name $params application_name=fiber_one",
                       PGSQL_CONNECT_FORCE_NEW);
    Fiber::suspend();
    return array(app_name($conn), app_name());
  });

  $fiber_two = new Fiber(function () use ($ip, $params) {
    /* Keep the connection alive while this Fiber is suspended. */
    $conn = pg_connect("host=$ip $params application_name=fiber_two",
                       PGSQL_CONNECT_FORCE_NEW);
    Fiber::suspend();
  });

  $fiber_one->start();   /* connects by host name, suspends */
  $fiber_two->start();   /* connects by IP, suspends */
  $fiber_one->resume();  /* explicit query, then link-less query */

  list($explicit, $linkless) = $fiber_one->getReturn();

  tap_equal('fiber_one', $explicit,
            "explicit link in fiber one ran on fiber one's connection");
  tap_equal('fiber_two', $linkless,
            "link-less query in fiber one ran on fiber two's connection "
            . "(shared default link)");
}

test_default_link_fibers();
