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

/*EXPECT
ok - All NR-LINKING elements present
ok - NR-LINKING present
ok - entity.guid correct
ok - entity.guid correct
ok - trace.id correct
ok - span.id is non-zero length and alphanumeric
ok - entity.name correct
ok - All NR-LINKING elements present
ok - NR-LINKING present
ok - entity.guid correct
ok - entity.guid correct
ok - trace.id correct
ok - span.id is non-zero length and alphanumeric
ok - entity.name correct
ok - All NR-LINKING elements present
ok - NR-LINKING present
ok - entity.guid correct
ok - entity.guid correct
ok - trace.id correct
ok - span.id is non-zero length and alphanumeric
ok - entity.name correct
ok - All NR-LINKING elements present
ok - NR-LINKING present
ok - entity.guid correct
ok - entity.guid correct
ok - trace.id correct
ok - span.id is non-zero length and alphanumeric
ok - entity.name correct
ok - All NR-LINKING elements present
ok - NR-LINKING present
ok - entity.guid correct
ok - entity.guid correct
ok - trace.id correct
ok - span.id is non-zero length and alphanumeric
ok - entity.name correct
ok - All NR-LINKING elements present
ok - NR-LINKING present
ok - entity.guid correct
ok - entity.guid correct
ok - trace.id correct
ok - span.id is non-zero length and alphanumeric
ok - entity.name correct
ok - All NR-LINKING elements present
ok - NR-LINKING present
ok - entity.guid correct
ok - entity.guid correct
ok - trace.id correct
ok - span.id is non-zero length and alphanumeric
ok - entity.name correct
ok - All NR-LINKING elements present
ok - NR-LINKING present
ok - entity.guid correct
ok - entity.guid correct
ok - trace.id correct
ok - span.id is non-zero length and alphanumeric
ok - entity.name correct
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
    [{"name": "Supportability/api/get_linking_metadata"},                         [16, "??", "??", "??", "??", "??"]],
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
require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');
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
    $nrlinking = $record['extra']['NR-LINKING'] ?? 'NR-LINKING DATA MISSING!!!';
    $result = preg_match("/(NR\-LINKING)\|([\w\d]+)\|([\w\d]+)\|([\w\d]+)\|([\w\d]+)\|([\w\d\%]+\.php)\|/", $nrlinking, $matches);
    $linkmeta = newrelic_get_linking_metadata();

    tap_equal(7, count($matches), "All NR-LINKING elements present");
    if (7 == count($matches)) {
      tap_equal("NR-LINKING", $matches[1], "NR-LINKING present");
      tap_equal($linkmeta['entity.guid'] ?? '<missing entity.guid>', $matches[2], "entity.guid correct");
      tap_equal($linkmeta['hostname'] ?? '<missing hostname>', $matches[3], "entity.guid correct");
      tap_equal($linkmeta['trace.id'] ?? '<missing trace.id>', $matches[4], "trace.id correct");
      tap_equal(true, strlen($matches[5]) > 0 && preg_match("/[\w\d]+/",$matches[5]), "span.id is non-zero length and alphanumeric");
      if (isset($linkmeta['entity.name'])) {
        $name = urlencode($linkmeta['entity.name']);
      } else {
        $name = '<missing entity.name>';
      }
      tap_equal($name, $matches[6], "entity.name correct");
    }
  }

  public function formatBatch(array $records) {
    foreach ($records as $key => $record) {
      $records[$key] = $this->format($record);
    }

    return $records;  
  }
}


function test_logging() {
    $logger = new Logger('monolog3');

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