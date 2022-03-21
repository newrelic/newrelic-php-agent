<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that CAT works with file_get_contents when a context is provided to the
call.
 */

/*SKIPIF
<?php
if (!isset($_ENV["ACCOUNT_supportability"]) || !isset($_ENV["APP_supportability"])) {
    die("skip: env vars required");
}
*/

/*INI
newrelic.distributed_tracing_enabled=0
*/

/*EXPECT
tracing endpoint reached
tracing endpoint reached
tracing endpoint reached
tracing endpoint reached
tracing endpoint reached
tracing endpoint reached
Customer-Header=found tracing endpoint reached
Customer-Header=found tracing endpoint reached
Customer-Header=found tracing endpoint reached
Customer-Header=found tracing endpoint reached
Customer-Header=found tracing endpoint reached
Customer-Header=found tracing endpoint reached
*/

/*EXPECT_RESPONSE_HEADERS
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"External/all"},                             [12, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                        [12, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                   [12, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalApp/127.0.0.1/ENV[ACCOUNT_supportability]#ENV[APP_supportability]/all"},
                                                          [12, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/ENV[ACCOUNT_supportability]#ENV[APP_supportability]/WebTransaction/Custom/tracing"},
                                                          [12, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/ENV[ACCOUNT_supportability]#ENV[APP_supportability]/WebTransaction/Custom/tracing",
      "scope":"OtherTransaction/php__FILE__"},            [12, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                     [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},             [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},    [ 1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');

// Context Without Options
$context = stream_context_create();
echo file_get_contents($url, false, $context);

// Context Empty Array Options
$opts = array();
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context Without ['http']
$opts = array('kitesurfing' => array('windsurfing' => 'zen'));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context ['http'] = array()
$opts = array('http' => array());
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context ['http']['header'] = array()
$opts = array('http' => array('method' => 'GET', 'header' => array()));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context ['http']['header'] = ''
$opts = array('http' => array('method' => 'GET', 'header' => ''));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context ['http']['header'] = CUSTOMER_HEADER . ": zipzap"
$opts = array('http' => array('method' => 'GET', 'header' => CUSTOMER_HEADER . ": zipzap"));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context ['http']['header'] = CUSTOMER_HEADER . ": zipzap\r\n"
$opts = array('http' => array('method' => 'GET', 'header' => CUSTOMER_HEADER . ": zipzap\r\n"));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context ['http']['header'] = CUSTOMER_HEADER . ": zipzap\r\nicecream: chocolate"
$opts = array('http' => array('method' => 'GET', 'header' => CUSTOMER_HEADER . ": zipzap\r\nicecream: chocolate"));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context ['http']['header'] = CUSTOMER_HEADER . ": zipzap\r\nicecream: chocolate\r\n"
$opts = array('http' => array('method' => 'GET', 'header' => CUSTOMER_HEADER . ": zipzap\r\nicecream: chocolate\r\n"));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context ['http']['header'] = array(CUSTOMER_HEADER . ": zipzap")
$opts = array('http' => array('method' => 'GET', 'header' => array(CUSTOMER_HEADER . ": zipzap")));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

// Context ['http']['header'] = array(CUSTOMER_HEADER . ": zipzap", "icecream-two: chocolate")
$opts = array('http' => array('method' => 'GET', 'header' => array(CUSTOMER_HEADER . ": zipzap", "icecream-two: chocolate")));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);
