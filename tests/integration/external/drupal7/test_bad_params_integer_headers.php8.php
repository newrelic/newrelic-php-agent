<?php

/*DESCRIPTION
Test that adding cross process headers doesn't blow up if the second parameter
is bogus.
*/

// force the framework to avoid requiring the drupal detection file
/*INI
newrelic.framework = drupal
newrelic.distributed_tracing_enabled=0
*/

/*SKIPIF
<?php
require("skipif.inc");
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0 not supported\n");
}
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/OtherTransaction/php__FILE__"},               [1, 0, 0, 0, 0, 0]],
    [{"name":"Errors/all"},                                        [1, 0, 0, 0, 0, 0]],
    [{"name":"Errors/allOther"},                                   [1, 0, 0, 0, 0, 0]],
    [{"name":"OtherTransaction/all"},                              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Drupal/forced"},            [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/


require_once(realpath(dirname(__FILE__)) . '/../../../include/drupal_7_bootstrap.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../include/drupal_7_common.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . $EXTERNAL_HOST;

drupal_http_request($url, array("headers" => 22));
