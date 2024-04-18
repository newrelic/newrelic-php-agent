<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
For a certain number of error types, PHP triggers an exception if EH_THROW is toggled on.
Verify we don't record the error if the exception is thrown and caught.
There should be no traced errors, error events, or errors attached to span events.
*/

/*INI
display_errors=1
log_errors=0
error_reporting=E_ALL
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 1
  },
  [
    [
      {
        "traceId": "??",
        "duration": "??",
        "transactionId": "??",
        "name": "OtherTransaction\/php__FILE__",
        "guid": "??",
        "type": "Span",
        "category": "generic",
        "priority": "??",
        "sampled": true,
        "nr.entryPoint": true,
        "timestamp": "??",
        "transaction.name": "OtherTransaction\/php__FILE__"
      },
      {},
      {}
    ]
  ]
]
*/


/*EXPECT_REGEX
Exception caught*
*/

/*EXPECT_TRACED_ERRORS
null
*/

/*EXPECT_ERROR_EVENTS
null
*/

$base_directory = __DIR__ . '/nonexist';;

try {
       $n = new RecursiveDirectoryIterator( $base_directory ) ;
}
catch (\Exception $e) {
       echo ("Exception caught: " . $e->getMessage());
}
