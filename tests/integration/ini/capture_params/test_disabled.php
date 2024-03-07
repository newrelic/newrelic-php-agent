<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When capture_params is disabled, no request parameters should be captured.
*/

/*INI
newrelic.capture_params = 0
*/

/*ENVIRONMENT
REQUEST_METHOD=GET
QUERY_STRING=foo=1&bar=2
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "WebTransaction/Uri__FILE__",
      "HACK: forced error",
      "NoticedError",
      {
        "stack_trace": "??",
        "agentAttributes": {
          "response.headers.contentType": "text/html",
          "response.statusCode": 200,
          "http.statusCode": 200,
          "httpResponseCode": "200",
          "request.uri": "__FILE__",
          "SERVER_NAME": "??",
          "request.method": "GET",
          "request.headers.host": "??"
        },
        "intrinsics": "??",
        "request_uri": "??"
      },
      "?? transaction ID"
    ]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../include/helpers.php');

/*
 * Request parameters are not put into the transaction event when enabled by
 * non-attribute configuration, therefore an error or trace is required.
 */
force_error();
