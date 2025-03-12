<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that Monolog3 instrumentation filters context data:
Rule 3:
   include = "A, B"
   exclude = ""
   input = "A" "B" "C"
   expect = "A" "B"
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
newrelic.application_logging.forwarding.context_data.enabled = 1
newrelic.application_logging.forwarding.context_data.include = "A, B"
newrelic.application_logging.forwarding.context_data.exclude = ""
*/

/*EXPECT
monolog3.DEBUG: A B converted {"A":"A value","B":"B value","C":"C value"}
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
    [{"name": "Supportability/PHP/package/monolog/monolog/3/detected"},           [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/library/Monolog/detected"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/LocalDecorating/PHP/disabled"},             [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Forwarding/PHP/enabled"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/enabled"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Labels/PHP/disabled"},                      [1, "??", "??", "??", "??", "??"]]
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
          "message": "A B converted",
          "level": "DEBUG",
          "timestamp": "??",
          "trace.id": "??",
          "span.id": "??",
          "entity.guid": "??",
          "entity.name": "tests/integration/logging/monolog3__FILE__",
          "hostname": "__HOST__",
          "attributes": {
            "context.B": "B value",
            "context.A": "A value"
          }
        }
      ]
    }
  ]
 */

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/monolog.php');
require_monolog(3);

use Monolog\Logger;
use Monolog\Handler\StreamHandler;
use Monolog\Formatter\LineFormatter;


function test_logging() {
    $logger = new Logger('monolog3');

    $logfmt = "%channel%.%level_name%: %message% %context%\n";
    $formatter = new LineFormatter($logfmt);

    $stdoutHandler = new StreamHandler('php://stdout', Logger::DEBUG);
    $stdoutHandler->setFormatter($formatter);

    $logger->pushHandler($stdoutHandler);

    $context = array("A" => "A value", "B" => "B value", "C" => "C value");
    $logger->debug("A B converted", $context);
}

test_logging();