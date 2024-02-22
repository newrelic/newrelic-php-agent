<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Transaction event created and no errors despite creating spans
for a HUGE number of calls.
*/

/*SKIPIF
<?php

require('skipif.inc');
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
error_reporting = E_ALL
opcache.enable=0
opcache.enable_cli=0
opcache.file_update_protection=0
opcache.jit_buffer_size=32M
opcache.jit=function
*/

/*EXPECT_ANALYTICS_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 50,
    "events_seen": 1
  },
  [
    [
      {
        "traceId": "??",
        "duration": "??",
        "timestamp": "??",
        "type": "Transaction",
        "name": "OtherTransaction\/php__FILE__",
        "guid": "??",
        "priority": "??",
        "sampled": true,
        "totalTime": "??",
        "error": false
      },
      {},
      {}
    ]
  ]
]
*/


/*EXPECT
Hello
*/
require('opcache_test.inc');

newrelic_add_custom_tracer('computation');

function computation(float $a): int
{

    $b = intval($a) % (2 ** 32);
    return $b;
}

for ($i = 0; $i < 500; ++$i) {
    computation(2**64);
}
echo 'Hello';
