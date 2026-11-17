<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
An exception thrown by mysqli_options() on a link that was never initialized
SHALL NOT affect the transaction or the explain plans of other links.

A mysqli subclass whose constructor does not call the parent constructor is
not initialized, so PHP 8.0 and above throw an Error when it is used. The test
catches the Error from options() on such an object. It then connects another
link and runs a slow query, which gets an explain plan.

The TLS test server is used for the second link.
The test is skipped only if MYSQL_TLS_HOST is not set or PHP is below 8.0;
an unusable server or user fails it.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0.0 not supported\n");
}
$tls_profile = 'optional';
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
ok - options threw
ok - connection
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

class Uninitialized extends mysqli
{
  public function __construct()
  {
  }
}

$threw = false;
try {
  $uninitialized = new Uninitialized();
  $uninitialized->options(MYSQLI_INIT_COMMAND, 'SET @x = 1');
} catch (Error $e) {
  $threw = true;
}
tap_assert($threw, 'options threw');

$link = mysqli_connect($MYSQL_TLS_HOST, 'tls_optional', 'tls_optional', $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT);
tap_assert($link instanceof mysqli, 'connection');

$result = mysqli_query($link, 'SELECT 1 AS one');
tap_assert(false !== $result, 'query succeeded');
mysqli_free_result($result);
mysqli_close($link);
