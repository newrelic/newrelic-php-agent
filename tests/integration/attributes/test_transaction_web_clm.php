<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
In a web transaction that has no user defined functions, code level metrics (CLM)
will not be provided.  This is a conscious decision to not force a filepath to be
a function name in the case of a file.  As such, the agent should include CLM agent
attributes in error traces, error events, analytic events and span events.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.0", "<")) {
  die("skip: CLM for PHP 5 not supported\n");
}
*/

/*INI
newrelic.transaction_events.attributes.include=request.uri
newrelic.distributed_tracing_enabled=1
newrelic.span_events_enabled=1
newrelic.cross_application_tracer.enabled=false
newrelic.code_level_metrics.enabled=1
*/

/*HEADERS
X-Request-Start=1368811467146000
Content-Type=text/html
Accept=text/plain
User-Agent=Mozilla/5.0
Referer=http://user:pass@example.com/foo?q=bar#fragment
*/

/*ENVIRONMENT
REQUEST_METHOD=POST
CONTENT_LENGTH=348
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "WebTransaction/Uri__FILE__",
      "I'M COVERED IN BEES!",
      "NoticedError",
      {
        "stack_trace": [
          " in newrelic_notice_error called at __FILE__ (??)"
        ],
        "agentAttributes": {
          "response.headers.contentLength": 41,
          "response.headers.contentType": "text/html",
          "response.statusCode": 200,
          "http.statusCode": 200,
          "httpResponseCode": "200",
          "request.uri": "__FILE__",
          "SERVER_NAME": "??",
          "request.headers.userAgent": "Mozilla/5.0",
          "request.headers.User-Agent": "Mozilla/5.0",
          "request.method": "POST",
          "request.headers.host": "127.0.0.1",
          "request.headers.contentType": "text/html",
          "request.headers.accept": "text/plain",
          "request.headers.contentLength": 348,
          "request.headers.referer": "http://example.com/foo"
        },
        "intrinsics": "??",
        "request_uri": "__FILE__"
      },
      "?? transaction ID"
    ]
  ]
]
*/

/*EXPECT_ERROR_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": "??",
    "events_seen": 1
  },
  [
    [
      {
        "type": "TransactionError",
        "timestamp": "??",
        "error.class": "NoticedError",
        "error.message": "I'M COVERED IN BEES!",
        "transactionName": "WebTransaction/Uri__FILE__",
        "duration": "??",
        "queueDuration": "??",
        "nr.transactionGuid": "??",
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??",
        "spanId": "??"
      },
      {},
      {
        "response.headers.contentLength": 41,
        "response.headers.contentType": "text/html",
        "response.statusCode": 200,
        "http.statusCode": 200,
        "httpResponseCode": "200",
        "request.uri": "__FILE__",
        "SERVER_NAME": "??",
        "request.method": "POST",
        "request.headers.host": "127.0.0.1",
        "request.headers.contentType": "text/html",
        "request.headers.contentLength": 348,
        "request.headers.accept": "text/plain",
        "request.headers.userAgent": "Mozilla/5.0",
        "request.headers.User-Agent": "Mozilla/5.0",
        "request.headers.referer": "http://example.com/foo"
      }
    ]
  ]
]
*/

/*EXPECT_ANALYTICS_EVENTS
 [
  "?? agent run id",
  "?? sampling information",
  [
    [
      {
        "type": "Transaction",
        "name": "WebTransaction/Uri__FILE__",
        "timestamp": "??",
        "duration": "??",
        "totalTime": "??",
        "nr.apdexPerfZone": "F",
        "queueDuration": "??",
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??",
        "error": true
      },
      {
      },
      {
        "errorType": "NoticedError",
        "errorMessage": "I'M COVERED IN BEES!",
        "response.headers.contentLength": 41,
        "response.headers.contentType": "text/html",
        "response.statusCode": 200,
        "http.statusCode": 200,
        "httpResponseCode": "200",
        "request.uri": "__FILE__",
        "request.method": "POST",
        "request.headers.host": "127.0.0.1",
        "request.headers.contentType": "text/html",
        "request.headers.contentLength": 348,
        "request.headers.accept": "text/plain"
      }
    ]
  ]
]
 */

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 1
  },
  [
    [
      {
        "traceId": "??",
        "duration": "??",
        "transactionId": "??",
        "name": "WebTransaction\/Uri__FILE__",
        "guid": "??",
        "type": "Span",
        "category": "generic",
        "priority": "??",
        "sampled": true,
        "nr.entryPoint": true,
        "transaction.name": "WebTransaction\/Uri__FILE__",
        "timestamp": "??"
      },
      {},
      {
        "error.class": "NoticedError",
        "error.message": "I'M COVERED IN BEES!",
        "response.headers.contentLength": 41,
        "response.headers.contentType": "text/html",
        "response.statusCode": 200,
        "http.statusCode": 200,
        "httpResponseCode": "200",
        "request.uri": "__FILE__",
        "request.method": "POST",
        "request.headers.host": "127.0.0.1",
        "request.headers.contentType": "text/html",
        "request.headers.contentLength": 348,
        "request.headers.accept": "text/plain"
      }
    ]
  ]
]
*/

header('Content-Type: text/html');
header('Content-Length: 41');
newrelic_notice_error("I'M COVERED IN BEES!");
