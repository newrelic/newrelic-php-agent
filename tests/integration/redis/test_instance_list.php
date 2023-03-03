<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
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

$txn = new Transaction;

redis_trace_nodes_match($txn, [
  'Datastore/operation/Redis/connect',
  'Datastore/operation/Redis/del',
  'Datastore/operation/Redis/exists',
  'Datastore/operation/Redis/expire',
  'Datastore/operation/Redis/lget',
  'Datastore/operation/Redis/lindex',
  'Datastore/operation/Redis/linsert',
  'Datastore/operation/Redis/llen',
  'Datastore/operation/Redis/lpop',
  'Datastore/operation/Redis/lpush',
  'Datastore/operation/Redis/lpushx',
  'Datastore/operation/Redis/lrange',
  'Datastore/operation/Redis/lrem',
  'Datastore/operation/Redis/lremove',
  'Datastore/operation/Redis/lset',
  'Datastore/operation/Redis/ltrim',
  'Datastore/operation/Redis/rpop',
  'Datastore/operation/Redis/rpoplpush',
  'Datastore/operation/Redis/rpush',
  'Datastore/operation/Redis/rpushx',
]);

redis_datastore_instance_metric_exists($txn);
