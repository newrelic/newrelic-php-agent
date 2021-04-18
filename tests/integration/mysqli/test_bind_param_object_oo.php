<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Database metrics for mysqli_stmt objects that call
mysqli_stmt::bind_param() with objects implementing __toString().
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
STATISTICS
STATISTICS
STATISTICS
STATISTICS
STATISTICS
STATISTICS
STATISTICS
STATISTICS
STATISTICS
STATISTICS
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/all"},                           [10, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                      [10, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/all"},                     [10, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/allOther"},                [10, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select"},        [10, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/tables/select"}, [10, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/tables/select",
    "scope":"OtherTransaction/php__FILE__"},             [10, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                    [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},            [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},               [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},   [ 1, "??", "??", "??", "??", "??"]]
  ]
]
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?",
      "Datastore/statement/MySQL/tables/select",
      10,
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
              "tables",
              null,
              "ALL",
              null,
              "TABLE_NAME",
              null,
              null,
              null,
              null,
              "Using where; Skip_open_table; Scanned 1 database"
            ]
          ]
        ],
        "backtrace": [
          " in mysqli_stmt::execute called at __FILE__ (??)",
          " in test_stmt_prepare called at __FILE__ (??)"
        ]
      }
    ]
  ]
]
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

class TableName {
  public function __construct($name) {
    $this->name = $name;
  }

  public function __toString() {
    return $this->name;
  }
}

function test_stmt_prepare($link, $name)
{

  $query = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?";

  $stmt = $link->prepare($query);
    if (FALSE === $stmt) {
        echo mysqli_error($link) . "\n";
        return;
    }
  if (FALSE === $stmt->bind_param('s', $name) ||
      FALSE === $stmt->execute() ||
      FALSE === $stmt->bind_result($output)) {
    echo mysqli_stmt_error() . "\n";
    mysqli_stmt_close($stmt);
    return;
  }

  while (mysqli_stmt_fetch($stmt)) {
    echo $output . "\n";
  }

  mysqli_stmt_close($stmt);
}

$link = mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if (mysqli_connect_errno()) {
  echo mysqli_connect_error() . "\n";
  exit(1);
}

/*
 * Needs to run multiple times in order to trigger any issues that
 * may occur after a garbage collection.
 */
for ($i = 0; $i < 10; $i++) {
  test_stmt_prepare($link, new TableName('STATISTICS'));
}
