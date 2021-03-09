<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should gracefully handle sqlite_query() being invoked with no
arguments.
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/all"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/all"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/allOther"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/other"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/other",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/OtherTransaction/php__FILE__"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/all"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/allOther"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"}, [1, "??", "??", "??", "??", "??"]]
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
          " in sqlite_exec called at __FILE__ (??)",
          " in test_sqlite called at __FILE__ (??)"
        ],
        "agentAttributes": "??",
        "intrinsics": "??"
      }
    ]
  ]
]
*/

function test_sqlite() {
  $result = sqlite_exec();
  if (false !== $result) {
    die("sqlite_exec() should have failed");
  }
}

test_sqlite();
