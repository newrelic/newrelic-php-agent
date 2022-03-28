<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that agent does not crash when transactions are restarted after curl
handles are initialized.
*/

/*SKIPIF
<?php
if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}
*/

/*INI
newrelic.transaction_tracer.threshold=0
newrelic.cross_application_tracer.enabled = false
*/

/*EXPECT
X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
ok - end of function reached without crash
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"External/all"},                                [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/start_transaction"},        [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

function test_txn_restart() {
  $url = make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');

  $ch1 = curl_init($url);
  $ch2 = curl_init($url);
  $mh = curl_multi_init();

  $active = 0;

  curl_multi_add_handle($mh, $ch1);
  curl_multi_exec($mh, $active);

  newrelic_ignore_transaction();
  newrelic_end_transaction();
  newrelic_start_transaction(ini_get("newrelic.appname"));

  curl_multi_add_handle($mh, $ch2);
  do {
    curl_multi_exec($mh, $active);
  } while ($active > 0);

  curl_close($ch1);
  curl_close($ch2);
  curl_multi_close($mh);

  tap_ok("end of function reached without crash", true);
}

test_txn_restart();
