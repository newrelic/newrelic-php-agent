<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Code Level Metrics (CLM) should not report a namespace/classname value 
for class names longer than 255 characters
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
      {
        "code.lineno": 1,
        "code.filepath": "__FILE__",
        "code.function": "__FILE__"
      }
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
      {
        "code.lineno": 1,
        "code.filepath": "__FILE__",
        "code.function": "__FILE__"
      }
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/TheFitnessGramPacerTestIsAMultistageAerobicCapacityTestThatProgressivelyGetsMoreDifficultAsItContinuesThe20MeterPacerTestWillBeginIn30SecondsLineUpAtTheStartTheRunningSpeedStartsSlowlyButGetsFasterEachMinuteAfterYouHearThisSignalBeepASingleLapShouldBeCompl::getLap",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {
        "code.lineno": 125,
        "code.filepath":  "__FILE__",
        "code.function":  "getLap"
      }
    ]
  ]
]
 */

class TheFitnessGramPacerTestIsAMultistageAerobicCapacityTestThatProgressivelyGetsMoreDifficultAsItContinuesThe20MeterPacerTestWillBeginIn30SecondsLineUpAtTheStartTheRunningSpeedStartsSlowlyButGetsFasterEachMinuteAfterYouHearThisSignalBeepASingleLapShouldBeCompl {
    public $start;

    public $lap;

    public function __construct($start, $lap = "0")
    {
        $this->start = $start;
        $this->lap = $lap;
    }

    public function getLap()
    {
        echo "Beep\n";
        return $this->lap;
    }
}
newrelic_add_custom_tracer("TheFitnessGramPacerTestIsAMultistageAerobicCapacityTestThatProgressivelyGetsMoreDifficultAsItContinuesThe20MeterPacerTestWillBeginIn30SecondsLineUpAtTheStartTheRunningSpeedStartsSlowlyButGetsFasterEachMinuteAfterYouHearThisSignalBeepASingleLapShouldBeCompl::getLap");
$pacer = new TheFitnessGramPacerTestIsAMultistageAerobicCapacityTestThatProgressivelyGetsMoreDifficultAsItContinuesThe20MeterPacerTestWillBeginIn30SecondsLineUpAtTheStartTheRunningSpeedStartsSlowlyButGetsFasterEachMinuteAfterYouHearThisSignalBeepASingleLapShouldBeCompl(true, "0");
$pacer->getLap();
