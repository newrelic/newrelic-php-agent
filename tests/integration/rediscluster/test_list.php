<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore/Redis metrics for \RedisCluster list operations,
mirroring the coverage of tests/integration/redis/test_list.php.
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

/*EXPECT_METRICS_EXIST
Datastore/all
Datastore/allOther
Datastore/Redis/all
Datastore/Redis/allOther
Datastore/operation/Redis/rpush
Datastore/operation/Redis/lpush
Datastore/operation/Redis/lindex
Datastore/operation/Redis/linsert
Datastore/operation/Redis/lrange
Datastore/operation/Redis/llen
Datastore/operation/Redis/lpop
Datastore/operation/Redis/rpop
Datastore/operation/Redis/lpushx
Datastore/operation/Redis/rpushx
Datastore/operation/Redis/rpoplpush
Datastore/operation/Redis/ltrim
Datastore/operation/Redis/lrem
Datastore/operation/Redis/lset
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

function test_list_cluster() {
  global $REDIS_CLUSTER_HOST, $REDIS_CLUSTER_PORT;

  $cluster = new RedisCluster(null,
    [$REDIS_CLUSTER_HOST . ':' . $REDIS_CLUSTER_PORT],
    1.5, 1.5, true);

  /*
   * Both keys share a hashtag so multi-key operations route to
   * the same slot, which is required in cluster mode.
   */
  $tag = '{rc_list}';
  $suffix = randstr(16);
  $key  = $tag . '_a_' . $suffix;
  $key2 = $tag . '_b_' . $suffix;

  if ($cluster->exists([$key, $key2])) {
    die("skip: key(s) already exist: $key, $key2\n");
  }

  /* Ensure the keys don't persist longer than the test. */
  $cluster->expire($key, 30 /* seconds */);
  $cluster->expire($key2, 30 /* seconds */);

  tap_equal(1, $cluster->rPush($key, 'B'), 'append B');
  tap_equal(2, $cluster->rPush($key, 'C'), 'append C');
  tap_equal(3, $cluster->lPush($key, 'A'), 'prepend A');

  tap_equal('A', $cluster->lIndex($key, 0), 'retrieve element 0');
  tap_equal('B', $cluster->lIndex($key, 1), 'retrieve element 1');
  tap_equal('C', $cluster->lIndex($key, 2), 'retrieve element 2');
  tap_equal('C', $cluster->lIndex($key, -1), 'retrieve last element');
  tap_refute($cluster->lIndex($key, 10), 'retrieve invalid element');

  tap_equal('A', $cluster->lIndex($key, 0), 'retrieve element 0');
  tap_equal('B', $cluster->lIndex($key, 1), 'retrieve element 1');
  tap_equal('C', $cluster->lIndex($key, 2), 'retrieve element 2');

  tap_equal(4, $cluster->lInsert($key, Redis::BEFORE, 'A', 'a'), 'linsert BEFORE A');
  tap_equal(['a', 'A', 'B', 'C'], $cluster->lrange($key, 0, -1), 'check list elements');
  tap_equal(5, $cluster->lInsert($key, Redis::AFTER, 'B', 'b'), 'linsert AFTER B');
  tap_equal(['a', 'A', 'B', 'b', 'C'], $cluster->lrange($key, 0, -1), 'check list elements');
  tap_equal(5, $cluster->llen($key), 'retreive list length');
  tap_equal('a', $cluster->lpop($key), 'pop the first element off list');
  tap_equal('C', $cluster->rpop($key), 'pop the last element off list');

  tap_equal(4, $cluster->lpushx($key, 'HEAD'), 'lpushx to a list that exists');
  tap_equal(5, $cluster->rpushx($key, 'TAIL'), 'rpushx to a list that exists');

  tap_equal('TAIL', $cluster->rpoplpush($key, $key2), 'pop from one list and push to another');
  tap_equal(['TAIL'], $cluster->lrange($key2, 0, -1), 'check the list we pushed to');

  tap_assert($cluster->ltrim($key, 1, 0), 'delete list with ltrim');
  tap_equal(0, $cluster->llen($key), 'verify list is deleted');

  tap_equal(4, $cluster->rpush($key, 'A', 'B', 'B', 'C'), 'add new elements');

  tap_equal(1, @$cluster->lRem($key, 'B', 1), 'remove first occurence of B');
  tap_equal(['A', 'B', 'C'], $cluster->lrange($key, 0, -1), 'verify list elements');
  tap_equal(0, $cluster->lRem($key, 'NOT_IN_LIST', 1), 'remove missing element');

  tap_assert($cluster->lSet($key, 0, 'AA'), 'replace list head');
  tap_equal('AA', $cluster->lIndex($key, 0), 'list head was replaced');

  /* cleanup the keys used by this test run */
  tap_equal(1, $cluster->del($key), 'delete list');
  tap_equal(1, $cluster->del($key2), 'delete second list');

  tap_equal(0, $cluster->lpushx($key, 'no-op'), 'lpushx to a list that does not exist');
  tap_equal(0, $cluster->rpushx($key, 'no-op'), 'rpushx to a list that does not exist');

  $cluster->close();
}

test_list_cluster();
