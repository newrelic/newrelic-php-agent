<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should *not* send code level metrics (CLM) when disabled.
*/

/*SKIPIF
<?php

require('skipif.inc');

*/

/*INI
newrelic.application_logging.enabled = true
newrelic.application_logging.forwarding.enabled = true
newrelic.application_logging.metrics.enabled = true
newrelic.application_logging.forwarding.max_samples_stored = 10
newrelic.application_logging.forwarding.log_level = DEBUG
newrelic.code_level_metrics.enabled = 0
newrelic.transaction_tracer.detail = 0
*/

/*EXPECT
monolog2.DEBUG: debug []
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name": "DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},            [1, "??", "??", "??", "??", "??"]],
    [{"name": "DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},       [1, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines"},                                                   [1, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines/DEBUG"},                                             [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransaction/all"},                                            [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransaction/php__FILE__"},                                    [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransactionTotalTime"},                                       [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransactionTotalTime/php__FILE__"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/PHP/Monolog/enabled"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/library/Monolog/detected"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/LocalDecorating/PHP/disabled"},             [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Forwarding/PHP/enabled"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/enabled"},                      [1, "??", "??", "??", "??", "??"]]
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
        "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "Custom\/Monolog\\Logger::addRecord",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "??"
      },
      {},
      {}
    ]
  ]
]
*/

/*EXPECT_LOG_EVENTS
[
    {
      "common": {
        "attributes": {}
      },
      "logs": [
        {
          "message": "debug",
          "level": "DEBUG",
          "timestamp": "??",
          "trace.id": "??",
          "span.id": "??",
          "entity.guid": "??",
          "entity.name": "tests/integration/logging/monolog2__FILE__",
          "hostname": "__HOST__"
        }
      ]
    }
  ]
 */

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/monolog.php');
require_monolog(2);

use Monolog\Logger;
use Monolog\Handler\StreamHandler;
use Monolog\Formatter\LineFormatter;


function test_logging() {
    $logger = new Logger('monolog2');

    $logfmt = "%channel%.%level_name%: %message% %context%\n";
    $formatter = new LineFormatter($logfmt);

    $stdoutHandler = new StreamHandler('php://stdout', Logger::DEBUG);
    $stdoutHandler->setFormatter($formatter);

    $logger->pushHandler($stdoutHandler);
    
    // insert delays between log messages to allow priority sampling
    // to resolve that later messages have higher precedence
    // since timestamps are only millisecond resolution
    // without delays sometimes order in output will reflect
    // all having the same timestamp.
    $logger->debug("debug");
    usleep(10000);

}

test_logging();
