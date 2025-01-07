<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should send code level metrics (CLM) including function name,
class name, and lineno for closures.
PHP 8.4+ names closures differently.
 */

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.4", "<")) {
  die("skip: older test for PHP 8.3 and below\n");
}
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.span_events_enabled=1
newrelic.cross_application_tracer.enabled=false
newrelic.code_level_metrics.enabled=true
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
        "type": "Transaction",
        "name": "OtherTransaction/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "totalTime": "??",
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??",
        "error": false
      },
      {
      },
      {}
    ]
  ]
]
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 4
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
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/{closure:__FILE__:??}",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {
        "code.lineno": 151,
        "code.filepath": "__FILE__",
        "code.function": "{closure:__FILE__:??}"
      }
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/{closure:__FILE__:??}",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {
        "code.lineno": 159,
        "code.filepath": "__FILE__",
        "code.function": "{closure:__FILE__:??}"
      }
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/{closure:__FILE__:??}",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {
        "code.lineno": 159,
        "code.filepath": "__FILE__",
        "code.function": "{closure:__FILE__:??}"
      }
    ]
  ]
]
 */

/*
 * Closure type 1
 */
echo preg_replace_callback('~-([a-z])~', function ($match) {
sleep(1);
    return strtoupper($match[1]);
}, 'hello-world');

/*
 * Closure type 2
 */
$greet = function($name) {
sleep(1);
    printf("Hello %s\r\n", $name);
};

$greet('World');
$greet('PHP');

