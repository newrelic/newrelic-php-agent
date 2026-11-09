<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A transaction that a stream wrapper ends and starts again while
file_get_contents() runs SHALL NOT break the agent, and the request SHALL NOT be
recorded in either transaction.

The use of freed memory this guards against is only detected under valgrind.
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.distributed_tracing_enabled = 0
newrelic.cross_application_tracer.enabled = 0
*/

/*EXPECT
transaction restarted
done
*/

/*EXPECT_METRICS_DONT_EXIST
External/all
*/

/*EXPECT_TRACED_ERRORS
null
*/

class EndsTransactionStream {
  public $context;

  function stream_open($path, $mode, $options, &$opened_path) {
    newrelic_end_transaction();
    newrelic_start_transaction(ini_get("newrelic.appname"));
    echo "transaction restarted\n";
    return false;
  }
}

stream_wrapper_unregister('http');
stream_wrapper_register('http', 'EndsTransactionStream');

$result = @file_get_contents('http://localhost/');
echo "done\n";
