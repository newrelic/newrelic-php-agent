<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
In a regular request the HTTP kernel boots the application from within
Kernel::handle(). The application-level wrappers must then be installed from
Application::boot() as usual, and the agent must not resolve any service from
the container before the application has booted.
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
ok - nothing resolved before boot
*/

require_once __DIR__ . '/mock_worker_boot.php';
require_once __DIR__ . '/../../../include/tap.php';

list($app, $kernel) = mock_kernel('mock.route', false);

$response = $kernel->handle(new Illuminate\Http\Request, function ($request) {
    throw new RuntimeException('boom');
});
tap_equal('rendered', $response, 'exception rendered');
tap_equal(array(), $app->resolvedBeforeBoot, 'nothing resolved before boot');
