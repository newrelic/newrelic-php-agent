<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should capture database metrics and slow sqls for mysqli_query().
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = "obfuscated"
*/

/*EXPECT
key=TABLE_NAME value=STATISTICS
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"Datastore/all"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/all"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/allOther"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/TABLES/select"},[1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/TABLES/select",
      "scope":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL id",
      "SELECT TABLE_NAME FROM TABLES WHERE TABLE_NAME = ?",
      "Datastore/statement/MySQL/TABLES/select",
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
            "type",
            "possible_keys",
            "key",
            "key_len",
            "ref",
            "rows",
            "Extra"
          ],
          [
            [
              1,
              "SIMPLE",
              "TABLES",
              "ALL",
              null,
              "TABLE_NAME",
              null,
              null,
              null,
              "Using where; Skip_open_table; Scanned 1 database"
            ]
          ]
        ],
        "backtrace": [
          " in mysqli_query called at __FILE__ (??)",
          " in test_mysqli_query called at __FILE__ (??)"
        ]
      }
    ]
  ]
]
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath (dirname ( __FILE__ )) . '/mysqli.inc');

function test_mysqli_query($link)
{
  $query = "SELECT TABLE_NAME FROM TABLES WHERE TABLE_NAME = 'STATISTICS'";
  $result = mysqli_query($link, $query);

  if (FALSE === $result) {
    echo mysqli_error($link);
    return;
  }

  while ($row = mysqli_fetch_assoc($result)) {
    foreach ($row as $key => $value) {
      printf("key=${key} value=${value}\n");
    }
  }

  mysqli_free_result($result);
}

$link = mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if (mysqli_connect_errno()) {
  echo mysqli_connect_error() . "\n";
  exit(1);
}

test_mysqli_query($link);
mysqli_close($link);
