<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test return value of curl_multi_add_handle and curl_multi_remove_handle.
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
ok - curl handle added
ok - curl handle added
ok - curl handle removed
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
    [{"name":"OtherTransaction/all"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

function test_curl_multi_add_remove_handles() {
  $ch1 = curl_init();
  $ch2 = curl_init();

  $mh = curl_multi_init();

  $result = curl_multi_add_handle($mh, $ch1);
  if (0 === $result) {
    tap_ok("curl handle added");
  } else {
    tap_not_ok("execute request", 0, $result);
  }

  $result = curl_multi_add_handle($mh, $ch2);
  if (0 === $result) {
    tap_ok("curl handle added");
  } else {
    tap_not_ok("execute request", 0, $result);
  }

  $result = curl_multi_remove_handle($mh, $ch2);
  if (0 === $result) {
    tap_ok("curl handle removed");
  } else {
    tap_not_ok("execute request", 0, $result);
  }

  curl_close($ch1);
  curl_close($ch2);
  curl_multi_close($mh);
}

test_curl_multi_add_remove_handles();
