<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test should show proper exclusive time in metrics generated for fibers when all
fibers are properly resumed after suspension.

total - exclusive for a segment is exactly the amount subtracted for its
children; suspend_time shifts total and exclusive by the same amount, so it
cancels out of their difference. fiber two has no nested custom-traced
children, so its total and exclusive are always equal (diff 0), regardless
of how much of it was spent suspended. fiber one's diff instead reflects the
one real, uninterrupted block of active work its child (fiber two) did
while fiber one was busy with its own work - here fiber two's single
un-suspended time_nanosleep(0.1s).
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
Ending Func 'two'
Ending Func 'one'
ok - metric for fiber two exists
ok - metric for fiber one exists
ok - fiber one: total - exclusive diff is as expected
ok - fiber two: total - exclusive diff is as expected
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/integration.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

use NewRelic\Integration\Transaction;

if (extension_loaded('newrelic')) { // Ensure PHP agent is available
    newrelic_add_custom_tracer("one");
    newrelic_add_custom_tracer("two");
}

function two()
{
    echo "Starting Func 'two'\n";
    Fiber::suspend();
    Fiber::suspend();
    time_nanosleep(0, 100000000);
    echo "Ending Func 'two'\n";
}

function one()
{
    echo "Starting Func 'one'\n";
    $fiber2 = new Fiber('two');
    $fiber2->start();
        time_nanosleep(0, 100000000);
    $fiber2->resume();
        time_nanosleep(0, 100000000);
    $fiber2->resume();
    Fiber::suspend();
    echo "Ending Func 'one'\n";
}

$fiber1 = new Fiber('one');
$fiber1->start();
time_nanosleep(0, 300000000);
$fiber1->resume();

$txn = new Transaction;
$metrics = $txn->getScopedMetrics();
tap_assert(isset($metrics["Custom/two"]), 'metric for fiber two exists');
tap_assert(isset($metrics["Custom/one"]), 'metric for fiber one exists');

// fiber one's own explicit Fiber::suspend() (0.3s) cancels out of the diff;
// what's left is exactly fiber two's own amended total - the one
// uninterrupted 0.1s time_nanosleep it did between its two suspends.
$round_one = round($metrics["Custom/one"]->total - $metrics["Custom/one"]->exclusive, 1);
tap_assert($round_one === 0.1, 'fiber one: total - exclusive diff is as expected');

// fiber two has no nested custom-traced children, so total == exclusive.
$round_two = round($metrics["Custom/two"]->total - $metrics["Custom/two"]->exclusive, 1);
tap_assert($round_two === 0.0, 'fiber two: total - exclusive diff is as expected');
