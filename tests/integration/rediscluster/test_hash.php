<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore/Redis metrics for \RedisCluster hash operations,
mirroring the coverage of tests/integration/redis/test_hash.php.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT
ok - set hash key
ok - check hash key
ok - ensure hash key exists
ok - delete existing hash key
ok - delete missing hash key
ok - ensure hash key does not exist
ok - re-use deleted hash key
ok - duplicate hash key
ok - set multiple hash keys
ok - increment hash key by int
ok - increment hash key by float
ok - get multiple hash keys
ok - get hash length
ok - get hash key string length
ok - get all hash keys and values
ok - get all hash keys
ok - get all hash values
ok - delete hash
*/

/*EXPECT_METRICS_EXIST
Datastore/all
Datastore/allOther
Datastore/Redis/all
Datastore/Redis/allOther
Datastore/operation/Redis/hset
Datastore/operation/Redis/hget
Datastore/operation/Redis/hexists
Datastore/operation/Redis/hdel
Datastore/operation/Redis/hsetnx
Datastore/operation/Redis/hmset
Datastore/operation/Redis/hincrby
Datastore/operation/Redis/hincrbyfloat
Datastore/operation/Redis/hmget
Datastore/operation/Redis/hlen
Datastore/operation/Redis/hstrlen
Datastore/operation/Redis/hgetall
Datastore/operation/Redis/hkeys
Datastore/operation/Redis/hvals
Datastore/operation/Redis/del
Datastore/operation/Redis/exists
Datastore/operation/Redis/expire
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/operation/Redis/connect
Datastore/operation/Redis/pconnect
Datastore/operation/Redis/open
Datastore/operation/Redis/popen
Datastore/operation/Redis/select
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

function test_hash_cluster() {
  global $REDIS_CLUSTER_HOST, $REDIS_CLUSTER_PORT;

  $cluster = new RedisCluster(null,
    [$REDIS_CLUSTER_HOST . ':' . $REDIS_CLUSTER_PORT],
    1.5, 1.5, true);

  /* Generate a unique key to use for this test run. */
  $hash = randstr(16);
  if ($cluster->exists($hash)) {
    die("skip: key already exists: $hash\n");
  }

  /* Ensure the hash doesn't persist longer than the test. */
  $cluster->expire($hash, 30 /* seconds */);

  $key = 'foo';
  $value = 'aaa';

  tap_equal(1, $cluster->hSet($hash, $key, $value), 'set hash key');
  tap_equal($value, $cluster->hGet($hash, $key), 'check hash key');

  tap_assert($cluster->hExists($hash, $key), 'ensure hash key exists');

  tap_equal(1, $cluster->hDel($hash, $key), 'delete existing hash key');
  tap_equal(0, $cluster->hDel($hash, $key), 'delete missing hash key');

  tap_refute($cluster->hExists($hash, $key), 'ensure hash key does not exist');

  tap_assert($cluster->hSetnx($hash, $key, $value), 're-use deleted hash key');
  tap_refute($cluster->hSetnx($hash, $key, $value), 'duplicate hash key');

  tap_assert($cluster->hMset($hash, array('dval' => 1.5, 'ival' => 30)), 'set multiple hash keys');
  tap_equal(32, $cluster->hIncrBy($hash, 'ival', 2), 'increment hash key by int');
  tap_equal(2.5, $cluster->hIncrByFloat($hash, 'dval', 1.0), 'increment hash key by float');

  $want = array('dval' => '2.5', 'ival' => '32');
  $got = $cluster->hMget($hash, array('dval', 'ival'));
  tap_equal_unordered_values($want, $got, 'get multiple hash keys');

  $want[$key] = $value;
  tap_equal(count($want), $cluster->hlen($hash), 'get hash length');
  tap_equal(strlen($value), $cluster->hstrlen($hash, $key), 'get hash key string length');
  tap_equal_unordered($want, $cluster->hgetall($hash), 'get all hash keys and values');
  tap_equal_unordered_values(array_keys($want), $cluster->hkeys($hash), 'get all hash keys');
  tap_equal_unordered_values(array_values($want), $cluster->hvals($hash), 'get all hash values');

  /* Cleanup the key used by this test run. */
  tap_equal(1, $cluster->del($hash), 'delete hash');

  $cluster->close();
}

test_hash_cluster();
