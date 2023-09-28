<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that Monolog3 instrumentation does not send log events
when forwarding is disabled.
*/

/*SKIPIF
<?php

require('skipif.inc');

*/

/*INI
newrelic.application_logging.enabled = true
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = true
newrelic.application_logging.forwarding.max_samples_stored = 10
newrelic.application_logging.forwarding.log_level = DEBUG
*/

/*EXPECT
monolog3.DEBUG: debug []
monolog3.INFO: info []
monolog3.NOTICE: notice []
monolog3.WARNING: warning []
monolog3.ERROR: error []
monolog3.CRITICAL: critical []
monolog3.ALERT: alert []
monolog3.EMERGENCY: emergency []
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
    [{"name": "Supportability/Logging/Metrics/PHP/enabled"},                      [1, "??", "??", "??", "??", "??"]]
  ]
]
*/


/*EXPECT_LOG_EVENTS
null
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
    
    $logger->debug("debug");
    $logger->info("info");
    $logger->notice("notice");
    $logger->warning("warning");
    $logger->error("error");
    $logger->critical("critical");
    $logger->alert("alert");
    $logger->emergency("emergency");
}

test_logging();