<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that a version 0.1 payload is accepted.
*/

/*INI
newrelic.distributed_tracing_enabled=1
*/

/*EXPECT
ok - Accepted
*/


/*EXPECT_ANALYTICS_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": "??",
    "events_seen": "??"
  },
  [
    [
      {
        "type": "Transaction",
        "name": "OtherTransaction\/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "totalTime": "??",
        "guid": "??",
        "sampled": true,
        "traceId": "aabbccddeeff0000",
        "priority": 1.28674,
        "parent.type": "App",
        "parent.app": "2222222",
        "parent.account": "111111",
        "parent.transportType": "Unknown",
        "parent.transportDuration": "??",
        "parentId": "0011223344556677",
        "parentSpanId": "332c7b9a18777990",
        "error": false
      },
      {},
      {}
    ]
  ]
]
*/
require_once(realpath (dirname ( __FILE__ )) . '/../../../../include/tap.php');

$payload = '{"v":[0,1],"d":{"ty":"App","ac":"111111","ap":"2222222","id":"332c7b9a18777990","tr":"aabbccddeeff0000","pr":1.28674,"sa":true,"ti":1530311294670,"tk":"222222","tx":"0011223344556677"}}';

$result = tap_equal(true, newrelic_accept_distributed_trace_payload($payload), 'Accepted');
