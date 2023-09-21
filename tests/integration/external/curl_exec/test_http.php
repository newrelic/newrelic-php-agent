<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should generate external metrics for curl_exec when the HTTP protocol
is used.
*/

/*SKIPIF
<?php
if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}
*/

/*INI
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
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/TraceContext/Create/Success"},           [4, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/CreatePayload/Success"},[4, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                                         [4, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                                    [4, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                               [4, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},                        [4, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/




require_once(realpath(dirname( __FILE__ )) . '/../../../include/tap.php');
require_once(realpath(dirname( __FILE__ )) . '/../../../include/config.php');


function test_curl()
{
    global $EXTERNAL_HOST;

    $ch = curl_init();

    curl_setopt($ch, CURLOPT_NOBODY, true);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);

    curl_setopt($ch, CURLOPT_URL, 'http://' . $EXTERNAL_HOST . '');
    tap_not_equal(false, curl_exec($ch), 'simple hostname');

    /* Query string should be stripped. */
    curl_setopt($ch, CURLOPT_URL, 'http://' . $EXTERNAL_HOST . '?a=1&b=2');
    tap_not_equal(false, curl_exec($ch), 'strip query string');

    /* Fragment should be stripped. */
    curl_setopt($ch, CURLOPT_URL, 'http://' . $EXTERNAL_HOST . '/#fragment');
    tap_not_equal(false, curl_exec($ch), 'strip fragment');

    /* Auth credentials should be stripped. */
    curl_setopt($ch, CURLOPT_URL, 'http://user:pass@' . $EXTERNAL_HOST . '');
    tap_not_equal(false, curl_exec($ch), 'strip credentials');

    curl_close($ch);
}

test_curl();
