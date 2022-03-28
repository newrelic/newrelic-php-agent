<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should generate external metrics for curl_multi_exec when the HTTP
protocol is used.
*/

/*SKIPIF
<?php
if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}
*/

/*EXPECT
ok - simple hostname
ok - strip query string
ok - strip fragment
ok - strip credentials
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"External/all"},                                    [4, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                               [4, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                          [4, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},                   [4, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname( __FILE__ )) . '/../../../include/tap.php');
require_once(realpath(dirname( __FILE__ )) . '/../../../include/config.php');

function test_multi_url($url, $msg) {
    $cm = curl_multi_init();

    $ch = curl_init();
    curl_setopt($ch, CURLOPT_NOBODY, true);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_URL, $url);
    curl_multi_add_handle($cm, $ch);

    $active = 0;

    do {
	curl_multi_exec($cm, $active);
    } while ($active > 0);

    /* No errors */
    $info = curl_multi_info_read($cm);
    tap_ok($msg, $info["result"] == 0);

    curl_multi_close($cm);

    curl_close($ch);
}


function test_curl()
{
    global $EXTERNAL_HOST;

    test_multi_url('http://' . $EXTERNAL_HOST . '', 'simple hostname');
    test_multi_url('http://' . $EXTERNAL_HOST . '?a=1&b=2', 'strip query string');
    test_multi_url('http://' . $EXTERNAL_HOST . '/#fragment', 'strip fragment');
    test_multi_url('http://user:pass@' . $EXTERNAL_HOST . '', 'strip credentials');
}

test_curl();
