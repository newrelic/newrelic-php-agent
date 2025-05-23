<?php

/*DESCRIPTION
Test that multiple calls to drupal_http_request() doesn't result in a segfault.
*/

// force the framework to avoid requiring the drupal detection file
/*INI
newrelic.framework = drupal
newrelic.distributed_tracing_enabled=0
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*EXPECT
Hello world!
Hello world!
Hello world!
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                            [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                                      [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                                 [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},                     [3, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Drupal/forced"},            [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},        [1, "??", "??", "??", "??", "??"]]
  ]
]
*/


require_once(realpath(dirname(__FILE__)) . '/../../../include/drupal_6_bootstrap.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../include/drupal_6_common.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . $EXTERNAL_HOST; 
$rv = drupal_http_request($url);
echo $rv->data."\n";
$rv = drupal_http_request($url, array());
echo $rv->data."\n";
$rv = drupal_http_request($url, array('X-Foo' => 'bar'), 'GET', NULL, 2, 2);
echo $rv->data."\n";
