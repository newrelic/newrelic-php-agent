<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should capture and report errors NOT configured via the
newrelic.error_collector.ignore_errors setting.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0.0 not supported\n");
}
*/

/*INI
error_reporting = E_ALL | E_STRICT
newrelic.error_collector.ignore_errors = E_ALL & ~E_WARNING
display_errors=1
log_errors=0
*/

/*EXPECT_REGEX
^\s*(PHP )?Warning:\s*include\(abc.php\): Failed to open stream.*
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "/include\\(\\): Failed opening 'abc.php' for inclusion \\(include_path='.:.*'\\)/",
      "E_WARNING",
      {
        "stack_trace": "??",
        "agentAttributes": "??",
        "intrinsics": "??"
      }
    ]
  ]
]
*/

/*EXPECT_ERROR_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": "??",
    "events_seen": 1
  },
  [
    [
      {
        "type": "TransactionError",
        "timestamp": "??",
        "error.class": "E_WARNING",
        "error.message": "/include\\(\\): Failed opening 'abc.php' for inclusion \\(include_path='.:.*'\\)/",
        "transactionName": "OtherTransaction\/php__FILE__",
        "duration": "??",
        "nr.transactionGuid": "??",
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??",
        "spanId": "??"
      },
      {},
      {}
    ]
  ]
]
*/

include("abc.php");

echo("Let this serve as a warning");
