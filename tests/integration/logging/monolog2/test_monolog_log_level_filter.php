<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that Monolog2 instrumentation filters by log level
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
newrelic.application_logging.forwarding.log_level = WARNING
*/

/*EXPECT
monolog2.DEBUG: debug []
monolog2.INFO: info []
monolog2.NOTICE: notice []
monolog2.WARNING: warning []
monolog2.ERROR: error []
monolog2.CRITICAL: critical []
monolog2.ALERT: alert []
monolog2.EMERGENCY: emergency []
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name": "DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},            [1, "??", "??", "??", "??", "??"]],
    [{"name": "DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},       [1, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/forwarding/Dropped"},                                      [3, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines"},                                                   [8, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines/ALERT"},                                             [1, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines/CRITICAL"},                                          [1, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines/EMERGENCY"},                                         [1, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines/ERROR"},                                             [1, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines/WARNING"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransaction/all"},                                            [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransaction/php__FILE__"},                                    [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransactionTotalTime"},                                       [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransactionTotalTime/php__FILE__"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/PHP/Monolog/enabled"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/library/Monolog/detected"},                         [1, "??", "??", "??", "??", "??"]]
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
          "message": "error",
          "level": "ERROR",
          "timestamp": "??",
          "trace.id": "??",
          "span.id": "??",
          "entity.guid": "??",
          "entity.name": "tests/integration/logging/monolog2__FILE__",
          "hostname": "__HOST__"
        },
        {
          "message": "critical",
          "level": "CRITICAL",
          "timestamp": "??",
          "trace.id": "??",
          "span.id": "??",
          "entity.guid": "??",
          "entity.name": "tests/integration/logging/monolog2__FILE__",
          "hostname": "__HOST__"
        },        
        {
          "message": "warning",
          "level": "WARNING",
          "timestamp": "??",
          "trace.id": "??",
          "span.id": "??",
          "entity.guid": "??",
          "entity.name": "tests/integration/logging/monolog2__FILE__",
          "hostname": "__HOST__"
        },
        {
          "message": "alert",
          "level": "ALERT",
          "timestamp": "??",
          "trace.id": "??",
          "span.id": "??",
          "entity.guid": "??",
          "entity.name": "tests/integration/logging/monolog2__FILE__",
          "hostname": "__HOST__"
        },  
        {
          "message": "emergency",
          "level": "EMERGENCY",
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
    $logger->info("info");
    usleep(10000);
    $logger->notice("notice");
    usleep(10000);
    $logger->warning("warning");
    usleep(10000);
    $logger->error("error");
    usleep(10000);
    $logger->critical("critical");
    usleep(10000);
    $logger->alert("alert");
    usleep(10000);
    $logger->emergency("emergency");
}

test_logging();