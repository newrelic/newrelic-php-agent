<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore/Redis metrics for \RedisCluster set operations,
mirroring the coverage of tests/integration/redis/test_set.php.
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
ok - add new element to set
ok - add existing element to set
ok - add two elements to second set
ok - get second set cardinality
ok - get difference between two sets
ok - store difference of two sets
ok - read stored set difference
ok - get set intersection
ok - store intersection of two sets
ok - read stored set intersection
ok - get set union
ok - store set union
ok - read stored set union
ok - check set membership
ok - move an element from one set to another
ok - move a nonexistent element from one set to another
ok - get a random set element
ok - pop an element off a set
ok - remove existing member from a set
ok - remove nonexistent member from a set
ok - delete first key
ok - verify second key already deleted
ok - remove destination key
*/

/*EXPECT_METRICS_EXIST
Datastore/all
Datastore/allOther
Datastore/Redis/all
Datastore/Redis/allOther
Datastore/operation/Redis/sadd
Datastore/operation/Redis/scard
Datastore/operation/Redis/sdiff
Datastore/operation/Redis/sdiffstore
Datastore/operation/Redis/sinter
Datastore/operation/Redis/sinterstore
Datastore/operation/Redis/sunion
Datastore/operation/Redis/sunionstore
Datastore/operation/Redis/sismember
Datastore/operation/Redis/smembers
Datastore/operation/Redis/smove
Datastore/operation/Redis/srandmember
Datastore/operation/Redis/spop
Datastore/operation/Redis/srem
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

function test_set_cluster() {
  global $REDIS_CLUSTER_HOST, $REDIS_CLUSTER_PORT;

  $cluster = new RedisCluster(null,
    [$REDIS_CLUSTER_HOST . ':' . $REDIS_CLUSTER_PORT],
    1.5, 1.5, true);

  /*
   * All three keys share a hashtag so multi-key operations
   * route to the same slot, which is required in cluster mode.
   */
  $tag  = '{rc_set}';
  $suffix = randstr(16);
  $key1 = $tag . '_a_' . $suffix;
  $key2 = $tag . '_b_' . $suffix;
  $dkey = $tag . '_d_' . $suffix;

  if ($cluster->exists([$key1, $key2, $dkey])) {
    die("skip: key(s) already exist: $key1, $key2, $dkey\n");
  }

  tap_equal(1, $cluster->sadd($key1, 'foo'), 'add new element to set');
  tap_equal(0, $cluster->sadd($key1, 'foo'), 'add existing element to set');
  tap_equal(2, $cluster->sadd($key2, 'foo', 'bar'), 'add two elements to second set');
  tap_equal(2, $cluster->scard($key2), 'get second set cardinality');

  tap_equal(['bar'], $cluster->sdiff([$key2, $key1]), 'get difference between two sets');
  tap_equal(1, $cluster->sdiffstore($dkey, $key2, $key1), 'store difference of two sets');
  tap_equal(['bar'], $cluster->smembers($dkey), 'read stored set difference');

  tap_equal(['foo'], $cluster->sinter([$key1, $key2]), 'get set intersection');
  tap_equal(1, $cluster->sinterstore($dkey, $key1, $key2), 'store intersection of two sets');
  tap_equal(['foo'], $cluster->smembers($dkey), 'read stored set intersection');

  tap_equal_unordered_values(['foo', 'bar'], $cluster->sunion([$key1, $key2]), 'get set union');
  tap_equal(2, $cluster->sunionstore($dkey, $key1, $key2), 'store set union');
  tap_equal_unordered_values(['foo', 'bar'], $cluster->smembers($dkey), 'read stored set union');

  tap_assert($cluster->sismember($key1, 'foo'), 'check set membership');

  tap_assert($cluster->smove($key2, $key1, 'bar'), 'move an element from one set to another');
  tap_refute($cluster->smove($key2, $key1, 'bar'), 'move a nonexistent element from one set to another');
  tap_assert(in_array($cluster->srandmember($key1), ['foo', 'bar']), 'get a random set element');
  tap_assert(in_array($cluster->spop($key1), ['foo', 'bar']), 'pop an element off a set');

  tap_equal(1, $cluster->srem($key2, 'foo'), 'remove existing member from a set');
  tap_equal(0, $cluster->srem($key2, 'foo'), 'remove nonexistent member from a set');

  tap_equal(1, $cluster->del($key1), 'delete first key');
  tap_equal(0, $cluster->del($key1), 'verify second key already deleted');
  tap_equal(1, $cluster->del($dkey), 'remove destination key');

  $cluster->close();
}

test_set_cluster();
