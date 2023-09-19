<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that Monolog3 instrumentation adds linking metadata for log decoration
*/

/*SKIPIF
<?php

require('skipif.inc');

*/

/*INI
newrelic.loglevel = verbosedebug
newrelic.application_logging.enabled = true
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.local_decorating.enabled = true
newrelic.application_logging.metrics.enabled = true
newrelic.application_logging.forwarding.max_samples_stored = 10
newrelic.application_logging.forwarding.log_level = INFO
*/

/*EXPECT_REGEX
monolog3.DEBUG: debug NR\-LINKING|.*|.*|.*|.*|.*|
monolog3.INFO: info NR\-LINKING|.*|.*|.*|.*|.*|
monolog3.NOTICE: notice NR\-LINKING|.*|.*|.*|.*|.*|
monolog3.WARNING: warning NR\-LINKING|.*|.*|.*|.*|.*|
monolog3.ERROR: error NR\-LINKING|.*|.*|.*|.*|.*|
monolog3.CRITICAL: critical NR\-LINKING|.*|.*|.*|.*|.*|
monolog3.ALERT: alert NR\-LINKING|.*|.*|.*|.*|.*|
monolog3.EMERGENCY: emergency NR\-LINKING|.*|.*|.*|.*|.*|
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
    [{"name": "Supportability/api/get_linking_metadata"},                         [8, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/PHP/Monolog/enabled"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/library/Monolog/detected"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/LocalDecorating/PHP/enabled"},              [1, "??", "??", "??", "??", "??"]],
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
use Monolog\LogRecord;
use Monolog\Handler\StreamHandler;

/* create formatter class that echos the interesting data that the log 
   decoration should have added. */
class CheckDecorateFormatter implements Monolog\Formatter\FormatterInterface {
  public function __construct(?string $dateFormat = null) {
  }
  public function format(LogRecord $record) {
    echo $record['level_name'] ?? 'No level_name';
    echo " ";
    echo $record['message'] ?? 'No message';
    echo " ";
    echo $record['extra']['NR-LINKING'] ?? 'No decoration data exists.';
    echo "\n";
  }
  public function formatBatch(array $records) {
    foreach ($records as $key => $record) {
      $records[$key] = $this->format($record);
    }

    return $records;  
  }
}


function test_logging() {
    $logger = new Logger('monolog2');

    $formatter = new CheckDecorateFormatter();

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