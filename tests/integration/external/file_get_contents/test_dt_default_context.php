<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that distributed tracing works with file_get_contents 
when the customer has added a header through the default context.
 */

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.cross_application_tracer.enabled = false
*/

/*EXPECT
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing Customer-Header=found tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing Customer-Header=found tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing Customer-Header=found tracing endpoint reached
*/

/*EXPECT_RESPONSE_HEADERS
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"External/all"},                             [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all", 
      "scope":"OtherTransaction/php__FILE__"},            [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                   [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, 
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, 
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/TraceContext/Create/Success"},    
							  [3, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/CreatePayload/Success"}, 
                                                          [3, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

/*
 * If a context is not provided to file_get_contents, our agent should add the
 * cross process headers to the default context and not a brand new context.
 * Therefore, this test uses the endpoint which requires the $CUSTOMER_HEADER
 * header to make it to the external application.
 */
$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');

$http_header['http']['header'] = CUSTOMER_HEADER . ": ok";
stream_context_set_default ($http_header);

echo file_get_contents ($url);

/*
 * Repeat the call multiple times to ensure that the default context is not
 * modified.
 */
echo file_get_contents ($url);
echo file_get_contents ($url);
