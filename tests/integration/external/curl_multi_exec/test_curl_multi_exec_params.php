<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should not throw a warning if the second arg to curl_multi_exec is NULL
*/

/*SKIPIF
<?php
if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}
*/

/*INI
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name": "DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},[1,"??","??","??","??","??"]],
    [{"name": "DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},[1,"??","??","??","??","??"]],
    [{"name": "External/all"},[1,"??","??","??","??","??"]],
    [{"name": "External/allOther"},[1,"??","??","??","??","??"]],
    [{"name": "External/127.0.0.1/all"},[1,"??","??","??","??","??"]],
    [{"name": "OtherTransaction/all"},[1,"??","??","??","??","??"]],
    [{"name": "OtherTransaction/php__FILE__"},[1,"??","??","??","??","??"]],
    [{"name": "OtherTransactionTotalTime"},[1,"??","??","??","??","??"]],
    [{"name": "OtherTransactionTotalTime/php__FILE__"},[1,"??","??","??","??","??"]],
    [{"name": "Supportability/DistributedTrace/CreatePayload/Success"},[1,"??","??","??","??","??"]],
    [{"name": "Supportability/Logging/Forwarding/PHP/enabled"},[1,"??","??","??","??","??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/enabled"},[1,"??","??","??","??","??"]],
    [{"name": "Supportability/TraceContext/Create/Success"},[1,"??","??","??","??","??"]],
    [{"name": "External/127.0.0.1/all","scope": "OtherTransaction/php__FILE__"},[1,"??","??","??","??","??"]]
  ]
]
*/

ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
error_reporting(E_ALL);

function req($url) {
    $curl = curl_init($url);

    curl_setopt($curl, CURLOPT_HEADER, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);

    return $curl;
}

$multi = curl_multi_init();
curl_multi_add_handle($multi, req('127.0.0.1'));

$active = null;
curl_multi_exec($multi, $active);
