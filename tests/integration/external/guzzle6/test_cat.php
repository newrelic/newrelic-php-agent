<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that CAT works with guzzle 6.
*/

/*SKIPIF
<?php
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');

if (version_compare(phpversion(), '5.5.0', '<=')) {
    die("skip: PHP > 5.5.0 required\n");
}

if (!unpack_guzzle(6)) {
    die("skip: guzzle 6 installation required\n");
}

if (!isset($_ENV["ACCOUNT_supportability"]) || !isset($_ENV["APP_supportability"])) {
    die("skip: env vars required");
}
*/

/*EXPECT
tracing endpoint reached
tracing endpoint reached
Customer-Header=found tracing endpoint reached
*/

/*EXPECT_RESPONSE_HEADERS
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"External/127.0.0.1/all"},                   [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                             [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalApp/127.0.0.1/ENV[ACCOUNT_supportability]#ENV[APP_supportability]/all"},
                                                          [3, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/ENV[ACCOUNT_supportability]#ENV[APP_supportability]/WebTransaction/Custom/tracing"},
                                                          [3, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/ENV[ACCOUNT_supportability]#ENV[APP_supportability]/WebTransaction/Custom/tracing",
      "scope":"OtherTransaction/php__FILE__"},            [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Guzzle 4-5/detected"},
                                                          [1,    0,    0,    0,    0,    0]],
    [{"name":"Supportability/library/Guzzle 6/detected"}, [1,    0,    0,    0,    0,    0]],
    [{"name":"Supportability/Unsupported/curl_setopt/CURLOPT_HEADERFUNCTION/closure"},   
                                                          [3,    0,    0,    0,    0,    0]]
  ]
]
*/

?>
<?php
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
require_guzzle(6);

/* Create URL. */
$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) .  '/../../../include/tracing_endpoint.php');

/* Use guzzle 6 to make an http request. */
use GuzzleHttp\Client;

$client = new Client();
$response = $client->get($url);
echo $response->getBody();

$response = $client->get($url, [
    'headers' => [
        'zip' => 'zap']]);
echo $response->getBody();

$response = $client->get($url, [
    'headers' => [
        'zip' => 'zap',
        CUSTOMER_HEADER => 'zap']]);
echo $response->getBody();
