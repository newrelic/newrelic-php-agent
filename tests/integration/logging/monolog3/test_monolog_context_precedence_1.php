<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that Monolog3 instrumentation obeys specific enable over general
when disabled.
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
newrelic.attributes.enabled = 1
newrelic.application_logging.forwarding.context_data.enabled = 0
newrelic.application_logging.forwarding.context_data.include = ""
newrelic.application_logging.forwarding.context_data.exclude = ""
*/

/*EXPECT
monolog3.DEBUG: key is string converted {"testkey_string":"value"}
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


/*EXPECT_LOG_EVENTS
[
  {
    "common": {
      "attributes": {}
    },
    "logs": [
      {
        "message": "key is string converted",
        "level": "DEBUG",
        "trace.id": "??",
        "span.id": "??",
        "entity.guid": "??",
        "entity.name": "tests\/integration\/logging\/monolog3__FILE__",
        "hostname": "__HOST__",
        "timestamp": "??"
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


function test_logging()
{
  $logger = new Logger('monolog3');

  $logfmt = "%channel%.%level_name%: %message% %context%\n";
  $formatter = new LineFormatter($logfmt);

  $stdoutHandler = new StreamHandler('php://stdout', Logger::DEBUG);
  $stdoutHandler->setFormatter($formatter);

  $logger->pushHandler($stdoutHandler);
  $context = ["testkey_string" => "value"];
  $logger->debug("key is string converted", $context);
}

test_logging();
