<?php

/*DESCRIPTION
The agent SHALL NOT add X-NewRelic-ID and X-NewRelic-Transaction headers
to external calls when cross application tracing is disabled.
*/

// force the framework to avoid requiring the drupal detection file
/*INI
newrelic.framework = drupal
newrelic.cross_application_tracer.enabled = false
newrelic.distributed_tracing_enabled=0
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.4", ">")) {
  die("skip: PHP > 7.4.0 not supported\n");
}
*/

/*EXPECT
X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
*/

/*EXPECT_RESPONSE_HEADERS
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name": "Supportability/Logging/Forwarding/PHP/enabled"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/enabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Drupal/forced"}, [1,    0,    0,    0,    0,    0]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/drupal_6_bootstrap.inc');
require_once(realpath(dirname(__FILE__)) . '/drupal_6_common.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');
$rv = drupal_http_request($url);
echo $rv->data;
