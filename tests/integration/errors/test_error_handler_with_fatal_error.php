<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should capture and report E_COMPILE_ERROR even if a custom error handler 
exists and attempts to handle E_COMPILE_ERROR
However, the following fatal error types 
cannot be handled with a user defined function: 
E_ERROR, E_PARSE, E_CORE_ERROR, E_CORE_WARNING, E_COMPILE_ERROR, E_COMPILE_WARNING 
It will therefore ignore the custom error handler.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0.0 not supported\n");
}
*/

/*INI
display_errors=1
log_errors=0
*/

/*EXPECT_REGEX
^\s*(PHP )?Warning:\s*Private methods cannot be final as they are never overridden by other classes
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Private methods cannot be final as they are never overridden by other classes",
      "E_COMPILE_WARNING",
      {
        "stack_trace": [],
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
        "error.class": "E_COMPILE_WARNING",
        "error.message": "Private methods cannot be final as they are never overridden by other classes",
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
  echo("Nothing to see here ever apparently.");
    switch ($errno) {
        case E_COMPILE_WARNING:
            return true;
        }
    return false;
}

// set to the user defined error handler
$old_error_handler = set_error_handler("errorHandlerOne");

class Foo {
  final private static function compileWarning(){
    echo 'Compile warning',"\n";
  }
}

