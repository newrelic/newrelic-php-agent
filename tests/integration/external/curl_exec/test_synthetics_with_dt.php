<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should add X-NewRelic-Synthetics and DT headers to external calls when
the current request is from the Synthetics product.
*/

/*INI
newrelic.distributed_tracing_enabled = true
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
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing X-NewRelic-Synthetics=found tracing endpoint reached
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
    [{"name":"Apdex"},                                    ["??", "??", "??", "??", "??",    0]],
    [{"name":"Apdex/Uri__FILE__"},                        ["??", "??", "??", "??", "??",    0]],
    [{"name":"External/all"},                             [   1, "??", "??", "??", "??", "??"]],
    [{"name":"External/allWeb"},                          [   1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                   [   1, "??", "??", "??", "??", "??"]],
    [{"name": "External/127.0.0.1/all", "scope": "WebTransaction/Uri__FILE__"},
                                                          [   1, "??", "??", "??", "??", "??"]],
    [{"name":"HttpDispatcher"},                           [   1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction"},                           [   1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction/Uri__FILE__"},               [   1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime"},                  [   1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime/Uri__FILE__"},      [   1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, 
                                                          [   1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allWeb"}, 
                                                          [   1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/TraceContext/Create/Success"},    
							  [   1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/CreatePayload/Success"}, 
                                                          [   1, "??", "??", "??", "??", "??"]]
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
