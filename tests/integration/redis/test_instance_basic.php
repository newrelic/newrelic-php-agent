<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics including instance information for Redis
basic operations.
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
ok - ping redis
ok - set key
ok - get key
ok - get first character of key
ok - verify keys command returns known key
ok - ensure randomkey returns a key
ok - GET via rawcommand returns key
ok - set first character of key
ok - append to key
ok - get key
ok - ask redis for key length
ok - ask redis for key type
ok - execute getset command
ok - verify getset updated key
ok - delete key
ok - delete missing key
ok - mset key
ok - mget key
ok - msetnx existing key
ok - delete key
ok - msetnx non existing key
ok - unlink key
ok - reuse deleted key
ok - set duplicate key
ok - rename a key
ok - verify key was properly renamed
ok - renamenx to a nonexistent key
ok - create a second key
ok - renamenx to an existing key
ok - verify time command works
ok - we can EVAL a lua script
ok - we can EVAL a lua script with an SHA hash
ok - we can start a pipeline
ok - we can execute commands in a pipeline
ok - exec returns the correct response
ok - delete our temp keys
ok - pfadd reports new elements
ok - pfadd reports no new elements
ok - pfcount reports correct cardinality
ok - we can pfmerge into a destination key
ok - our merged key has the correct count
ok - there are no listners to a random channel
ok - verify a non-zero number of keys
ok - delete keys
ok - verify reduced redis db key count
ok - trace nodes match
ok - datastore instance metric exists
*/

use NewRelic\Integration\Transaction;

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/integration.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/redis.inc');

function test_basic() {
  global $REDIS_HOST, $REDIS_PORT;

  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT);

  /* generate a unique key to use for this test run */
  $key1 = randstr(16);
  $key2 = "${key1}_b";
  if ($redis->exists([$key1, $key2])) {
    die("skip: key(s) already exist: ${key1}, ${key2}\n");
  }

  tap_equal("+PONG", $redis->ping("+PONG"), 'ping redis');

  tap_assert($redis->set($key1, 'car'), 'set key');
  tap_equal('car', $redis->get($key1), 'get key');
  tap_equal('c', $redis->getrange($key1, 0, 0), 'get first character of key');

  tap_equal([$key1], $redis->keys($key1), 'verify keys command returns known key');
  tap_assert(is_string($redis->randomkey()), 'ensure randomkey returns a key');
  tap_equal('car', $redis->rawcommand('get', $key1), 'GET via rawcommand returns key');

  tap_equal(3, $redis->setrange($key1, 0, 'b'), 'set first character of key');
  tap_equal(strlen('barometric'), $redis->append($key1, 'ometric'), 'append to key');
  tap_equal('barometric', $redis->get($key1), 'get key');
  tap_equal(strlen('barometric'), $redis->strlen($key1), 'ask redis for key length');

  tap_equal(Redis::REDIS_STRING, $redis->type($key1), 'ask redis for key type');

  tap_equal('barometric', $redis->getset($key1, 'geometric'), 'execute getset command');
  tap_equal('geometric', $redis->get($key1), 'verify getset updated key');

  tap_equal(1, $redis->del($key1), 'delete key');
  tap_equal(0, $redis->del($key1), 'delete missing key');

  tap_assert($redis->mset([$key1 => 'bar']), 'mset key');
  tap_equal(['bar'], $redis->mget([$key1]), 'mget key');

  tap_refute($redis->msetnx([$key1 => 'newbar']), 'msetnx existing key');
  tap_equal(1, $redis->del($key1), 'delete key');
  tap_assert($redis->msetnx([$key1 => 'newbar']), 'msetnx non existing key');
  tap_equal(1, $redis->unlink($key1), 'unlink key');

  tap_assert($redis->setnx($key1, 'bar'), 'reuse deleted key');
  tap_refute($redis->setnx($key1, 'bar'), 'set duplicate key');

  tap_assert($redis->rename($key1, $key2), 'rename a key');
  tap_equal('bar', $redis->get($key2), 'verify key was properly renamed');
  tap_assert($redis->renamenx($key2, $key1), 'renamenx to a nonexistent key');
  tap_assert($redis->set($key2, 'baz'), 'create a second key');
  tap_refute($redis->renamenx($key1, $key2), 'renamenx to an existing key');

  $res = $redis->time();
  tap_assert(is_array($res) && count($res) == 2 && is_numeric($res[0]) && is_numeric($res[1]),
             'verify time command works');

  $lua = 'return 42';
  tap_equal(42, $redis->eval($lua), 'we can EVAL a lua script');
  tap_equal(42, $redis->evalsha(sha1($lua)), 'we can EVAL a lua script with an SHA hash');

  $msg = 'So Long, and Thanks for All the Fish';
  tap_assert(is_object($redis->pipeline()), 'we can start a pipeline');
  tap_assert(is_object($redis->set($key1, $msg)->get($key1)), 'we can execute commands in a pipeline');
  tap_equal([true, $msg], $redis->exec(), 'exec returns the correct response');

  tap_equal(2, $redis->del([$key1, $key2]), 'delete our temp keys');
  tap_equal(1, $redis->pfadd($key1, ['one', 'two', 'three']), 'pfadd reports new elements');
  tap_equal(0, $redis->pfadd($key1, ['three']), 'pfadd reports no new elements');
  tap_equal(3, $redis->pfcount($key1), 'pfcount reports correct cardinality');
  tap_assert($redis->pfmerge($key2, [$key1]), 'we can pfmerge into a destination key');
  tap_equal(3, $redis->pfcount($key2), 'our merged key has the correct count');

  /* There's no trivial way to do deep verification of PUBLISH without a subscribed client
     so simply issue the command and validate the response isn't an error */
  tap_equal(0, $redis->publish(uniqid(), 'message'), 'there are no listners to a random channel');

  tap_assert(($db_size = $redis->dbsize()) > 0, 'verify a non-zero number of keys');

  /* cleanup the key used by this test run */
  tap_equal(2, $redis->del([$key1, $key2]), 'delete keys');

  tap_equal($db_size - 2, $redis->dbsize(), 'verify reduced redis db key count');

  /* close connection */
  $redis->close();
}

