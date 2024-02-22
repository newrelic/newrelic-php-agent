<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
This test exists to ensure that our test suites run with opcache
enabled, thus it will FAIL (instead of skip) if opcache is not
enabled. Our tests running with opcache on by default is
important because that is the expected mode of operation by
our customers.

Transaction event created and no errors despite creating spans
for a HUGE number of calls.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.4", "<")) {
  die("skip: PHP < 7.4 not supported\n");
}
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
error_reporting = E_ALL
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

if (!extension_loaded('Zend OPcache')) {
  die("fail: opcache not loaded");
}
if (!opcache_get_status()) {
    die("fail: opcache disabled");
}

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

