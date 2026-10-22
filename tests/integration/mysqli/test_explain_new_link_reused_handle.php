<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A mysqli link made by the constructor that takes the handle of a freed link
SHALL NOT inherit its credentials for the connection the agent opens to run
EXPLAIN.

The first link connects as a user with a password and is freed. Links are
created with new mysqli() until one takes the first's handle. It connects as a
user that has no password and leaves the password out, so nothing in what the
agent saves for it replaces the first link's password. If the agent kept the
saved password for that handle, the explain connection would log in as the
second user with the first user's password and be refused, and the slow query
would have no explain plan.

The users are in tests/tls.
The test is skipped only if MYSQL_TLS_HOST is not set or PHP is below 7.4;
an unusable server or user fails it.
*/

/*SKIPIF
<?php
require("skipif.inc");
$tls_profile = 'nopass';
require("skipif_tls.inc");
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
;comment=Disable instance and database name reporting (it is tested separately).
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT
ok - second link reuses the first link's handle
ok - second link connected
ok - query succeeded
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT ? AS one",
      "Datastore/operation/MySQL/select",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "explain_plan": "??",
        "backtrace": "??"
      }
    ]
  ]
]
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

mysqli_report(MYSQLI_REPORT_OFF);

/* The first link: a user with a password. */
$first = new mysqli($MYSQL_TLS_HOST, 'tls_optional', 'tls_optional',
                    $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT);
$first_handle = spl_object_id($first);
unset($first);

/* The next links, until one takes the freed handle. */
$held = array();
$second = null;
for ($i = 0; $i < 50; $i++) {
  $second = new mysqli($MYSQL_TLS_HOST, 'nr_nopass', null, $MYSQL_TLS_DB,
                       (int)$MYSQL_TLS_PORT);
  if (spl_object_id($second) === $first_handle) {
    break;
  }
  /* Hold the ones that missed so the next one lands on another handle. */
  $held[] = $second;
}
tap_equal($first_handle, spl_object_id($second),
          "second link reuses the first link's handle");
tap_assert(0 === mysqli_connect_errno(), 'second link connected');

$result = mysqli_query($second, 'SELECT 1 AS one');
tap_assert(false !== $result, 'query succeeded');
mysqli_free_result($result);
mysqli_close($second);
