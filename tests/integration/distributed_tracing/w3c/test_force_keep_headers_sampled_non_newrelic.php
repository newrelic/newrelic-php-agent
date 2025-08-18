<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that remote_parent_not_sampled = 'always_on' works.
*/

/*INI
newrelic.transaction_events.attributes.include=request.uri
newrelic.distributed_tracing.sampler.remote_parent_sampled = 'always_on'
*/

/*HEADERS
X-Request-Start=1368811467146000
Content-Type=text/html
Accept=text/plain
User-Agent=Mozilla/5.0
Referer=http://user:pass@example.com/foo?q=bar#fragment
traceparent=00-87b1c9a429205b25e5b687d890d4821f-7d3efb1b173fecfa-01
*/

/*ENVIRONMENT
REQUEST_METHOD=POST
CONTENT_LENGTH=348
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
        "parentId": "7d3efb1b173fecfa",
        "transaction.name": "WebTransaction\/Uri__FILE__",
        "timestamp": "??"
      },
      {},
      {
        "parent.transportType": "HTTP",
        "response.headers.contentLength": 41,
        "response.headers.contentType": "text\/html",
        "http.statusCode": 200,
        "response.statusCode": 200,
        "httpResponseCode": "200",
        "request.uri": "__FILE__",
        "request.method": "POST",
        "request.headers.host": "127.0.0.1",
        "request.headers.contentType": "text\/html",
        "request.headers.accept": "text\/plain",
        "request.headers.contentLength": "??"
      }

    ]
  ]
]
*/

header('Content-Type: text/html');
header('Content-Length: 41');