test_basic();

$txn = new Transaction;

redis_trace_nodes_match($txn, array(
  'Datastore/operation/Redis/append',
  'Datastore/operation/Redis/close',
  'Datastore/operation/Redis/connect',
  'Datastore/operation/Redis/dbsize',
  'Datastore/operation/Redis/del',
  'Datastore/operation/Redis/eval',
  'Datastore/operation/Redis/evalsha',
  'Datastore/operation/Redis/exec',
  'Datastore/operation/Redis/exists',
  'Datastore/operation/Redis/get',
  'Datastore/operation/Redis/getrange',
  'Datastore/operation/Redis/getset',
  'Datastore/operation/Redis/keys',
  'Datastore/operation/Redis/mget',
  'Datastore/operation/Redis/mset',
  'Datastore/operation/Redis/msetnx',
  'Datastore/operation/Redis/pfadd',
  'Datastore/operation/Redis/pfcount',
  'Datastore/operation/Redis/pfmerge',
  'Datastore/operation/Redis/ping',
  'Datastore/operation/Redis/pipeline',
  'Datastore/operation/Redis/publish',
  'Datastore/operation/Redis/randomkey',
  'Datastore/operation/Redis/rawcommand',
  'Datastore/operation/Redis/rename',
  'Datastore/operation/Redis/renamenx',
  'Datastore/operation/Redis/select',
  'Datastore/operation/Redis/set',
  'Datastore/operation/Redis/setnx',
  'Datastore/operation/Redis/setrange',
  'Datastore/operation/Redis/strlen',
  'Datastore/operation/Redis/time',
  'Datastore/operation/Redis/type',
  'Datastore/operation/Redis/unlink',
));

redis_datastore_instance_metric_exists($txn);
