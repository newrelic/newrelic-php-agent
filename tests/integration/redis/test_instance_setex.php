<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics including instance information for Redis
increment operations.
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 1
newrelic.datastore_tracer.instance_reporting.enabled = 1
*/

/*EXPECT
ok - set key to expire in 1s
ok - retrieve key before expiry
ok - retrieve expired key
ok - delete expired key
ok - set key to expire in 1500ms
ok - retreive key before expiry
ok - retreive key ttl in miliseconds
ok - remove key expiration
ok - verify expiry removal
ok - set timestamp expiry
ok - verify expiry set
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

  /* Generate a unique key to use for this test run */
  $key = randstr(16);
  if ($redis->exists($key)) {
    echo "key already exists: ${key}\n";
    exit(1);
  }

  tap_assert($redis->setex($key, 1, 'bar'), 'set key to expire in 1s');
  tap_equal('bar', $redis->get($key), 'retrieve key before expiry');
  sleep(2);
  tap_refute($redis->get($key), 'retrieve expired key');
  tap_equal(0, $redis->del($key), 'delete expired key');  // it is no longer there: auto expiration

  tap_assert($redis->psetex($key, 1500, 'pbar'), 'set key to expire in 1500ms');
  tap_equal('pbar', $redis->get($key), 'retreive key before expiry');

  $pttl = $redis->pttl($key);
  tap_assert($pttl >= 0 && $pttl <= 1500, 'retreive key ttl in miliseconds');

  tap_assert($redis->persist($key), 'remove key expiration');
  tap_equal(-1, $redis->ttl($key), 'verify expiry removal');

  tap_assert($redis->expireat($key, time() + 1), 'set timestamp expiry');
  tap_assert($redis->ttl($key) >= 0, 'verify expiry set');

  tap_equal(1, $redis->del($key), 'delete key');

  $redis->close();
}

test_redis();

$txn = new Transaction;

redis_trace_nodes_match($txn, array(
  'Datastore/operation/Redis/close',
  'Datastore/operation/Redis/connect',
  'Datastore/operation/Redis/del',
  'Datastore/operation/Redis/exists',
  'Datastore/operation/Redis/expireat',
  'Datastore/operation/Redis/get',
  'Datastore/operation/Redis/persist',
  'Datastore/operation/Redis/psetex',
  'Datastore/operation/Redis/pttl',
  'Datastore/operation/Redis/setex',
  'Datastore/operation/Redis/ttl',
));

redis_datastore_instance_metric_exists($txn);
