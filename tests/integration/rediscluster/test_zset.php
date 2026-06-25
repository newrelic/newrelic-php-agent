<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore/Redis metrics for \RedisCluster sorted set
operations, mirroring the coverage of tests/integration/redis/test_zset.php.
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
ok - add elements to first sorted set
ok - add element to second sorted set
ok - get cardinality of first sorted set
ok - count elements in first sorted set by score
ok - increment element in second sorted set
ok - compute intersection of two sorted sets
ok - list intersection keys w/scores
ok - compute union of two sorted sets
ok - list union keys w/scores
ok - count elements in lexographical range
ok - list elements in lexographical range
ok - list elements in reverse lexographical range
ok - list elements by score
ok - list elements by reverse score
ok - get rank of an element
ok - get the score of an element
ok - get reverse rank of an element
ok - remove first element from first set
ok - delete working sets keys
ok - add elements to new set
ok - remove elements by rank
ok - remove elements by lexographical value
ok - remove final element by score
ok - ensure all keys are cleaned up
*/

/*EXPECT_METRICS_EXIST
Datastore/all
Datastore/allOther
Datastore/Redis/all
Datastore/Redis/allOther
Datastore/operation/Redis/zadd
Datastore/operation/Redis/zcard
Datastore/operation/Redis/zcount
Datastore/operation/Redis/zincrby
Datastore/operation/Redis/zinterstore
Datastore/operation/Redis/zunionstore
Datastore/operation/Redis/zlexcount
Datastore/operation/Redis/zrange
Datastore/operation/Redis/zrangebylex
Datastore/operation/Redis/zrangebyscore
Datastore/operation/Redis/zrank
Datastore/operation/Redis/zscore
Datastore/operation/Redis/zrevrange
Datastore/operation/Redis/zrevrangebylex
Datastore/operation/Redis/zrevrangebyscore
Datastore/operation/Redis/zrevrank
Datastore/operation/Redis/zrem
Datastore/operation/Redis/zremrangebyrank
Datastore/operation/Redis/zremrangebylex
Datastore/operation/Redis/zremrangebyscore
Datastore/operation/Redis/del
Datastore/operation/Redis/exists
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

function test_zset_cluster() {
  global $REDIS_CLUSTER_HOST, $REDIS_CLUSTER_PORT;

  $cluster = new RedisCluster(null,
    [$REDIS_CLUSTER_HOST . ':' . $REDIS_CLUSTER_PORT],
    1.5, 1.5, true);

  /*
   * All three keys share a hashtag so multi-key operations
   * route to the same slot, which is required in cluster mode.
   */
  $tag  = '{rc_zset}';
  $suffix = randstr(16);
  $key1 = $tag . '_a_' . $suffix;
  $key2 = $tag . '_b_' . $suffix;
  $dkey = $tag . '_d_' . $suffix;

  if ($cluster->exists([$key1, $key2, $dkey])) {
    die("skip: key(s) already exist: $key1, $key2, $dkey\n");
  }

  tap_equal(2, $cluster->zadd($key1, 3, 'Fizz', 5, 'Buzz'), 'add elements to first sorted set');
  tap_equal(2, $cluster->zadd($key2, 3, 'Fizz', 15, 'FizzBuzz'), 'add element to second sorted set');
  tap_equal(2, $cluster->zcard($key1), 'get cardinality of first sorted set');
  tap_equal(1, $cluster->zcount($key1, 0, 3), 'count elements in first sorted set by score');
  tap_equal(30.0, $cluster->zincrby($key2, 15, 'FizzBuzz'), 'increment element in second sorted set');

  tap_equal(1, $cluster->zinterstore($dkey, [$key1, $key2]), 'compute intersection of two sorted sets');
  tap_equal(['Fizz' => 6.0], $cluster->zrange($dkey, 0, -1, true), 'list intersection keys w/scores');

  tap_equal(3, $cluster->zunionstore($dkey, [$key1, $key2]), 'compute union of two sorted sets');
  tap_equal(['FizzBuzz' => 30.0, 'Fizz' => 6.0, 'Buzz' => 5.0], $cluster->zrevrange($dkey, 0, -1, true), 'list union keys w/scores');

  tap_equal(2, $cluster->zlexcount($key2, '[Fizz', '[FizzZ'), 'count elements in lexographical range');
  tap_equal(['Fizz', 'FizzBuzz'], $cluster->zrangebylex($key2, '[Fizz', '[FizzZ'), 'list elements in lexographical range');
  tap_equal(['FizzBuzz', 'Fizz'], $cluster->zrevrangebylex($key2, '[FizzZ', '[Fizz'), 'list elements in reverse lexographical range');
  tap_equal(['Fizz', 'FizzBuzz'], $cluster->zrangebyscore($key2, '-inf', '+inf'), 'list elements by score');
  tap_equal(['FizzBuzz', 'Fizz'], $cluster->zrevrangebyscore($key2, '+inf', '-inf'), 'list elements by reverse score');

  tap_equal(1, $cluster->zrank($key2, 'FizzBuzz'), 'get rank of an element');
  tap_equal(30.0, $cluster->zscore($key2, 'FizzBuzz'), 'get the score of an element');
  tap_equal(0, $cluster->zrevrank($key2, 'FizzBuzz'), 'get reverse rank of an element');

  tap_equal(1, $cluster->zrem($key1, 'Fizz'), 'remove first element from first set');

  tap_equal(3, $cluster->del([$key1, $key2, $dkey]), 'delete working sets keys');
  tap_equal(5, $cluster->zadd($key1, 0, 'Apple', 1, 'Banana', 3, 'Cabbage', 3, 'Carrot', 3, 'Corn'), 'add elements to new set');

  tap_equal(2, $cluster->zremrangebyrank($key1, 0, 1), 'remove elements by rank');
  tap_equal(2, $cluster->zremrangebylex($key1, '[Ca', '[Cb'), 'remove elements by lexographical value');
  tap_equal(1, $cluster->zremrangebyscore($key1, 3, 3), 'remove final element by score');

  tap_equal(0, $cluster->exists([$key1, $key2, $dkey]), 'ensure all keys are cleaned up');

  $cluster->close();
}

test_zset_cluster();
