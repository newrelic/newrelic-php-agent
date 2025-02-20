<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that Monolog2 instrumentation converts context data to attributes
properly.
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
newrelic.application_logging.forwarding.context_data.include = ""
newrelic.application_logging.forwarding.context_data.exclude = ""
newrelic.high_security = true
*/

/*EXPECT
monolog2.DEBUG: key is string converted {"testkey_string":"value"}
monolog2.INFO: key is int not converted {"1":"value"}
monolog2.NOTICE: int value converted {"int":1}
monolog2.WARNING: dbl value converted {"dbl":3.1415926}
monolog2.ERROR: TRUE value converted {"TRUE":true}
monolog2.CRITICAL: FALSE value converted {"FALSE":false}
monolog2.ALERT: array value not converted {"array":{"foo":"bar","baz":"long"}}
monolog2.EMERGENCY: object value not converted {"object":{"Monolog\\Logger":[]}}
*/


/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name": "DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},            [1, "??", "??", "??", "??", "??"]],
    [{"name": "DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},       [1, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines"},                                                   [8, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines/ALERT"},                                             [1, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines/CRITICAL"},                                          [1, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines/DEBUG"},                                             [1, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines/EMERGENCY"},                                         [1, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines/ERROR"},                                             [1, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines/INFO"},                                              [1, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines/NOTICE"},                                            [1, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines/WARNING"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransaction/all"},                                            [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransaction/php__FILE__"},                                    [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransactionTotalTime"},                                       [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransactionTotalTime/php__FILE__"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/PHP/Monolog/enabled"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/library/Monolog/detected"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/LocalDecorating/PHP/disabled"},             [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Forwarding/PHP/disabled"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/enabled"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Labels/PHP/disabled"},                      [1, "??", "??", "??", "??", "??"]]
  ]
]
*/


/*EXPECT_LOG_EVENTS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/monolog.php');
require_monolog(2);

use Monolog\Logger;
use Monolog\Handler\StreamHandler;
use Monolog\Formatter\LineFormatter;


function test_logging()
{
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
  $context = ["testkey_string" => "value"];
  $logger->debug("key is string converted", $context);
  usleep(10000);

  $context = [1 => "value"];
  $logger->info("key is int not converted", $context);
  usleep(10000);

  $context = ["int" => 1];
  $logger->notice("int value converted", $context);
  usleep(10000);

  $context = ["dbl" => 3.1415926];
  $logger->warning("dbl value converted", $context);
  usleep(10000);

  $context = ["TRUE" => TRUE];
  $logger->error("TRUE value converted", $context);
  usleep(10000);

  $context = array("FALSE" => FALSE);
  $logger->critical("FALSE value converted", $context);
  usleep(10000);

  $context = ["array" => array('foo' => 'bar', 'baz' => 'long')];
  $logger->alert("array value not converted", $context);
  usleep(10000);

  $context = ["object" => $logger];
  $logger->emergency("object value not converted", $context);
}

test_logging();
