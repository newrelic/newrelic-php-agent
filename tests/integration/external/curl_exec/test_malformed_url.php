<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test the agent's handling of malformed urls passed to curl_exec().
*/

/*SKIPIF
<?php
if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}
*/

/*INI
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
*/

/*EXPECT
ok - empty string
ok - type mismatch (boolean)
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name": "Supportability/Logging/Forwarding/PHP/disabled"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                                 [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/TraceContext/Create/Success"},     [2, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/DistributedTrace/CreatePayload/Success"},
                                                                 [2, "??", "??", "??", "??", "??"]],
    [{"name":"External/<unknown>/all"},                          [2, "??", "??", "??", "??", "??"]],
    [{"name":"External/<unknown>/all",
      "scope":"OtherTransaction/php__FILE__"},                   [2, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                                    [2, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                               [2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');

function test_curl() {
  $ch = curl_init();

  curl_setopt($ch, CURLOPT_NOBODY, true);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);

  curl_setopt($ch, CURLOPT_URL, '');
  tap_refute(curl_exec($ch), 'empty string');

  curl_setopt($ch, CURLOPT_URL, false);
  tap_refute(curl_exec($ch), 'type mismatch (boolean)');

  curl_close($ch);
}

test_curl();
