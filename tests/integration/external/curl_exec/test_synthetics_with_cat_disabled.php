<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent SHALL add X-NewRelic-Synthetics headers to external calls when
the current request is a synthetics request regardless of whether
cross application tracing is enabled.
 */

/*SKIPIF
<?php
if (!$_ENV["SYNTHETICS_HEADER_supportability"]) {
    die("skip: env vars required");
}
*/

/*XFAIL Expected */

/*INI
newrelic.cross_application_tracer.enabled = false
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
X-NewRelic-ID=missing X-NewRelic-Transaction=missing X-NewRelic-Synthetics=found tracing endpoint reached
ok - execute request
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
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},[1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},         [1, "??", "??", "??", "??", "??"]]
  ]
]
*/




if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

function test_curl() {
  $url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');
  $ch = curl_init($url);

  $result = curl_exec($ch);
  if (false !== $result) {
    tap_ok("execute request");
  } else {
    tap_not_ok("execute request", true, $result);
    tap_diagnostic("errno=" . curl_errno($ch));
    tap_diagnostic("error=" . curl_error($ch));
  }

  curl_close($ch);
}

test_curl();
