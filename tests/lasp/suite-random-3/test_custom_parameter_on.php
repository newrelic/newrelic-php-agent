<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests newrelic_add_custom_parameter() sends no custom attributes when LASP configuration
indicates custom_parameter:{enabled:false}, but agent is configured to allow
custom_parameters
*/

/*INI
newrelic.custom_parameters_enabled = 1
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
        "error": "??"
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
