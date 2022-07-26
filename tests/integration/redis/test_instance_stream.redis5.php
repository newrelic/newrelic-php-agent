<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics including instance information for Redis
stream operations.
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '5.4', '<')) {
    die("skip: PHP > 5.3 required\n");
}
$minimum_redis_datastore_version='5.0.0';
require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 1
newrelic.datastore_tracer.instance_reporting.enabled = 1
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
ok - trace nodes match
ok - datastore instance metric exists
*/

use NewRelic\Integration\Transaction;

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/integration.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/redis.inc');

/* Confirm a valid XPENDING response with N elements */
function tap_xread_response($res, $stream, $n) {
    tap_assert(is_array($res) && isset($res[$stream]) && count($res[$stream]) == $n,
               "proper xread[group] response with $n elements");
}

function tap_valid_stream_id($id) {
  tap_matches('/[0-9]+\-[0-9]+/', $id, 'result is a valid stream ID');
}

function test_stream() {
  global $REDIS_HOST, $REDIS_PORT;

  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT);

  /* generate a unique key to use for this test run */
  $key1 = randstr(16);
  $key2 = randstr(16);
  if ($redis->exists([$key1, $key2])) {
    echo "key(s) already exist: ${key1}, ${key2}\n";
    exit(1);
  }

  tap_valid_stream_id($redis->xadd($key1, '*', ['foo' => 'bar']));
  tap_valid_stream_id($redis->xadd($key1, '*', ['baz' => 'bop']));
  tap_valid_stream_id($redis->xadd($key1, '*', ['for' => 'fen']));
  tap_valid_stream_id($redis->xadd($key2, '*', ['new' => 'one']));

  /* Store this ID specifically so we can delete it with xdel */
  $id = $redis->xadd($key2, '*', ['final' => 'value']);
  tap_valid_stream_id($id);
  tap_equal(1, $redis->xdel($key2, [$id]), 'we can delete a stream entry by id');

  tap_equal(3, count($redis->xrange($key1, 0, '+')), 'first stream returns three entries');
  tap_equal(1, count($redis->xrevrange($key2, '+', 0)), 'second stream returns one entry');

  tap_equal(3, $redis->xlen($key1), 'first stream length is three');
  tap_equal(1, $redis->xtrim($key1, 2), 'trim first stream to two entries');
  tap_equal(2, $redis->xlen($key1), 'verify stream was trimmed');
  tap_equal(1, $redis->xlen($key2), 'second stream length is one');

  /* PhpRedis >= 5.0.0 has a custom XINFO handler that returns nicer associative key => value pairs,
     while < 5.0.0 returns the Redis response raw, which is why we test if 'length' is a key OR is in
     the returned array */
  $info = $redis->xinfo('stream', $key1);
  tap_assert(is_array($info) && (isset($info['length']) || in_array('length', $info)),
             'get first stream info');

  tap_assert($redis->xgroup('CREATE', $key1, 'group1', '0'), 'create a consumer group');

  $res = $redis->xreadgroup('group1', 'consumer1', [$key1 => '>'], 2);
  tap_assert(is_array($res) && $res, 'xreadgroup returns entries');
  $res = $redis->xreadgroup('group1', 'consumer1', [$key1 => '>'], 1);
  tap_assert(is_array($res) && !$res, 'first stream is now empty');

  /* Execute XREADGROUP and capture results so we can test XACK and XPENDING */
  $res = $redis->xreadgroup('group1', 'consumer1', [$key1 => '0']);
  tap_xread_response($res, $key1, 2);
  list ($id1, $id2) = array_keys($res[$key1]);

  /* XACK a message and verify the other is PENDING */
  tap_equal(1, $redis->xack($key1, 'group1', [$id1]), 'we can acknowledge receipt of an ID');
  tap_xread_response($redis->xreadgroup('group1', 'consumer1', [$key1 => '0']), $key1, 1);
  $res = $redis->xpending($key1, 'group1');
  tap_assert(is_array($res) && in_array($id2, $res), 'our second ID is still pending');

  /* There is one pending message.  Have another consumer claim it */
  $res = $redis->xclaim($key1, 'group1', 'newconsumer', 0, [$id2]);
  tap_assert(is_array($res) && isset($res[$id2]), 'we were able to claim the message');

  /* Finally perform a normal XREAD */
  tap_xread_response($redis->xread([$key1 => 0]), $key1, 2);

  tap_equal(2, $redis->del([$key1, $key2]), 'delete our test keys');

  /* close connection */
  $redis->close();
}

test_stream();

$txn = new Transaction;

redis_trace_nodes_match($txn, array(
  'Datastore/operation/Redis/connect',
  'Datastore/operation/Redis/del',
  'Datastore/operation/Redis/exists',
  'Datastore/operation/Redis/xack',
  'Datastore/operation/Redis/xadd',
  'Datastore/operation/Redis/xclaim',
  'Datastore/operation/Redis/xdel',
  'Datastore/operation/Redis/xgroup',
  'Datastore/operation/Redis/xinfo',
  'Datastore/operation/Redis/xlen',
  'Datastore/operation/Redis/xpending',
  'Datastore/operation/Redis/xrange',
  'Datastore/operation/Redis/xread',
  'Datastore/operation/Redis/xreadgroup',
  'Datastore/operation/Redis/xrevrange',
  'Datastore/operation/Redis/xtrim',
));

redis_datastore_instance_metric_exists($txn);
