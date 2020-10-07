<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that ending a transaction after curl handles have been added
doesn't crash the daemon.
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

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

function test_stop_txn() {
  $url = make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');

  $ch1 = curl_init($url);
  $ch2 = curl_init($url);
  $mh = curl_multi_init();

  curl_multi_add_handle($mh, $ch1);
  curl_multi_exec($mh, $active);

  newrelic_end_transaction();

  curl_multi_add_handle($mh, $ch2);
  $active = null;
  do {
    curl_multi_exec($mh, $active);
  } while ($active > 0);

  curl_close($ch1);
  curl_close($ch2);
  curl_multi_close($mh);

  tap_ok("end of function reached without crash", true);
}

test_stop_txn();
