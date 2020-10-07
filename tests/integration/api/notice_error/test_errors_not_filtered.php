<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report the last error when newrelic_notice_error is called
multiple times with the same error priority.
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Report Me!",
      "NoticedError",
      {
        "stack_trace": [
          " in newrelic_notice_error called at __FILE__ (??)",
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
        "error.class": "NoticedError",
        "error.message": "Report Me!",
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

function alpha()
{
    newrelic_notice_error('Report Me!');
}

function beta()
{
    alpha();
}

function gamma($password)
{
    beta();
}

gamma('my super secret password that New Relic cannot know');
