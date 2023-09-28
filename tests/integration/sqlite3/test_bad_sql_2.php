<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should record an error when a SQLite3::querysingle() fails.
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*INI
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/all"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/allOther"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/select"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/non_existent_table/select"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/non_existent_table/select",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/OtherTransaction/php__FILE__"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/all"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/allOther"},                                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/




/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "?? error message",
      "E_WARNING",
      {
        "stack_trace": [
          "\/ in SQLite3::query[sS]ingle called at .*\/",
          " in test_sqlite3_querysingle called at __FILE__ (??)"
        ],
        "agentAttributes": "??",
        "intrinsics": "??"
      }
    ]
  ]
]
*/

function test_sqlite3_querysingle() {
  $conn = new SQLite3(":memory:");
  $result = $conn->querySingle("SELECT * FROM non_existent_table;");

  if (false !== $result) {
    die("query should have returned false");
    exit(1);
  }
}

test_sqlite3_querysingle();
