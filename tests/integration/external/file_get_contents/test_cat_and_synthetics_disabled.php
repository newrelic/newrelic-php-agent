<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
1. The agent SHALL NOT add X-NewRelic-ID and X-NewRelic-Transaction headers
   to external calls when cross application tracing is disabled.
2. The agent SHALL NOT add an X-NewRelic-Synthetics header to external calls
   when the Synthetics feature is disabled.
 */

/*SKIPIF
<?php
if (!isset($_ENV["SYNTHETICS_HEADER_supportability"])) {
    die("skip: env vars required");
}
*/

/*INI
newrelic.cross_application_tracer.enabled = false
newrelic.synthetics.enabled = false
newrelic.distributed_tracing_enabled=0
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
    [{"name":"Apdex"},                                              ["??", "??", "??", "??", "??", 0]],
    [{"name":"Apdex/Uri__FILE__"},                                  ["??", "??", "??", "??", "??", 0]],
    [{"name":"External/all"},                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/allWeb"},                                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"WebTransaction/Uri__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"HttpDispatcher"},                                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction"},                                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction/Uri__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime/Uri__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},[1, "??", "??", "??", "??", "??"]]
  ]
]
*/




require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');

/* Context Without Options. */
$context = stream_context_create();
echo file_get_contents($url, false, $context);
