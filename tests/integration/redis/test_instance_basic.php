<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics including instance information for Redis
basic operations.
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '5.4', '<')) {
    die("skip: PHP > 5.3 required\n");
}
require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 1
newrelic.datastore_tracer.instance_reporting.enabled = 1
*/

/*EXPECT
ok - ping redis
ok - set key
ok - get key
ok - get first character of key
ok - append to key
ok - get key
ok - ask redis for key length
ok - ask redis for key type
ok - delete key
ok - delete missing key
ok - mset key
ok - mget key
ok - msetnx existing key
ok - delete key
ok - msetnx non existing key
ok - delete key
ok - reuse deleted key
ok - set duplicate key
ok - verify a non-zero number of keys
ok - delete key
ok - verify reduced redis db key count
ok - trace nodes match
ok - datastore instance metric exists
*/

use NewRelic\Integration\Transaction;

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/integration.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/redis.inc');

function test_basic() {
  global $REDIS_HOST, $REDIS_PORT;

  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT);

  /* generate a unique key to use for this test run */
  $key = randstr(16);
  if ($redis->exists($key)) {
    echo "key already exists: ${key}\n";
    exit(1);
  }

  /* the tests */
  tap_assert($redis->ping(), 'ping redis');

  tap_assert($redis->set($key, 'bar'), 'set key');
  tap_equal('bar', $redis->get($key), 'get key');
  tap_equal('b', $redis->getrange($key, 0, 0), 'get first character of key');

  tap_equal(strlen('barometric'), $redis->append($key, 'ometric'), 'append to key');
  tap_equal('barometric', $redis->get($key), 'get key');
  tap_equal(strlen('barometric'), $redis->strlen($key), 'ask redis for key length');

  tap_equal(Redis::REDIS_STRING, $redis->type($key), 'ask redis for key type');

  tap_equal(1, $redis->del($key), 'delete key');
  tap_equal(0, $redis->del($key), 'delete missing key');

  tap_assert($redis->mset([$key => 'bar']), 'mset key');
  tap_equal(['bar'], $redis->mget([$key]), 'mget key');

  tap_refute($redis->msetnx([$key => 'newbar']), 'msetnx existing key');
  tap_equal(1, $redis->del($key), 'delete key');
  tap_assert($redis->msetnx([$key => 'newbar']), 'msetnx non existing key');
  tap_equal(1, $redis->del($key), 'delete key');

  tap_assert($redis->setnx($key, 'bar'), 'reuse deleted key');
  tap_refute($redis->setnx($key, 'bar'), 'set duplicate key');

  tap_assert(($db_size = $redis->dbsize()) > 0, 'verify a non-zero number of keys');

  /* cleanup the key used by this test run */
  tap_equal(1, $redis->del($key), 'delete key');

  tap_equal($db_size - 1, $redis->dbsize(), 'verify reduced redis db key count');

  /* close connection */
  $redis->close();
}

test_basic();

$txn = new Transaction;

redis_trace_nodes_match($txn, array(
  'Datastore/operation/Redis/append',
  'Datastore/operation/Redis/close',
  'Datastore/operation/Redis/connect',
  'Datastore/operation/Redis/dbsize',
  'Datastore/operation/Redis/del',
  'Datastore/operation/Redis/exists',
  'Datastore/operation/Redis/get',
  'Datastore/operation/Redis/getrange',
  'Datastore/operation/Redis/mget',
  'Datastore/operation/Redis/mset',
  'Datastore/operation/Redis/msetnx',
  'Datastore/operation/Redis/ping',
  'Datastore/operation/Redis/set',
  'Datastore/operation/Redis/setnx',
  'Datastore/operation/Redis/strlen',
  'Datastore/operation/Redis/type',
));

redis_datastore_instance_metric_exists($txn);
