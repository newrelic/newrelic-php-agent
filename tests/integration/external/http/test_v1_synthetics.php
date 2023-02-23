<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent SHALL add an X-NewRelic-Synthetics header to CAT requests when
the current request is a synthetics request.
*/

/*SKIPIF
<?php

if (!extension_loaded('http')) {
    die("skip: http extension required\n");
}

if (version_compare(phpversion('http'), '2.0.0', '>=')) {
    die("skip: http 1.x required\n");
}

if (!isset($_ENV["ACCOUNT_supportability"]) || !isset($_ENV["APP_supportability"]) || !isset($_ENV["SYNTHETICS_HEADER_supportability"])) {
    die("skip: env vars required");
}
*/

/*INI
newrelic.distributed_tracing_enabled=0
newrelic.cross_application_tracer.enabled = true
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

/*HEADERS
X-NewRelic-Synthetics=ENV[SYNTHETICS_HEADER_supportability]
*/

/*EXPECT
X-NewRelic-Synthetics=found tracing endpoint reached
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
    [{"name":"Apdex"},                                              ["??", "??", "??", "??", "??", 0]],
    [{"name":"Apdex/Uri__FILE__"},                                  ["??", "??", "??", "??", "??", 0]],
    [{"name":"External/all"},                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/allWeb"},                                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalApp/127.0.0.1/ENV[ACCOUNT_supportability]#ENV[APP_supportability]/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/ENV[ACCOUNT_supportability]#ENV[APP_supportability]/WebTransaction/Custom/tracing"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/ENV[ACCOUNT_supportability]#ENV[APP_supportability]/WebTransaction/Custom/tracing",
      "scope":"WebTransaction/Uri__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"HttpDispatcher"},                                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction"},                                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction/Uri__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime/Uri__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},         [1, "??", "??", "??", "??", "??"]]
  ]
]
*/



require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');

$rq = new HttpRequest($url, HttpRequest::METH_GET);
$rv = $rq->send();
echo $rv->getBody();
