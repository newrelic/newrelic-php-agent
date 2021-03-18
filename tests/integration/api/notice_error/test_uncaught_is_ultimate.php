<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should prioritize uncaught errors above all others.
*/

/*INI
newrelic.error_collector.prioritize_api_errors = true
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Uncaught exception 'Exception' with message 'Sample Exception' in __FILE__:??",
      "Exception",
      {
        "stack_trace": [
          " in alpha called at __FILE__ (??)",
          " in beta called at __FILE__ (??)",
          " in gamma called at __FILE__ (??)"
        ],
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
        "error.class": "Exception",
        "error.message": "Uncaught exception 'Exception' with message 'Sample Exception' in __FILE__:??",
        "transactionName": "OtherTransaction\/php__FILE__",
        "duration": "??",
        "nr.transactionGuid": "??"
      },
      {},
      {}
    ]
  ]
]
*/

function nop_error_handler($errno, $errstr, $errfile, $errline)
{
    /*
     * Nothing to do. This is only here to prevent early termination
     * due to an unhandled error.
     */
}

function alpha()
{
    throw new Exception('Sample Exception');
}

function beta()
{
    alpha();
}

function gamma($password)
{
    beta();
}

set_error_handler('nop_error_handler');

/* cause three errors to be noticed in order of increasing priority. */
trigger_error("ignore me", E_USER_ERROR);
newrelic_notice_error(new Exception('Random Exception'));
gamma('my super secret password that New Relic cannot know');
