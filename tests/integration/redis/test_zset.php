<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics for Redis sorted set operations.
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '5.4', '<')) {
    die("skip: PHP > 5.3 required\n");
}
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

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [26, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [26, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                                  [26, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},                             [26, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zadd"},                       [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zadd",
      "scope":"OtherTransaction/php__FILE__"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zcard"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zcard",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zcount"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zcount",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zincrby"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zincrby",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zinterstore"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zinterstore",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zlexcount"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zlexcount",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zrange"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zrange",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zrangebylex"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zrangebylex",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zrangebyscore"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zrangebyscore",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zrank"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zrank",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zrem"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zrem",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zremrangebylex"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zremrangebylex",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zremrangebyrank"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zremrangebyrank",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zremrangebyscore"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zremrangebyscore",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zrevrange"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zrevrange",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zrevrangebylex"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zrevrangebylex",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zrevrangebyscore"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zrevrangebyscore",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zrevrank"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zrevrank",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zscore"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zscore",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zunionstore"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zunionstore",
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




use NewRelic\Integration\Transaction;

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/integration.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/redis.inc');

function test_sorted_sets() {
  global $REDIS_HOST, $REDIS_PORT;

  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT);

  $key1 = randstr(16);
  $key2 = "{$key1}_b";
  $dkey = "{$key1}_d";
  if ($redis->exists([$key1, $key2, $dkey])) {
    die("skip: key(s) already exist: $key1, $key2, $dkey\n");
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

  tap_equal(1, $redis->zrem($key1, 'Fizz'), 'remove first element from first set');

  tap_equal(3, $redis->del([$key1, $key2, $dkey]), 'delete working sets keys');
  tap_equal(5, $redis->zadd($key1, 0, 'Apple', 1, 'Banana', 3, 'Cabbage', 3, 'Carrot', 3, 'Corn'), 'add elements to new set');

  tap_equal(2, $redis->zremrangebyrank($key1, 0, 1), 'remove elements by rank');
  tap_equal(2, $redis->zremrangebylex($key1, '[Ca', '[Cb'), 'remove elements by lexographical value');
  tap_equal(1, $redis->zremrangebyscore($key1, 3, 3), 'remove final element by score');

  tap_equal(0, $redis->exists([$key1, $key2, $dkey]), 'ensure all keys are cleaned up');

  $redis->close();
}

test_sorted_sets();
