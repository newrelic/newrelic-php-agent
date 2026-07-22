<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that guzzle 7 works when transaction is ignored, with Fibers.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (version_compare(phpversion(), '8.1', '<')) {
    die("skip: PHP >= 8.1 required\n");
}
?>
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.cross_application_tracer.enabled = false
newrelic.fibers.disabled = false
*/

/*EXPECT
X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
X-NewRelic-ID=missing X-NewRelic-Transaction=missing Customer-Header=found tracing endpoint reached
*/

/*EXPECT_RESPONSE_HEADERS
*/

/*EXPECT_METRICS_DONT_EXIST
External/127.0.0.1/all, 3
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 1
  },
  [
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "OtherTransaction\/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "nr.entryPoint": true,
        "transaction.name": "OtherTransaction\/php__FILE__"
      },
      {},
      {}
    ]
  ]
]*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
require_guzzle(7);

use GuzzleHttp\Client;

function test_guzzle_txn_ignore() {
    // Ignore this transaction
    newrelic_ignore_transaction();

    /* Create URL. */
    $url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) .  '/../../../include/tracing_endpoint.php');

    $client = new Client();

    $response = $client->get($url);
    echo $response->getBody();
    Fiber::suspend();

    $response = $client->get($url, [
        'headers' => [
            'zip' => 'zap']]);
    echo $response->getBody();
    Fiber::suspend();

    $response = $client->get($url, [
        'headers' => [
            'zip' => 'zap',
            CUSTOMER_HEADER => 'zap']]);
    echo $response->getBody();
}

$fiber = new Fiber('test_guzzle_txn_ignore');
$fiber->start();
$fiber->resume();
$fiber->resume();


// End/Start a new transaction to ensure some data is added to the harvest.
// This is required by the integration test runner.
newrelic_end_transaction();
newrelic_start_transaction(ini_get("newrelic.appname"));
