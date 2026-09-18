<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test should show proper exclusive time in metrics generated for fibers even when
Fiber::throw is used to resume a fiber.

total - exclusive for a segment is exactly the amount subtracted for its
children; suspend_time shifts total and exclusive by the same amount, so it
cancels out of their difference. fiber two has no nested custom-traced
children, so its total and exclusive are always equal (diff 0). fiber one's
diff instead reflects the one real, uninterrupted block of active work its
child (fiber two) did before the injected exception unwound it - here fiber
two's single un-suspended time_nanosleep(0.1s).
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.1", "<")) {
  die("skip: PHP 8.1+ required\n");
}
*/

/*INI
newrelic.fibers.disabled = false
*/

/*EXPECT_ERROR_EVENTS
null
*/

/*EXPECT
Starting Func 'one'
Starting Func 'two'
Caught exception: Throw an exception into two when resuming
ok - metric for fiber two exists
ok - metric for fiber one exists
ok - fiber one: total - exclusive diff is as expected
ok - fiber two: total - exclusive diff is as expected
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/integration.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');

use NewRelic\Integration\Transaction;

if (extension_loaded('newrelic')) { // Ensure PHP agent is available
    newrelic_add_custom_tracer("one");
    newrelic_add_custom_tracer("two");
}

function two()
{
    echo "Starting Func 'two'\n";
    env_var_for_expects("GUID_TWO", newrelic_get_linking_metadata()['span.id'] ?? '');
    Fiber::suspend();
    time_nanosleep(0, 100000000);
    Fiber::suspend();
    echo "Ending Func 'two'\n";
}

function one()
{
    echo "Starting Func 'one'\n";
    env_var_for_expects("GUID_ONE", newrelic_get_linking_metadata()['span.id'] ?? '');
    $fiber2 = new Fiber('two');
    $fiber2->start();
        time_nanosleep(0, 100000000);
    $fiber2->resume();
        time_nanosleep(0, 100000000);
    $fiber2->throw(new RuntimeException("Throw an exception into two when resuming"));
    Fiber::suspend();
    echo "Ending Func 'one'\n";
}

try {
    $fiber1 = new Fiber('one');
    $fiber1->start();
    time_nanosleep(0, 300000000);
    $fiber1->resume();

} catch (Throwable $e) {
    echo "Caught exception: " . $e->getMessage() . "\n";
}

$txn = new Transaction;
$metrics = $txn->getScopedMetrics();
tap_assert(isset($metrics["Custom/two"]), 'metric for fiber two exists');
tap_assert(isset($metrics["Custom/one"]), 'metric for fiber one exists');

// The diff should be approx equal to the time its child spent sleeping (0.1);
// fiber one's own Fiber::suspend() is never reached (the injected exception
// unwinds one() before it gets there), and suspend_time cancels out of the
// diff regardless, so this is purely fiber two's own amended total.
$round_one = round($metrics["Custom/one"]->total - $metrics["Custom/one"]->exclusive, 1);
tap_assert($round_one === 0.1, 'fiber one: total - exclusive diff is as expected');

// fiber two has no nested custom-traced children, so total == exclusive.
$round_two = round($metrics["Custom/two"]->total - $metrics["Custom/two"]->exclusive, 1);
tap_assert($round_two === 0.0, 'fiber two: total - exclusive diff is as expected');
