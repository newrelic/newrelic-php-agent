<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Content-type and content-length values with preceding whitespace should be
parsed correctly.
*/

/*INI
newrelic.distributed_tracing_enabled=0
*/

/*HEADERS
X-Request-Start=1368811467146000
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
          "response.headers.contentType": "text/xml",
          "response.statusCode": 200,
          "http.statusCode": 200,
          "httpResponseCode": "200",
          "request.uri": "__FILE__",
          "SERVER_NAME": "127.0.0.1",
          "request.headers.host": "127.0.0.1"
        },
        "intrinsics": "??",
        "request_uri": "__FILE__"
      }
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
        "nr.transactionGuid": "??"
      },
      {},
      {
      	"response.headers.contentLength": 41,
        "response.headers.contentType": "text/xml",
        "response.statusCode": 200,
        "http.statusCode": 200,
        "httpResponseCode": "200",
        "request.uri": "__FILE__",
        "SERVER_NAME": "127.0.0.1",
        "request.headers.host": "127.0.0.1"
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
        "error": true
      },
      {
      },
      {
        "errorType": "NoticedError",
        "errorMessage": "I'M COVERED IN BEES!",
        "response.headers.contentLength": 41,
        "response.headers.contentType": "text/xml",
        "response.statusCode": 200,
        "http.statusCode": 200,
        "httpResponseCode": "200",
        "request.uri": "__FILE__",
        "request.headers.host": "127.0.0.1"
      }
    ]
  ]
]
*/

header("Content-Length:       \t41");
header("Content-Type:                  \ttext/xml");
newrelic_notice_error("I'M COVERED IN BEES!");
