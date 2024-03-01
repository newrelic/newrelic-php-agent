<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Transaction event generated with JIT enabled even when doing lots of loops.
*/

/*SKIPIF
<?php

require('skipif.inc');

*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
newrelic.cross_application_tracer.enabled = false
error_reporting = E_ALL
opcache.enable=1
opcache.enable_cli=1
opcache.file_update_protection=0
opcache.jit_buffer_size=32M
opcache.jit=tracing
*/

/*PHPMODULES
zend_extension=opcache.so
*/

/*EXPECT_ERROR_EVENTS null */

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
5000
*/


function even(int $a): bool {
    if ($a == 1)  {
        return false;
    } else if (($a % 2) == 0) {
            return true;
        }
    return false;
}

$count = 0;
for ($i = 1; $i <= 10000; $i++)
{
        if (even($i)) $count++;
}
echo "$count";

