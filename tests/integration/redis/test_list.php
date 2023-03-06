<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics for Redis list operations.
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
ok - append B
ok - append C
ok - prepend A
ok - retrieve element 0
ok - retrieve element 1
ok - retrieve element 2
ok - retrieve last element
ok - retrieve invalid element
ok - retrieve element 0
ok - retrieve element 1
ok - retrieve element 2
ok - linsert BEFORE A
ok - check list elements
ok - linsert AFTER B
ok - check list elements
ok - retreive list length
ok - pop the first element off list
ok - pop the last element off list
ok - lpushx to a list that exists
ok - rpushx to a list that exists
ok - pop from one list and push to another
ok - check the list we pushed to
ok - delete list with ltrim
ok - verify list is deleted
ok - add new elements
ok - remove first occurence of B
ok - verify list elements
ok - remove missing element
ok - replace list head
ok - list head was replaced
ok - delete list
ok - delete second list
ok - lpushx to a list that does not exist
ok - rpushx to a list that does not exist
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [38, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [38, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                                  [38, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},                             [38, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/expire"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/expire",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/lget"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/lget",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/lindex"},                     [8, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/lindex",
      "scope":"OtherTransaction/php__FILE__"},                        [8, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/linsert"},                    [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/linsert",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/llen"},                       [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/llen",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/lpop"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/lpop",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/lpush"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/lpush",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/lpushx"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/lpushx",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/lrange"},                     [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/lrange",
      "scope":"OtherTransaction/php__FILE__"},                        [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/lrem"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/lrem",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/lremove"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/lremove",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/lset"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/lset",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/ltrim"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/ltrim",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/rpop"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/rpop",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/rpoplpush"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/rpoplpush",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/rpush"},                      [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/rpush",
      "scope":"OtherTransaction/php__FILE__"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/rpushx"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/rpushx",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]]
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

  /* generate unique key names to use for this test run */
  $key = randstr(16);
  $key2 = "{$key}_b";
  if ($redis->exists([$key, $key2])) {
    die("skip: key(s) already exist: $key, $key2\n");
  }

  /* Ensure the keys don't persist (too much) longer than the test. */
  $redis->expire($key, 30 /* seconds */);
  $redis->expire($key2, 30 /* seconds */);

  tap_equal(1, $redis->rPush($key, 'B'), 'append B');
  tap_equal(2, $redis->rPush($key, 'C'), 'append C');
  tap_equal(3, $redis->lPush($key, 'A'), 'prepend A');

  /* Redis->lGet is deprecated, but use it once to verify it works */
  tap_equal('A', @$redis->lGet($key, 0), 'retrieve element 0');
  tap_equal('B', $redis->lIndex($key, 1), 'retrieve element 1');
  tap_equal('C', $redis->lIndex($key, 2), 'retrieve element 2');
  tap_equal('C', $redis->lIndex($key, -1), 'retrieve last element');
  tap_refute($redis->lIndex($key, 10), 'retrieve invalid element');

  tap_equal('A', $redis->lIndex($key, 0), 'retrieve element 0');
  tap_equal('B', $redis->lIndex($key, 1), 'retrieve element 1');
  tap_equal('C', $redis->lIndex($key, 2), 'retrieve element 2');

  tap_equal(4, $redis->lInsert($key, Redis::BEFORE, 'A', 'a'), 'linsert BEFORE A');
  tap_equal(['a', 'A', 'B', 'C'], $redis->lrange($key, 0, -1), 'check list elements');
  tap_equal(5, $redis->lInsert($key, Redis::AFTER, 'B', 'b'), 'linsert AFTER B');
  tap_equal(['a', 'A', 'B', 'b', 'C'], $redis->lrange($key, 0, -1), 'check list elements');
  tap_equal(5, $redis->llen($key), 'retreive list length');
  tap_equal('a', $redis->lpop($key), 'pop the first element off list');
  tap_equal('C', $redis->rpop($key), 'pop the last element off list');

  tap_equal(4, $redis->lpushx($key, 'HEAD'), 'lpushx to a list that exists');
  tap_equal(5, $redis->rpushx($key, 'TAIL'), 'rpushx to a list that exists');

  tap_equal('TAIL', $redis->rpoplpush($key, $key2), 'pop from one list and push to another');
  tap_equal(['TAIL'], $redis->lrange($key2, 0, -1), 'check the list we pushed to');

  tap_assert($redis->ltrim($key, 1, 0), 'delete list with ltrim');
  tap_equal(0, $redis->llen($key), 'verify list is deleted');

  tap_equal(4, $redis->rpush($key, 'A', 'B', 'B', 'C'), 'add new elements');

  /* Redis->lRemove is depreacted, but use it once to verity it works */
  tap_equal(1, @$redis->lRemove($key, 'B', 1), 'remove first occurence of B');
  tap_equal(['A', 'B', 'C'], $redis->lrange($key, 0, -1), 'verify list elements');
  tap_equal(0, $redis->lRem($key, 'NOT_IN_LIST', 1), 'remove missing element');

  tap_assert($redis->lSet($key, 0, 'AA'), 'replace list head');
  tap_equal('AA', $redis->lIndex($key, 0), 'list head was replaced');

  /* cleanup the key used by this test run */
  tap_equal(1, $redis->del($key), 'delete list');
  tap_equal(1, $redis->del($key2), 'delete second list');

  tap_equal(0, $redis->lpushx($key, 'no-op'), 'lpushx to a list that does not exist');
  tap_equal(0, $redis->rpushx($key, 'no-op'), 'rpushx to a list that does not exist');

  $redis->close();
}

test_redis();
