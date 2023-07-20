<?php

/*DESCRIPTION
1. The agent SHALL NOT add X-NewRelic-ID and X-NewRelic-Transaction headers
   to external calls when cross application tracing is disabled.
2. The agent SHALL add X-NewRelic-Synthetics headers to external calls when
   the current request is a synthetics request regardless of whether
   cross application tracing is enabled.
*/

/*XFAIL PHP-1022 */

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

/*
 * The synthetics header contains the following JSON.
 *   [
 *     1,
 *     432507,
 *     "rrrrrrr-rrrr-1234-rrrr-rrrrrrrrrrrr",
 *     "jjjjjjj-jjjj-1234-jjjj-jjjjjjjjjjjj",
 *     "mmmmmmm-mmmm-1234-mmmm-mmmmmmmmmmmm"
 *   ]
 */

/*HEADERS
X-NewRelic-Synthetics=PwcbVVVRDQMHSEMQRUNFFBZDG0EQFBFPAVALVhVKRkBBSEsTQxNBEBZERRMUERofEg4LCF1bXQxJW1xZCEtSUANWFQhSUl4fWQ9TC1sLWQgOXF0LRE8aXl0JDA9aXBoLCVxbHlNUUFYdD1UPVRVZX14IVAxcDF4PCVsVPA==
*/

/*EXPECT
X-NewRelic-ID=missing X-NewRelic-Transaction=missing X-NewRelic-Synthetics=found tracing endpoint reached
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
    [{"name":"Apdex"},                                  ["??", "??", "??", "??", "??",    0]],
    [{"name":"Apdex/Uri__FILE__"},                      ["??", "??", "??", "??", "??",    0]],
    [{"name":"External/all"},                           [   1, "??", "??", "??", "??", "??"]],
    [{"name":"External/allWeb"},                        [   1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                 [   1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"WebTransaction/Uri__FILE__"},            [   1, "??", "??", "??", "??", "??"]],
    [{"name":"HttpDispatcher"},                         [   1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Drupal/forced"}, [   1,    0,    0,    0,    0,    0]],
    [{"name":"WebTransaction"},                         [   1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction/Uri__FILE__"},             [   1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime"},                [   1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime/Uri__FILE__"},    [   1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/drupal_6_bootstrap.inc');
require_once(realpath(dirname(__FILE__)) . '/drupal_6_common.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');
$rv = drupal_http_request($url);
echo $rv->data;
