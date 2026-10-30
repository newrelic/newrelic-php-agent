<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Long-running workers (e.g. Laravel Octane on FrankenPHP) construct and boot the
application once. When no transaction is recording at that point (for example,
the daemon has not connected the application yet), exceptions handled by the
Laravel exception handler during later requests must still be reported.
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

/* Worker boot: no transaction is recording. */
newrelic_end_transaction(true);
$kernel = mock_worker_boot('mock.route');

/* A request served by the worker. */
newrelic_start_transaction(ini_get('newrelic.appname'));
$response = $kernel->handle(new Illuminate\Http\Request, function ($request) {
    throw new RuntimeException('worker boom');
});
tap_equal('rendered', $response, 'exception rendered');
