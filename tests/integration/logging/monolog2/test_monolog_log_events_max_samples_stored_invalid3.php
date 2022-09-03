<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When the log events max_samples_stored limit is set to any integer 
less than 1 or greater than 10000 (max value)
the max_samples_stored value should be treated as the default (10000) value.
Setting a invalid value should result in the default value of 10000 being seen.
*/

/*SKIPIF
<?php

require('skipif.inc');

*/

/*INI
newrelic.application_logging.enabled = true
newrelic.application_logging.forwarding.enabled = true
newrelic.application_logging.metrics.enabled = true
newrelic.application_logging.forwarding.max_samples_stored = 100100
*/


/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name": "DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},            [1, "??", "??", "??", "??", "??"]],
    [{"name": "DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},       [1, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines"},                                                   [833, "??", "??", "??", "??", "??"]],
    [{"name": "Logging/lines/DEBUG"},                                             [833, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransaction/all"},                                            [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransaction/php__FILE__"},                                    [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransactionTotalTime"},                                       [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransactionTotalTime/php__FILE__"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Forwarding/PHP/enabled"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/enabled"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/PHP/Monolog/enabled"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/library/Monolog/detected"},                         [1, "??", "??", "??", "??", "??"]]
  ]
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
    
    /* maximum log event pool is 833 events every 5 second fast
     * harvest cycle.  Send that number of events and verify
     * all are accepted.
     */
    for ($i = 0; $i < 833; $i++) {
        $logger->debug("message " . $i);
    }
}


test_logging();