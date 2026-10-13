<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A mysqli link that takes the handle of a freed link SHALL NOT inherit its
credentials for the connection the agent opens to run EXPLAIN.

PHP reuses the handle of a freed object. The first link connects as a user with
a password and is freed. A second link is created until one takes the first's
handle, and connects as a user that has no password: it leaves the password out,
so nothing in what the agent saves for it replaces the first link's password. If
the agent kept the saved password for that handle, the explain connection would
log in as the second user with the first user's password and be refused, and
the slow query would have no explain plan.

The users are in tests/tls. The test is skipped without the TLS test server and
before PHP 8.1, where a null password falls back to mysqli.default_pw.
*/

/*SKIPIF
<?php
require("skipif.inc");
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');
if (PHP_VERSION_ID < 80100) {
  die("skip: a null password uses mysqli.default_pw from PHP 8.1\n");
}
if ('' == $MYSQL_TLS_HOST) {
  die("skip: MYSQL_TLS_HOST is not set\n");
}
mysqli_report(MYSQLI_REPORT_OFF);
$link = mysqli_init();
if (!@mysqli_real_connect($link, $MYSQL_TLS_HOST, 'nr_nopass', null,
                          $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT)) {
  die("skip: user nr_nopass not available: " . mysqli_connect_error() . "\n");
}
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
  $second = mysqli_init();
  if (spl_object_id($second) === $first_handle) {
    break;
  }
  /* Hold the ones that missed so the next one lands on another handle. */
  $held[] = $second;
}
tap_equal($first_handle, spl_object_id($second),
          "second link reuses the first link's handle");

/* The second link connects as a user without a password, leaving it out. */
mysqli_real_connect($second, $MYSQL_TLS_HOST, 'nr_nopass', null, null,
                    (int)$MYSQL_TLS_PORT);

$result = mysqli_query($second, 'SELECT 1 AS one');
tap_assert(false !== $result, 'query succeeded');
mysqli_free_result($result);
mysqli_close($second);
