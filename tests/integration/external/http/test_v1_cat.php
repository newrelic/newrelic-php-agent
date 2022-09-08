<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that Cross Application Tracing (CAT)
works with version 1 of the http library (HttpRequest::send).
*/

/*SKIPIF
<?php

if (!extension_loaded('http')) {
    die("skip: http extension required\n");
}

if (version_compare(phpversion('http'), '2.0.0', '>=')) {
    die("skip: http 1.x required\n");
}

if (!isset($_ENV["ACCOUNT_supportability"]) || !isset($_ENV["APP_supportability"])) {
    die("skip: env vars required");
}
*/

/*INI
newrelic.distributed_tracing_enabled=0
newrelic.cross_application_tracer.enabled = true
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
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
    [{"name": "Supportability/Logging/Forwarding/PHP/disabled"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalApp/127.0.0.1/ENV[ACCOUNT_supportability]#ENV[APP_supportability]/all"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/ENV[ACCOUNT_supportability]#ENV[APP_supportability]/WebTransaction/Custom/tracing"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/ENV[ACCOUNT_supportability]#ENV[APP_supportability]/WebTransaction/Custom/tracing",
      "scope":"OtherTransaction/php__FILE__"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},    [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');

$rq = new HttpRequest($url, HttpRequest::METH_GET);
$rv = $rq->send();
echo $rv->getBody();
