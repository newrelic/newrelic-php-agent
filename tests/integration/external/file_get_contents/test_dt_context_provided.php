<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that Distributed Tracing (DT) works with 
file_get_contents when a context is provided to the
call.
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.cross_application_tracer.enabled = false
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
*/

/*EXPECT
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing Customer-Header=found tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing Customer-Header=found tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing Customer-Header=found tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing Customer-Header=found tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing Customer-Header=found tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing Customer-Header=found tracing endpoint reached
*/

/*EXPECT_RESPONSE_HEADERS
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name": "Supportability/Logging/Forwarding/PHP/disabled"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                             [13, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                        [13, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                   [13, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all", 
      "scope":"OtherTransaction/php__FILE__"},            [13, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                     [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},             [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},    [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, 
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, 
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/TraceContext/Create/Success"},    
							  [13, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/CreatePayload/Success"}, 
                                                          [13, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');

/* Context Without Options. */
$context = stream_context_create();
echo file_get_contents($url, false, $context);

/* Context Empty Array Options. */
$opts = array();
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

/* Reuse Existing Context (headers should not be duplicated). */
echo file_get_contents($url, false, $context);

/* Context Without ['http'] */
$opts = array('kitesurfing' => array('windsurfing' => 'zen'));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

/* Context ['http'] = array() */
$opts = array('http' => array());
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

/* Context ['http']['header'] = array() */
$opts = array('http' => array('method' => 'GET', 'header' => array()));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

/* Context ['http']['header'] = '' */
$opts = array('http' => array('method' => 'GET', 'header' => ''));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

/* Context ['http']['header'] = CUSTOMER_HEADER . ": zipzap" */
$opts = array('http' => array('method' => 'GET', 'header' => CUSTOMER_HEADER . ": zipzap"));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

/* Context ['http']['header'] = CUSTOMER_HEADER . ": zipzap\r\n" */
$opts = array('http' => array('method' => 'GET', 'header' => CUSTOMER_HEADER . ": zipzap\r\n"));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

/* Context ['http']['header'] = CUSTOMER_HEADER . ": zipzap\r\nicecream: chocolate" */
$opts = array('http' => array('method' => 'GET', 'header' => CUSTOMER_HEADER . ": zipzap\r\nicecream: chocolate"));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

/* Context ['http']['header'] = CUSTOMER_HEADER . ": zipzap\r\nicecream: chocolate\r\n" */
$opts = array('http' => array('method' => 'GET', 'header' => CUSTOMER_HEADER . ": zipzap\r\nicecream: chocolate\r\n"));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

/* Context ['http']['header'] = array(CUSTOMER_HEADER . ": zipzap") */
$opts = array('http' => array('method' => 'GET', 'header' => array(CUSTOMER_HEADER . ": zipzap")));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);

/* Context ['http']['header'] = array(CUSTOMER_HEADER . ": zipzap", "icecream-two: chocolate") */
$opts = array('http' => array('method' => 'GET', 'header' => array(CUSTOMER_HEADER . ": zipzap", "icecream-two: chocolate")));
$context = stream_context_create($opts);
echo file_get_contents($url, false, $context);
