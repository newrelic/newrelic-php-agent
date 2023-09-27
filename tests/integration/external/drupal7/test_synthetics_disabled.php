<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent SHALL NOT add X-NewRelic-Synthetics headers to external calls
when the Synthetics feature is disabled.
*/

// force the framework to avoid requiring the drupal detection file
/*INI
newrelic.framework = drupal
newrelic.synthetics.enabled = false
newrelic.distributed_tracing_enabled=0
newrelic.cross_application_tracer.enabled = true
*/

/*SKIPIF
<?php
require("skipif.inc");
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
tracing endpoint reached
*/

/*EXPECT_RESPONSE_HEADERS
X-NewRelic-App-Data=??
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Apdex"},                                             ["??", "??", "??", "??", "??", 0]],
    [{"name":"Apdex/Uri__FILE__"},                                 ["??", "??", "??", "??", "??", 0]],
    [{"name":"External/all"},                                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/allWeb"},                                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalApp/127.0.0.1/432507#4741547/all"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/432507#4741547/WebTransaction/Custom/tracing"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/432507#4741547/WebTransaction/Custom/tracing",
      "scope":"WebTransaction/Uri__FILE__"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"HttpDispatcher"},                                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Drupal/forced"},            [1, 0, 0, 0, 0, 0]],
    [{"name":"WebTransaction"},                                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction/Uri__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime/Uri__FILE__"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/


require_once(realpath(dirname(__FILE__)) . '/../../../include/drupal_7_bootstrap.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../include/drupal_7_common.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');
$rv = drupal_http_request($url);
echo $rv->data;
