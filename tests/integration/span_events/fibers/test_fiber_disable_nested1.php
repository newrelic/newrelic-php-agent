<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
/*DESCRIPTION
Test that the txn is ended if PHP fiber use is detected. 
Fiber action detected on root span so only root span will exist and be named.

Output should show that PHP functionality should continue to work 
as expected.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.1", "<")) {
  die("skip: PHP 8.1+ required\n");
}
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
newrelic.span_events_enabled=1
newrelic.cross_application_tracer.enabled = false
newrelic.fibers.disabled = true
newrelic.loglevel = verbosedebug
newrelic.special = show_executes, show_execute_params, show_execute_stack, show_execute_returns, show_executes_untrimmed
*/

/*EXPECT_ERROR_EVENTS
null
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
        "name": "OtherTransaction\/php__FILE__",
        "guid": "ENV[GUID_ROOT]",
        "type": "Span",
        "category": "generic",
        "priority": "??",
        "sampled": true,
        "nr.entryPoint": true,
        "timestamp": "??",
        "transaction.name": "OtherTransaction\/php__FILE__"
      },
      {},
      {}
    ]
  ]
]
*/

/*EXPECT
Starting Func 'a'
Starting Func 'b'
Starting Func 'c'
Starting Func 'fraction'
Caught exception: Division by zero
Ending Func 'b'
Ending Func 'a'
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');


env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

function c()
{
    echo "Starting Func 'c'\n";
    env_var_for_expects("GUID_C", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 100000000);
    echo fraction(0) . "\n";
    echo "Ending Func 'c'\n";
}

function b()
{
    echo "Starting Func 'b'\n";
    env_var_for_expects("GUID_B", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 100000000);
    try {
        c();
    } catch (RuntimeException $e) {
        echo("Caught exception: " . $e->getMessage() . "\n");
    }
    echo "Ending Func 'b'\n";
}

function fraction($x) {
    echo "Starting Func 'fraction'\n";
    env_var_for_expects("GUID_FRACTION", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 100000000);
    if (!$x) {
        throw new RuntimeException('Division by zero');
    }
    echo "Ending Func 'fraction'\n";
    return 1/$x;
}

function a()
{
    echo "Starting Func 'a'\n";
    env_var_for_expects("GUID_A", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 100000000);
    Fiber::suspend();
    echo "Ending Func 'a'\n";
};

$fiber = new Fiber('a');
$fiber->start();

b();
$fiber->resume();
