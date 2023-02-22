<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics including instance information for Redis
decrement operations.
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 1
newrelic.datastore_tracer.instance_reporting.enabled = 1
*/

/*EXPECT
ok - set key
ok - check value
ok - decrement by 1
ok - check value
ok - decrement by 3
ok - check final value
ok - delete key
ok - trace nodes match
ok - datastore instance metric exists
*/

use NewRelic\Integration\Transaction;

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/integration.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/redis.inc');

function test_redis() {
  global $REDIS_HOST, $REDIS_PORT;

  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT);

  /* generate a unique key to use for this test run */
  $key = randstr(16);
  if ($redis->exists($key)) {
    die("skip: key already exists: $key\n");
  }

  /* Ensure the key doesn't persist (too much) longer than the test. */
  $redis->expire($key, 30 /* seconds */);

  tap_assert($redis->set($key, 20), 'set key');
  tap_equal('20', $redis->get($key), 'check value');
  tap_equal(19, $redis->decr($key), 'decrement by 1');
  tap_equal('19', $redis->get($key), 'check value');
  tap_equal(16, $redis->decrBy($key, 3), 'decrement by 3');
  tap_equal('16', $redis->get($key), 'check final value');

  /* Cleanup the key used by this test run. */
  tap_equal(1, $redis->del($key), 'delete key');

  $redis->close();
}

test_redis();

$txn = new Transaction;

redis_trace_nodes_match($txn, array(
  'Datastore/operation/Redis/connect',
  'Datastore/operation/Redis/exists',
  'Datastore/operation/Redis/expire',
  'Datastore/operation/Redis/decr',
  'Datastore/operation/Redis/decrby',
  'Datastore/operation/Redis/del',
  'Datastore/operation/Redis/get',
  'Datastore/operation/Redis/set',
));

redis_datastore_instance_metric_exists($txn);
