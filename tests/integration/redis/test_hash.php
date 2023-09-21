<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics for Redis hash operations.
*/

/*SKIPIF
<?php require("skipif.inc");
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

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [21, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [21, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                                  [21, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},                             [21, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/expire"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/expire",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hexists"},                    [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hexists",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hget"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hget",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hgetall"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hgetall",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hdel"},                       [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hdel",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hincrby"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hincrby",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hincrbyfloat"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hincrbyfloat",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hkeys"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hkeys",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hlen"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hlen",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hmget"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hmget",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hsetnx"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hsetnx",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hset"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hset",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hmset"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hmset",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hstrlen"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hstrlen",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hvals"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hvals",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/




require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/redis.inc');

function test_redis() {
  global $REDIS_HOST, $REDIS_PORT;

  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT);

  /* Generate a unique key to use for this test run. */
  $hash = randstr(16);
  if ($redis->exists($hash)) {
    die("skip: key already exists: $hash\n");
  }

  /* Ensure the hash doesn't persist longer than the test. */
  $redis->expire($hash, 30 /* seconds */);

  $key = 'foo';
  $value = 'aaa';

  tap_equal(1, $redis->hSet($hash, $key, $value), 'set hash key');
  tap_equal($value, $redis->hGet($hash, $key), 'check hash key');

  tap_assert($redis->hExists($hash, $key), 'ensure hash key exists');

  tap_equal(1, $redis->hDel($hash, $key), 'delete existing hash key');
  tap_equal(0, $redis->hDel($hash, $key), 'delete missing hash key');

  tap_refute($redis->hExists($hash, $key), 'ensure hash key does not exist');

  tap_assert($redis->hSetnx($hash, $key, $value), 're-use deleted hash key');
  tap_refute($redis->hSetnx($hash, $key, $value), 'duplicate hash key');

  tap_assert($redis->hMset($hash, array('dval' => 1.5, 'ival' => 30)), 'set multiple hash keys');
  tap_equal(32, $redis->hIncrBy($hash, 'ival', 2), 'increment hash key by int');
  tap_equal(2.5, $redis->hIncrByFloat($hash, 'dval', 1.0), 'increment hash key by float');

  $want = array('dval' => '2.5', 'ival' => '32');
  $got = $redis->hMget($hash, array('dval', 'ival'));
  tap_equal_unordered_values($want, $got, 'get multiple hash keys');

  $want[$key] = $value;
  tap_equal(count($want), $redis->hlen($hash), 'get hash length');
  tap_equal(strlen($value), $redis->hstrlen($hash, $key), 'get hash key string length');
  tap_equal_unordered($want, $redis->hgetall($hash), 'get all hash keys and values');
  tap_equal_unordered_values(array_keys($want), $redis->hkeys($hash), 'get all hash keys');
  tap_equal_unordered_values(array_values($want), $redis->hvals($hash), 'get all hash values');

  /* Cleanup the key used by this test run. */
  tap_equal(1, $redis->del($hash), 'delete hash');

  $redis->close();
}

test_redis();
