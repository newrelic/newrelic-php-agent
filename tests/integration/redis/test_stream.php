<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics for Redis basic operations.
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
    [{"name":"Datastore/all"},                        [24, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                   [24, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                  [24, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},             [24, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect",
      "scope":"OtherTransaction/php__FILE__"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists",
      "scope":"OtherTransaction/php__FILE__"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xack"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xack",
      "scope":"OtherTransaction/php__FILE__"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xadd"},       [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xadd",
      "scope":"OtherTransaction/php__FILE__"},        [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xclaim"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xclaim",
      "scope":"OtherTransaction/php__FILE__"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xdel"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xdel",
      "scope":"OtherTransaction/php__FILE__"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xgroup"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xgroup",
      "scope":"OtherTransaction/php__FILE__"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xinfo"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xinfo",
      "scope":"OtherTransaction/php__FILE__"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xlen"},       [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xlen",
      "scope":"OtherTransaction/php__FILE__"},        [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xpending"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xpending",
      "scope":"OtherTransaction/php__FILE__"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xrange"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xrange",
      "scope":"OtherTransaction/php__FILE__"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xread"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xread",
      "scope":"OtherTransaction/php__FILE__"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xreadgroup"}, [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xreadgroup",
      "scope":"OtherTransaction/php__FILE__"},        [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xrevrange"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xrevrange",
      "scope":"OtherTransaction/php__FILE__"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xtrim"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/xtrim",
      "scope":"OtherTransaction/php__FILE__"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
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

  $info = $redis->xinfo('stream', $key1);
  tap_assert(is_array($info) && isset($info['length']), 'get first stream info');

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

  /* close connection */
  $redis->close();
}

test_stream();
