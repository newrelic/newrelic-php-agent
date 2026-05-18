<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore/Redis metrics for \RedisCluster stream
operations, mirroring the coverage of tests/integration/redis/test_stream.redis5.php.
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
ok - result is a valid stream ID
ok - result is a valid stream ID
ok - result is a valid stream ID
ok - result is a valid stream ID
ok - result is a valid stream ID
ok - we can delete a stream entry by id
ok - first stream returns three entries
ok - second stream returns one entry
ok - first stream length is three
ok - trim first stream to two entries
ok - verify stream was trimmed
ok - second stream length is one
ok - get first stream info
ok - create a consumer group
ok - xreadgroup returns entries
ok - first stream is now empty
ok - proper xread[group] response with 2 elements
ok - we can acknowledge receipt of an ID
ok - proper xread[group] response with 1 elements
ok - our second ID is still pending
ok - we were able to claim the message
ok - proper xread[group] response with 2 elements
ok - delete our test keys
*/

/*EXPECT_METRICS_EXIST
Datastore/all
Datastore/allOther
Datastore/Redis/all
Datastore/Redis/allOther
Datastore/operation/Redis/xadd
Datastore/operation/Redis/xdel
Datastore/operation/Redis/xrange
Datastore/operation/Redis/xrevrange
Datastore/operation/Redis/xlen
Datastore/operation/Redis/xtrim
Datastore/operation/Redis/xinfo
Datastore/operation/Redis/xgroup
Datastore/operation/Redis/xreadgroup
Datastore/operation/Redis/xack
Datastore/operation/Redis/xpending
Datastore/operation/Redis/xclaim
Datastore/operation/Redis/xread
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

/* Confirm a valid XPENDING response with N elements */
function tap_xread_response($res, $stream, $n) {
  tap_assert(is_array($res) && isset($res[$stream]) && count($res[$stream]) == $n,
             "proper xread[group] response with $n elements");
}

function tap_valid_stream_id($id) {
  tap_matches('/[0-9]+\-[0-9]+/', $id, 'result is a valid stream ID');
}

function test_stream_cluster() {
  global $REDIS_CLUSTER_HOST, $REDIS_CLUSTER_PORT;

  $cluster = new RedisCluster(null,
    [$REDIS_CLUSTER_HOST . ':' . $REDIS_CLUSTER_PORT],
    1.5, 1.5, true);

  /*
   * Both keys share a hashtag so multi-key operations
   * route to the same slot, which is required in cluster mode.
   */
  $tag  = '{rc_stream}';
  $suffix = randstr(16);
  $key1 = $tag . '_a_' . $suffix;
  $key2 = $tag . '_b_' . $suffix;

  if ($cluster->exists([$key1, $key2])) {
    die("skip: key(s) already exist: $key1, $key2\n");
  }

  tap_valid_stream_id($cluster->xadd($key1, '*', ['foo' => 'bar']));
  tap_valid_stream_id($cluster->xadd($key1, '*', ['baz' => 'bop']));
  tap_valid_stream_id($cluster->xadd($key1, '*', ['for' => 'fen']));
  tap_valid_stream_id($cluster->xadd($key2, '*', ['new' => 'one']));

  /* Store this ID specifically so we can delete it with xdel */
  $id = $cluster->xadd($key2, '*', ['final' => 'value']);
  tap_valid_stream_id($id);
  tap_equal(1, $cluster->xdel($key2, [$id]), 'we can delete a stream entry by id');

  tap_equal(3, count($cluster->xrange($key1, 0, '+')), 'first stream returns three entries');
  tap_equal(1, count($cluster->xrevrange($key2, '+', 0)), 'second stream returns one entry');

  tap_equal(3, $cluster->xlen($key1), 'first stream length is three');
  tap_equal(1, $cluster->xtrim($key1, 2), 'trim first stream to two entries');
  tap_equal(2, $cluster->xlen($key1), 'verify stream was trimmed');
  tap_equal(1, $cluster->xlen($key2), 'second stream length is one');

  $info = $cluster->xinfo('stream', $key1);
  tap_assert(is_array($info) && (isset($info['length']) || in_array('length', $info)),
             'get first stream info');

  tap_assert($cluster->xgroup('CREATE', $key1, 'group1', '0'), 'create a consumer group');

  $res = $cluster->xreadgroup('group1', 'consumer1', [$key1 => '>'], 2);
  tap_assert(is_array($res) && $res, 'xreadgroup returns entries');
  $res = $cluster->xreadgroup('group1', 'consumer1', [$key1 => '>'], 1);
  tap_assert(is_array($res) && !$res, 'first stream is now empty');

  /* Execute XREADGROUP and capture results so we can test XACK and XPENDING */
  $res = $cluster->xreadgroup('group1', 'consumer1', [$key1 => '0']);
  tap_xread_response($res, $key1, 2);
  list($id1, $id2) = array_keys($res[$key1]);

  /* XACK a message and verify the other is PENDING */
  tap_equal(1, $cluster->xack($key1, 'group1', [$id1]), 'we can acknowledge receipt of an ID');
  tap_xread_response($cluster->xreadgroup('group1', 'consumer1', [$key1 => '0']), $key1, 1);
  $res = $cluster->xpending($key1, 'group1');
  tap_assert(is_array($res) && in_array($id2, $res), 'our second ID is still pending');

  /* There is one pending message. Have another consumer claim it */
  $res = $cluster->xclaim($key1, 'group1', 'newconsumer', 0, [$id2]);
  tap_assert(is_array($res) && isset($res[$id2]), 'we were able to claim the message');

  /* Finally perform a normal XREAD */
  tap_xread_response($cluster->xread([$key1 => 0]), $key1, 2);

  tap_equal(2, $cluster->del([$key1, $key2]), 'delete our test keys');

  $cluster->close();
}

test_stream_cluster();
