<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should capture and report deprecation warnings.
*/


/*INI
newrelic.framework = no_framework
newrelic.loglevel = verbosedebug
error_reporting = E_ALL | E_STRICT
display_errors=1
log_errors=0
newrelic.enabled = 0
zend_test.observer.enabled=1
zend_test.observer.observe_all=1
*/

/*EXPECT_REGEX
^\s*(PHP )?Deprecated: Required parameter \$b follows optional parameter \$a in .*? on line [0-9]+\s*$
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Required parameter $b follows optional parameter $a",
      "Error",
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
        "error.class": "Error",
        "error.message": "Required parameter $b follows optional parameter $a",
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

function a() {
    echo "in a";
}

function myErrorHandler(int $errno, string $errstr, string $errfile, int $errline){
        time_nanosleep(0, 100000000);
        return false;
    }

$old_error_handler = set_error_handler("myErrorHandler");

a();
date_sunrise(0);
