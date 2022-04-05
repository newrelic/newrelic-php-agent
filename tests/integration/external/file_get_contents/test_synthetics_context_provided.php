<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent SHALL add an X-NewRelic-Synthetics header to external calls when
the current request is a synthetics request.
*/

/*
 * The synthetics header contains the following JSON.
 *   [
 *     1,
 *     ENV[ACCOUNT_supportability],
 *     "rrrrrrr-rrrr-1234-rrrr-rrrrrrrrrrrr",
 *     "jjjjjjj-jjjj-1234-jjjj-jjjjjjjjjjjj",
 *     "mmmmmmm-mmmm-1234-mmmm-mmmmmmmmmmmm"
 *   ]
 */

/*SKIPIF
<?php
if (!isset($_ENV["ACCOUNT_supportability"]) || !isset($_ENV["APP_supportability"]) || !isset($_ENV["SYNTHETICS_HEADER_supportability"])) {
    die("skip: env vars required");
}
*/

/*INI
newrelic.distributed_tracing_enabled=0
newrelic.cross_application_tracer.enabled = true
*/

/*HEADERS
X-NewRelic-Synthetics=ENV[SYNTHETICS_HEADER_supportability]
*/

/*EXPECT
X-NewRelic-Synthetics=found tracing endpoint reached
X-NewRelic-Synthetics=found tracing endpoint reached
X-NewRelic-Synthetics=found tracing endpoint reached
X-NewRelic-Synthetics=found tracing endpoint reached
X-NewRelic-Synthetics=found tracing endpoint reached
X-NewRelic-Synthetics=found tracing endpoint reached
X-NewRelic-Synthetics=found Customer-Header=found tracing endpoint reached
X-NewRelic-Synthetics=found Customer-Header=found tracing endpoint reached
X-NewRelic-Synthetics=found Customer-Header=found tracing endpoint reached
X-NewRelic-Synthetics=found Customer-Header=found tracing endpoint reached
X-NewRelic-Synthetics=found Customer-Header=found tracing endpoint reached
X-NewRelic-Synthetics=found Customer-Header=found tracing endpoint reached
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
    [{"name":"Apdex"},                                    ["??", "??", "??", "??", "??",    0]],
    [{"name":"Apdex/Uri__FILE__"},                        ["??", "??", "??", "??", "??",    0]],
    [{"name":"External/all"},                             [  12, "??", "??", "??", "??", "??"]],
    [{"name":"External/allWeb"},                          [  12, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                   [  12, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalApp/127.0.0.1/ENV[ACCOUNT_supportability]#ENV[APP_supportability]/all"},
                                                          [  12, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/ENV[ACCOUNT_supportability]#ENV[APP_supportability]/WebTransaction/Custom/tracing"},
                                                          [  12, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/ENV[ACCOUNT_supportability]#ENV[APP_supportability]/WebTransaction/Custom/tracing",
      "scope":"WebTransaction/Uri__FILE__"},              [  12, "??", "??", "??", "??", "??"]],
    [{"name":"HttpDispatcher"},                           [   1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction"},                           [   1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction/Uri__FILE__"},               [   1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime"},                  [   1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime/Uri__FILE__"},      [   1, "??", "??", "??", "??", "??"]]
  ]
]
*/

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
