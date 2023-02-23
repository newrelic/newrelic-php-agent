<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that distributed tracing works with file_get_contents without a context.
 */

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.distributed_tracing_exclude_newrelic_header = true
*/

/*EXPECT
traceparent=found tracestate=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
*/

/*EXPECT_RESPONSE_HEADERS
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"External/all"},                                       [5, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                                  [5, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},                      [5, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                             [5, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/TraceContext/Create/Success"},         [5, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},         [1, "??", "??", "??", "??", "??"]]
  ]
]
*/



require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');

/*
 * Seeking (offset) is not supported with remote files, so testing if this
 * parameter is maintained is not important. Similarly,
 * use_include_path doesn't have  any effect on remote files.
 */

/* Only URL. */
echo file_get_contents ($url);

/* No context. */
echo file_get_contents ($url, false);

/* NULL context. */ 
echo file_get_contents ($url, false, NULL);

/* NULL context with offset and maxlen. */
echo file_get_contents ($url, false, NULL, 0, 50000);

/* Small maxlen. */
echo file_get_contents ($url, false, NULL, 0, 128);
