<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that nothing in Language Agent Security Policy (LASP) interferes with
the normal operation of newrelic.custom_parameters_enabled=0 (i.e. we don't
send custom parameters).
*/

/*INI
newrelic.custom_parameters_enabled = 0
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
        "error": false,
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??"
      },
      {},
      {}
    ]
  ]
]
*/

class MyClass {}

function test_add_custom_parameters() {
  newrelic_add_custom_parameter('foo', 'bar');
}

test_add_custom_parameters();
