<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should capture and report errors along with a stack trace 
when a custom error handler exists but it returns false for a handled
error.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.0", "<")) {
  die("skip: PHP < 7.0.0 not supported\n");
}
*/

/*INI
display_errors=1
log_errors=0
*/

/*EXPECT_REGEX
^\s*(PHP )?Deprecated:\s*Let this serve as a deprecation*
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Let this serve as a deprecation",
      "E_USER_DEPRECATED",
      {
        "stack_trace": "??",
        "agentAttributes": "??",
        "intrinsics": "??"
      },
      "?? transaction ID"
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
        "error.class": "E_USER_DEPRECATED",
        "error.message": "Let this serve as a deprecation",
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

function errorHandlerOne($errno, $errstr, $errfile, $errline)
{
    switch ($errno) {
        case E_USER_DEPRECATED:
            return false;
        }
    return false;
}

// set to the user defined error handler
$old_error_handler = set_error_handler("errorHandlerOne");


trigger_error("Let this serve as a deprecation", E_USER_DEPRECATED);  

echo("Hello from happy path!");         


