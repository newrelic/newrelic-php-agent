<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Long-running workers (e.g. Laravel Octane on FrankenPHP) construct and boot the
application once. When no transaction is recording at that point (for example,
the daemon has not connected the application yet), the agent must still name
the transactions of the requests handled afterwards after the matched route.
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
OtherTransaction/Action/mock.route
*/

/*EXPECT_METRICS_DONT_EXIST
Errors/all
*/

/*EXPECT_ERROR_EVENTS null */

/*EXPECT
ok - request handled
*/

require_once __DIR__ . '/mock_worker_boot.php';
require_once __DIR__ . '/../../../include/tap.php';

/* Worker boot: no transaction is recording. */
newrelic_end_transaction(true);
$kernel = mock_worker_boot('mock.route');

/* A request served by the worker. */
newrelic_start_transaction(ini_get('newrelic.appname'));
$response = $kernel->handle(new Illuminate\Http\Request, function ($request) {
    return 'response';
});
tap_equal('response', $response, 'request handled');
