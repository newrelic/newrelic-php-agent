<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

declare(strict_types=1);

/*DESCRIPTION
An integer option set with mysqli_options() under strict_types=1 SHALL NOT
break the query or its explain plan.

The agent wraps mysqli_options() to record the options for the connection it
opens to run EXPLAIN. The wrapper used to read the value as a string, which
strict_types=1 rejects for an integer, so the option was not recorded. It now
reads the value as any scalar. This is a control: the effect of the integer
options that the explain connection can use is not visible in the explain plan
(MYSQLI_OPT_INT_AND_FLOAT_NATIVE, used here, does not change it), so the test
passes without the change as well. It shows that the integer option is accepted
by the wrapper and the application's query and plan are intact. The TLS test
server is used only for its MySQL version, so that the plan is the same
everywhere; the test is skipped without it and on PHP below 7.4.
*/

/*SKIPIF
<?php
require("skipif.inc");
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');
if (PHP_VERSION_ID < 70400) {
  die("skip: the TLS test users need PHP 7.4 or later\n");
}
if ('' == $MYSQL_TLS_HOST) {
  die("skip: MYSQL_TLS_HOST is not set\n");
}
mysqli_report(MYSQLI_REPORT_OFF);
$link = mysqli_init();
if (!@mysqli_real_connect($link, $MYSQL_TLS_HOST, 'tls_optional', 'tls_optional',
                          $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT)) {
  die("skip: TLS MySQL server not available: " . mysqli_connect_error() . "\n");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
ok - option set
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
        "explain_plan": [
          [
            "id",
            "select_type",
            "table",
            "partitions",
            "type",
            "possible_keys",
            "key",
            "key_len",
            "ref",
            "rows",
            "filtered",
            "Extra"
          ],
          [
            [
              1,
              "SIMPLE",
              null,
              null,
              null,
              null,
              null,
              null,
              null,
              null,
              null,
              "No tables used"
            ]
          ]
        ],
        "backtrace": "??",
        "host": "??",
        "port_path_or_id": "??",
        "database_name": "??"
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
tap_assert(mysqli_options($link, MYSQLI_OPT_INT_AND_FLOAT_NATIVE, 1), 'option set');
$connected = mysqli_real_connect($link, $MYSQL_TLS_HOST, 'tls_optional', 'tls_optional',
                                 $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT);
tap_assert($connected, 'connection');

$result = mysqli_query($link, "SELECT 1 AS one");
tap_assert(false !== $result, 'query succeeded');
mysqli_free_result($result);
mysqli_close($link);
