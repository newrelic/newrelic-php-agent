<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When the application boots inside a transaction (the regular case), the
application-level wrappers are installed from Application::boot() and the
HTTP kernel fallback must not change naming or error reporting.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0 not supported\n");
}
*/

/*INI
newrelic.framework = laravel
*/

/*EXPECT_METRICS_EXIST
Errors/all
OtherTransaction/Action/App\Exceptions\Handler@render
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
        "error.class": "RuntimeException",
        "error.message": "??",
        "transactionName": "OtherTransaction\/Action\/App\\Exceptions\\Handler@render",
        "duration": "??",
        "nr.transactionGuid": "??",
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??",
        "spanId": "??"
      },
      {},
      {}
    ]
  ]
]
*/

/*EXPECT
ok - exception rendered
*/

require_once __DIR__ . '/mock_worker_boot.php';
require_once __DIR__ . '/../../../include/tap.php';

$kernel = mock_worker_boot('mock.route');

$response = $kernel->handle(new Illuminate\Http\Request, function ($request) {
    throw new RuntimeException('boom');
});
tap_equal('rendered', $response, 'exception rendered');
