<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics including instance information for Redis
list operations.
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
ok - pop maximum element off second sorted set
ok - pop minimum element off of first sorted set
ok - remove first element from first set
ok - delete working sets keys
ok - add elements to new set
ok - remove elements by rank
ok - remove elements by lexographical value
ok - remove final element by score
ok - ensure all keys are cleaned up
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

  $key1 = randstr(16);
  $key2 = randstr(16);
  $dkey = randstr(16);
  if ($redis->exists([$key1, $key2]) > 0) {
    echo "One or more test keys exist: (${key1}, ${key2}, {$dkey})\n";
    exit(1);
  }

  tap_equal(2, $redis->zadd($key1, 3, 'Fizz', 5, 'Buzz'), 'add elements to first sorted set');
  tap_equal(2, $redis->zadd($key2, 3, 'Fizz', 15, 'FizzBuzz'), 'add element to second sorted set');
  tap_equal(2, $redis->zcard($key1), 'get cardinality of first sorted set');
  tap_equal(1, $redis->zcount($key1, 0, 3), 'count elements in first sorted set by score');
  tap_equal(30.0, $redis->zincrby($key2, 15, 'FizzBuzz'), 'increment element in second sorted set');

  tap_equal(1, $redis->zinterstore($dkey, [$key1, $key2]), 'compute intersection of two sorted sets');
  tap_equal(['Fizz' => 6.0], $redis->zrange($dkey, 0, -1, true), 'list intersection keys w/scores');

  tap_equal(3, $redis->zunionstore($dkey, [$key1, $key2]), 'compute union of two sorted sets');
  tap_equal(['FizzBuzz' => 30.0, 'Fizz' => 6.0, 'Buzz' => 5.0], $redis->zrevrange($dkey, 0, -1, true), 'list union keys w/scores');

  tap_equal(2, $redis->zlexcount($key2, '[Fizz', '[FizzZ'), 'count elements in lexographical range');
  tap_equal(['Fizz', 'FizzBuzz'], $redis->zrangebylex($key2, '[Fizz', '[FizzZ'), 'list elements in lexographical range');
  tap_equal(['FizzBuzz', 'Fizz'], $redis->zrevrangebylex($key2, '[FizzZ', '[Fizz'), 'list elements in reverse lexographical range');
  tap_equal(['Fizz', 'FizzBuzz'], $redis->zrangebyscore($key2, '-inf', '+inf'), 'list elements by score');
  tap_equal(['FizzBuzz', 'Fizz'], $redis->zrevrangebyscore($key2, '+inf', '-inf'), 'list elements by reverse score');

  tap_equal(1, $redis->zrank($key2, 'FizzBuzz'), 'get rank of an element');
  tap_equal(30.0, $redis->zscore($key2, 'FizzBuzz'), 'get the score of an element');
  tap_equal(0, $redis->zrevrank($key2, 'FizzBuzz'), 'get reverse rank of an element');

  tap_equal(['FizzBuzz' => 30.0],  $redis->zpopmax($key2), 'pop maximum element off second sorted set');
  tap_equal(['Fizz' => 3.0], $redis->zpopmin($key1), 'pop minimum element off of first sorted set');

  tap_equal(1, $redis->zrem($key1, 'Buzz'), 'remove first element from first set');

  tap_equal(2, $redis->del([$key1, $key2, $dkey]), 'delete working sets keys');
  tap_equal(5, $redis->zadd($key1, 0, 'Apple', 1, 'Banana', 3, 'Cabbage', 3, 'Carrot', 3, 'Corn'), 'add elements to new set');

  tap_equal(2, $redis->zremrangebyrank($key1, 0, 1), 'remove elements by rank');
  tap_equal(2, $redis->zremrangebylex($key1, '[Ca', '[Cb'), 'remove elements by lexographical value');
  tap_equal(1, $redis->zremrangebyscore($key1, 3, 3), 'remove final element by score');

  tap_equal(0, $redis->exists([$key1, $key2, $dkey]), 'ensure all keys are cleaned up');

  $redis->close();
}

test_redis();

$txn = new Transaction;

redis_trace_nodes_match($txn, array(
  'Datastore/operation/Redis/close',
  'Datastore/operation/Redis/connect',
  'Datastore/operation/Redis/del',
  'Datastore/operation/Redis/exists',
  'Datastore/operation/Redis/zadd',
  'Datastore/operation/Redis/zcard',
  'Datastore/operation/Redis/zcount',
  'Datastore/operation/Redis/zincrby',
  'Datastore/operation/Redis/zinterstore',
  'Datastore/operation/Redis/zlexcount',
  'Datastore/operation/Redis/zpopmax',
  'Datastore/operation/Redis/zpopmin',
  'Datastore/operation/Redis/zrange',
  'Datastore/operation/Redis/zrangebylex',
  'Datastore/operation/Redis/zrangebyscore',
  'Datastore/operation/Redis/zrank',
  'Datastore/operation/Redis/zrem',
  'Datastore/operation/Redis/zremrangebylex',
  'Datastore/operation/Redis/zremrangebyrank',
  'Datastore/operation/Redis/zremrangebyscore',
  'Datastore/operation/Redis/zrevrange',
  'Datastore/operation/Redis/zrevrangebylex',
  'Datastore/operation/Redis/zrevrangebyscore',
  'Datastore/operation/Redis/zrevrank',
  'Datastore/operation/Redis/zscore',
  'Datastore/operation/Redis/zunionstore',
));

redis_datastore_instance_metric_exists($txn);
