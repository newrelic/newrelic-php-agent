<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When transaction tracer details are disabled, test that calls to long enough functions that are not custom wrapped do not appear as spans in span events.
*/

/*INI
newrelic.transaction_tracer.detail = 0
newrelic.special.expensive_node_min = 50us
*/

/*EXPECT
my_custom_function called
my_custom_function called
No alarms and no surprises.
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
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "OtherTransaction/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "nr.entryPoint": true,
        "transaction.name": "OtherTransaction/php__FILE__"
      },
      {},
      {}
    ]
  ]
]
*/

function my_custom_function() {
  error_reporting(error_reporting()); // prevent from optimizing this function away
  time_nanosleep(0, 100 * 1000); // 100 microseconds should be enough (= 2 x newrelic.special.expensive_node_min)
  echo 'my_custom_function called' . PHP_EOL;
}

my_custom_function();

my_custom_function();

echo 'No alarms and no surprises.'  . PHP_EOL;
