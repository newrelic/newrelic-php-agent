<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
1. The agent SHALL NOT add X-NewRelic-ID and X-NewRelic-Transaction headers
   to external calls when cross application tracing is disabled.
2. The agent SHALL add X-NewRelic-Synthetics headers to external calls when
   the current request is a synthetics request regardless of whether
   cross application tracing is enabled.
*/

/*SKIPIF
<?php

if (!extension_loaded('http')) {
    die("skip: http extension required\n");
}

if (version_compare(phpversion('http'), '2.0.0', '>=')) {
    die("skip: http 1.x required\n");
}
*/

/*INI
newrelic.cross_application_tracer.enabled = false
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"External/all"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');

$rq = new HttpRequest($url, HttpRequest::METH_GET);
$rv = $rq->send();
echo $rv->getBody();
