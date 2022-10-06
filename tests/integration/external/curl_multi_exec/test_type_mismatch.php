<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should safely handle arguments that are not valid curl_multi handles.
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
    [{"name": "ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                                 [1, "??", "??", "??", "??", "??"]],
    [{"name": "ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/all"},                                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/allOther"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/OtherTransaction/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

$ch = curl_init();

$active = null;

// provoke a type mismatch
curl_multi_exec("", $active);
curl_multi_exec(false, $active);
curl_multi_exec(4, $active);
curl_multi_exec($ch, $active);

curl_close($ch);
