<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should send code level metrics (CLM) including function name,
class name, and lineno.
 */

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.0", "<")) {
  die("skip: CLM for PHP 5 not supported\n");
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
    "events_seen": 2
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
        "name": "Custom\/Foo\\Bar\\Vegetable::getColor",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {
        "code.lineno": 123,
        "code.namespace": "Foo\\Bar\\Vegetable",
        "code.filepath": "__FILE__",
        "code.function": "getColor"
      }
    ]
  ]
]
 */
namespace Foo\Bar {
    class Vegetable {
        public $edible;

        public $color;

        public function __construct($edible, $color = "green")
        {
            $this->edible = $edible;
            $this->color = $color;
        }

        public function isEdible()
        {
            return $this->edible;
        }

        public function getColor()
        {
            echo "Yum\n";
            return $this->color;
        }
    }

    echo "two" . newrelic_add_custom_tracer("Foo\\Bar\\Vegetable::getColor");
    $veggie = new Vegetable(true, "blue");
    $veggie->getColor();
}

