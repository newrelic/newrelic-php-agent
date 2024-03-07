<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should attempt to determine the current mime type
by examining the response headers, and fallback to the SAPI mimetype field
if a Content-Type header has not been set.
*/

/*ENVIRONMENT
REQUEST_METHOD=GET
*/

/*INI
newrelic.distributed_tracing_enabled=0
*/

/*EXPECT
{"articlebody":"<body>Leave me alone!</body>"}
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
          "SERVER_NAME": "127.0.0.1",
          "response.statusCode": 200,
          "http.statusCode": 200,
          "httpResponseCode": "200",
          "request.headers.host": "127.0.0.1",
          "request.uri": "__FILE__",
          "request.method": "GET",
          "response.headers.contentType": "application/json"
        },
        "intrinsics": {
          "totalTime": "??",
          "cpu_time": "??",
          "cpu_user_time": "??",
          "cpu_sys_time": "??",
          "guid": "??"
        },
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
        "nr.transactionGuid": "??",
        "guid": "??"
      },
      {},
      {
        "SERVER_NAME": "127.0.0.1",
        "response.statusCode": 200,
        "http.statusCode": 200,
        "httpResponseCode": "200",
        "request.uri": "__FILE__",
        "request.headers.host": "127.0.0.1",
        "request.method": "GET",
        "response.headers.contentType": "application/json"
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
        "error": true
      },
      {
      },
      {
        "errorType": "NoticedError",
        "errorMessage": "I'M COVERED IN BEES!",
        "response.statusCode": 200,
        "http.statusCode": 200,
        "httpResponseCode": "200",
        "request.uri": "__FILE__",
        "request.headers.host": "127.0.0.1",
        "request.method": "GET",
        "response.headers.contentType": "application/json"
      }
    ]
  ]
]
*/

/* 
 * Add multiple headers to ensure the agent correctly iterates over the
 * response headers.
 */
header('Zip: zap');
header('Foo: bar');

/* 
 * Now add two Content-Type headers to ensure the agent prefers the value
 * from the response header over the SAPI mimetype field. This checks an
 * odd PHP behavior: the SAPI mimetype field contains the value of the
 * first Content-Type header. Subsequent changes to the Content-Type do not
 * update the SAPI mimetype field. The reason for this behavior is not
 * documented, it may not even be intentional.
 */
header('Content-Type: text/html');
header('Content-Type: application/json');

newrelic_notice_error("I'M COVERED IN BEES!");

?>
{"articlebody":"<body>Leave me alone!</body>"}
