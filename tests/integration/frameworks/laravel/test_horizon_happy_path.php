<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Ensure that when HorizonCommand::handle is executed, the transaction is properly stopped.
*/

/*INI
newrelic.framework = laravel
*/

/*EXPECT_HARVEST
no
*/

/*EXPECT
foobar
Custom/foobar
handle function
foobar
*/

require_once(__DIR__ . '/mock_horizon_happy_path.php');
require_once(__DIR__.'/../../../include/integration.php');

use Laravel\Horizon\Console\HorizonCommand;
use NewRelic\Integration\Transaction;

function foobar() {
    echo "foobar\n";
}
function get_txn() {
    return new Transaction;
}

newrelic_add_custom_tracer('foobar');
foobar();

$txn = get_txn();
foreach ($txn->getTrace()->findSegmentsByName('Custom/foobar') as $segment) {
    echo $segment->name;
    echo "\n";
}

$horizon = new HorizonCommand();
$horizon->handle();
foobar();
