<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
This test ensures that when the newrelic.special = disable_laravel_queue INI setting
is set, the transaction is still properly stopped when HorizonCommand::handle is executed.
This test verifies this behavior by first making sure the transaction exists and the
transaction trace is not empty. Then it calls the HorizonCommand::handle method and
verifies that the transaction has been stopped by checking that there was no harvest received.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0 not supported\n");
}
*/

/*INI
newrelic.framework = laravel
newrelic.special = disable_laravel_queue
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
