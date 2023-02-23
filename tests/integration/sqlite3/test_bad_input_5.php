<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should gracefully handle SQLite3::exec() being invoked with no
arguments.
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
    [{"name":"Datastore/operation/SQLite/other"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/other",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/OtherTransaction/php__FILE__"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/all"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/allOther"},                                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]]
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
      "??",
      {
        "stack_trace": [
          " in SQLite3::exec called at __FILE__ (??)",
          " in test_sqlite3 called at __FILE__ (??)"
        ],
        "agentAttributes": "??",
        "intrinsics": "??"
      }
    ]
  ]
]
*/

function test_sqlite3() {
  $conn = new SQLite3(":memory:");
  $result = $conn->exec();

  if ($result) {
    die("SQLite3::exec() should have failed");
  }
}

test_sqlite3();
