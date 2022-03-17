<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should not create external metrics (and not blow up!) when curl_exec
is called with zero arguments.
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
    curl_setopt($ch, CURLOPT_NOBODY, true);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_exec();  // provoke an error
    curl_close($ch);
}

test_curl();
