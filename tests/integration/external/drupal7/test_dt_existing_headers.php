<?php

/*DESCRIPTION
Test that DT works with drupal_http_request when called with a single header.
*/

// force the framework to avoid requiring the drupal detection file
/*INI
newrelic.framework = drupal
newrelic.distributed_tracing_enabled = true
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*EXPECT
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing Customer-Header=found tracing endpoint reached
*/

/*EXPECT_RESPONSE_HEADERS
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                            [2, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                                      [2, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                                 [2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Drupal/forced"},            [1, 0, 0, 0, 0, 0]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/TraceContext/Create/Success"},        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/CreatePayload/Success"}, [2, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/


require_once(realpath(dirname(__FILE__)) . '/../../../include/drupal_7_bootstrap.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../include/drupal_7_common.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');
$rv = drupal_http_request($url, array('headers' => array("kiteboarding" => "windsurfing")));
echo $rv->data;
$rv = drupal_http_request($url, array('headers' => array(CUSTOMER_HEADER => "windsurfing")));
echo $rv->data;
