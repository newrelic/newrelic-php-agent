<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test basic Predis functionality with datastore instance reporting enabled.
*/

/*SKIPIF
<?php
if (!function_exists('newrelic_get_trace_json')) {
  die("skip: release builds of the agent do not include newrelic_get_trace_json()");
}
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 1
newrelic.datastore_tracer.instance_reporting.enabled = 1
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
*/

/*EXPECT
ok - key does not exist
ok - get key
ok - instance host matches
ok - instance port matches
ok - instance database matches
ok - instance host matches
ok - instance port matches
ok - instance database matches
ok - instance host matches
ok - instance port matches
ok - instance database matches
ok - instance host matches
ok - instance port matches
ok - instance database matches
ok - instance host matches
ok - instance port matches
ok - instance database matches
ok - instance host matches
ok - instance port matches
ok - instance database matches
*/

/*EXPECT_TRACED_ERRORS null */

use NewRelic\Integration\Transaction;
use Predis\Client;

require_once __DIR__.'/predis.inc';
require_once(__DIR__.'/../../include/config.php');
require_once(__DIR__.'/../../include/helpers.php');
require_once(__DIR__.'/../../include/tap.php');
require_once(__DIR__.'/../../include/integration.php');

global $REDIS_HOST, $REDIS_PORT;
$client = new Predis\Client(array('host' => $REDIS_HOST, 'port' => $REDIS_PORT, 'database' => 7));
try {
  $client->connect();
} catch (Exception $e) {
  die("skip: " . $e->getMessage() . "\n");
}

$key = uniqid(__FILE__, true);
tap_equal(0, $client->exists($key), 'key does not exist');

$client->set($key, 1);
$client->incr($key);
tap_equal('2', $client->get($key), 'get key');

$client->del($key);

$txn = new Transaction;
foreach ($txn->getTrace()->findSegmentsWithDatastoreInstances() as $segment) {
  $instance = $segment->getDatastoreInstance();
  tap_assert($instance->isHost($REDIS_HOST), 'instance host matches');
  tap_equal((string) $REDIS_PORT, (string) $instance->portPathOrId, 'instance port matches');
  tap_equal("7", (string) $instance->databaseName, 'instance database matches');
}
