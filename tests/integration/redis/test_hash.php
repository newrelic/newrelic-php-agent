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
ok - delete existing hash key
ok - delete missing hash key
ok - re-use deleted hash key
ok - duplicate hash key
ok - set multiple hash keys
ok - increment hash key by int
ok - get multiple hash keys
ok - delete hash
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                         [8, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                    [8, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                   [8, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},              [8, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hget"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hget",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hmget"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hmget",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hsetnx"},      [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hsetnx",
      "scope":"OtherTransaction/php__FILE__"},         [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hset"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hset",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hmset"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/hmset",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"}, [1, "??", "??", "??", "??", "??"]]
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
    echo "hash already exists: ${hash}\n";
    exit(1);
  }

  /* Ensure the hash doesn't persist longer than the test. */
  $redis->expire($hash, 30 /* seconds */);

  $key = 'foo';
  $value = 'aaa';

  tap_equal(1, $redis->hSet($hash, $key, $value), 'set hash key');
  tap_equal($value, $redis->hGet($hash, $key), 'check hash key');
  tap_equal(1, $redis->hDel($hash, $key), 'delete existing hash key');
  tap_equal(0, $redis->hDel($hash, $key), 'delete missing hash key');

  tap_assert($redis->hSetnx($hash, $key, $value), 're-use deleted hash key');
  tap_refute($redis->hSetnx($hash, $key, $value), 'duplicate hash key');

  tap_assert($redis->hMset($hash, array('dval' => 1.5, 'ival' => 30)), 'set multiple hash keys');
  tap_equal(32, $redis->hIncrBy($hash, 'ival', 2), 'increment hash key by int');

  $want = array('dval' => '1.5', 'ival' => '32');
  $got = $redis->hMget($hash, array('dval', 'ival'));
  tap_equal_unordered($want, $got, 'get multiple hash keys');

  /* Cleanup the key used by this test run. */
  tap_equal(1, $redis->del($hash), 'delete hash');

  $redis->close();
}

test_redis();
