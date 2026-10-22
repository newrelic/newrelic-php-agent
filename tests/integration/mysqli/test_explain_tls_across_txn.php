<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The TLS settings of a mysqli link SHALL be kept after the transaction in which
the link was opened has ended. The user requires TLS, so the connection the agent
opens to run EXPLAIN is refused unless it also asks for TLS: if the settings
were lost, the slow query in the second transaction would have no explain plan.

The TLS setup is in tests/tls. The test is skipped only if
MYSQL_TLS_HOST is not set or PHP is below 8.0, where the link data is not kept
across transactions; an unusable server or user fails it.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0.0 not supported\n");
}
$tls_profile = 'required';
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
ok - connection uses TLS
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

$link = mysqli_init();
$connected = mysqli_real_connect($link, $MYSQL_TLS_HOST, 'tls_required',
                                 'tls_required', $MYSQL_TLS_DB,
                                 (int)$MYSQL_TLS_PORT, null,
                                 MYSQLI_CLIENT_SSL);
tap_assert($connected, 'connection uses TLS');

newrelic_end_transaction();
newrelic_start_transaction(ini_get("newrelic.appname"));

$result = mysqli_query($link, 'SELECT 1 AS one');
tap_assert(false !== $result, 'query succeeded');
mysqli_free_result($result);
mysqli_close($link);
