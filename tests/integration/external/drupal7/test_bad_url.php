<?php

/*DESCRIPTION
Test that our instrumentation of drupal_http_request doesn't blow up if the
first parameter is bogus.
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
ok - url is number
ok - url is false
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name": "Supportability/Logging/Forwarding/PHP/enabled"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/enabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Drupal/forced"}, [1,    0,    0,    0,    0,    0]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/drupal_7_bootstrap.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../include/drupal_7_common.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

$url = "http://" . $EXTERNAL_HOST; 
$rv = drupal_http_request(22);
tap_equal('missing schema', $rv->error, "url is number");
$rv = drupal_http_request(false);
tap_equal('missing schema', $rv->error, "url is false");
