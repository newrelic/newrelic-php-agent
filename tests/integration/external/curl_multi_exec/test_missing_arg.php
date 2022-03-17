<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should not create external metrics (and not blow up!) when 
curl_multi_exec is called with too few arguments.
*/

/*INI
newrelic.distributed_tracing_enabled=0
*/

/*SKIPIF
<?php
if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
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

/*EXPECT_TRACED_ERRORS
[
  "??",
  [
    [
      "?? timestamp",
      "OtherTransaction/php__FILE__",
      "?? error message",
      "??",
      {
        "stack_trace": "??",
        "agentAttributes": "??",
        "intrinsics": "??"
      }
    ]
  ]
]
*/

require_once(realpath(dirname( __FILE__ )) . '/../../../include/config.php');

function test_curl()
{
    global $EXTERNAL_HOST;

    $ch = curl_init();
    curl_setopt($ch, CURLOPT_URL, $EXTERNAL_HOST);

    $cm = curl_multi_init();
    curl_multi_add_handle($cm, $ch);

    /* Call with missing arguments */
    curl_multi_exec();
    curl_multi_exec($cm);

    curl_close($ch);
    curl_multi_close($cm);
}

test_curl();
