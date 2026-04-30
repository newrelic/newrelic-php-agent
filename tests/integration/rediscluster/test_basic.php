<?php
/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore/Redis metrics for \RedisCluster basic operations,
mirroring the coverage of tests/integration/redis/test_basic.php.

The key correctness assertion is that no Datastore/operation/Redis/connect (or
pconnect / select) metric is produced: \RedisCluster instances are constructed via
__construct(seeds[]), which the agent must NOT route through the connect-style
inner handlers. Each individual data operation is asserted to exist as
Datastore/operation/Redis/<op>, matching how lib_predis.c already reports Predis
cluster traffic (NR_DATASTORE_REDIS).
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
ok - set key
ok - get key
ok - get first character of key
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
ok - delete keys
ok - pfadd reports new elements
ok - pfadd reports no new elements
ok - pfcount reports correct cardinality
ok - we can pfmerge into a destination key
ok - our merged key has the correct count
ok - there are no listners to a random channel
ok - delete keys
*/

/*EXPECT_METRICS_EXIST
Datastore/all
Datastore/allOther
Datastore/Redis/all
Datastore/Redis/allOther
Datastore/operation/Redis/set
Datastore/operation/Redis/get
Datastore/operation/Redis/getrange
Datastore/operation/Redis/rawcommand
Datastore/operation/Redis/setrange
Datastore/operation/Redis/append
Datastore/operation/Redis/strlen
Datastore/operation/Redis/type
Datastore/operation/Redis/getset
Datastore/operation/Redis/del
Datastore/operation/Redis/mset
Datastore/operation/Redis/mget
Datastore/operation/Redis/msetnx
Datastore/operation/Redis/setnx
Datastore/operation/Redis/unlink
Datastore/operation/Redis/rename
Datastore/operation/Redis/renamenx
Datastore/operation/Redis/time
Datastore/operation/Redis/eval
Datastore/operation/Redis/evalsha
Datastore/operation/Redis/publish
Datastore/operation/Redis/pfadd
Datastore/operation/Redis/pfcount
Datastore/operation/Redis/pfmerge
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

function test_basic_cluster() {
  global $REDIS_CLUSTER_HOST, $REDIS_CLUSTER_PORT;

  /*
   * \RedisCluster::__construct(?string $name, ?array $seeds, ...). No connect()
   * call: every connection is established via the constructor, which the agent
   * must NOT treat as a connect event (asserted by EXPECT_METRICS_DONT_EXIST).
   */
  $cluster = new RedisCluster(null,
    [$REDIS_CLUSTER_HOST . ':' . $REDIS_CLUSTER_PORT],
    1.5, 1.5, true);

  /*
   * Every key shares the {rc_basic} hashtag so multi-key operations
   * (mset/mget/msetnx/del/rename/renamenx/pfmerge) all route to the same slot
   * — required for cluster mode to accept them.
   */
  $tag = '{rc_basic}';
  $suffix = randstr(16);
  $key1 = $tag . '_a_' . $suffix;
  $key2 = $tag . '_b_' . $suffix;

  tap_assert($cluster->set($key1, 'car'), 'set key');
  tap_equal('car', $cluster->get($key1), 'get key');
  tap_equal('c', $cluster->getrange($key1, 0, 0), 'get first character of key');

  tap_equal('car', $cluster->rawcommand($key1, 'get', $key1), 'GET via rawcommand returns key');

  tap_equal(3, $cluster->setrange($key1, 0, 'b'), 'set first character of key');
  tap_equal(strlen('barometric'), $cluster->append($key1, 'ometric'), 'append to key');
  tap_equal('barometric', $cluster->get($key1), 'get key');
  tap_equal(strlen('barometric'), $cluster->strlen($key1), 'ask redis for key length');

  /* \RedisCluster doesn't expose the REDIS_STRING type constants the way \Redis
     does; type() returns an int (1 == string per phpredis). */
  tap_equal(Redis::REDIS_STRING, $cluster->type($key1), 'ask redis for key type');

  tap_equal('barometric', $cluster->getset($key1, 'geometric'), 'execute getset command');
  tap_equal('geometric', $cluster->get($key1), 'verify getset updated key');

  tap_equal(1, $cluster->del($key1), 'delete key');
  tap_equal(0, $cluster->del($key1), 'delete missing key');

  tap_assert($cluster->mset([$key1 => 'bar']), 'mset key');
  tap_equal(['bar'], $cluster->mget([$key1]), 'mget key');

  /* \RedisCluster::msetnx returns an array of per-master results, not a bool.
     With single-slot {tag} keys all values go to one master → 1-element array. */
  tap_equal([0], $cluster->msetnx([$key1 => 'newbar']), 'msetnx existing key');
  tap_equal(1, $cluster->del($key1), 'delete key');
  tap_equal([1], $cluster->msetnx([$key1 => 'newbar']), 'msetnx non existing key');
  tap_equal(1, $cluster->unlink($key1), 'unlink key');

  tap_assert($cluster->setnx($key1, 'bar'), 'reuse deleted key');
  tap_refute($cluster->setnx($key1, 'bar'), 'set duplicate key');

  tap_assert($cluster->rename($key1, $key2), 'rename a key');
  tap_equal('bar', $cluster->get($key2), 'verify key was properly renamed');
  tap_assert($cluster->renamenx($key2, $key1), 'renamenx to a nonexistent key');
  tap_assert($cluster->set($key2, 'baz'), 'create a second key');
  tap_refute($cluster->renamenx($key1, $key2), 'renamenx to an existing key');

  /* time() is per-shard in cluster mode; route via the tag key. */
  $res = $cluster->time($key1);
  tap_assert(is_array($res) && count($res) == 2 && is_numeric($res[0]) && is_numeric($res[1]),
             'verify time command works');

  $lua = 'return 42';
  tap_equal(42, $cluster->eval($lua, [$key1], 1), 'we can EVAL a lua script');
  tap_equal(42, $cluster->evalsha(sha1($lua), [$key1], 1),
            'we can EVAL a lua script with an SHA hash');

  /* HyperLogLog operations — pfmerge requires same-slot keys; tag covers it.
     \RedisCluster::pfadd returns bool (not int) — the underlying RESP integer
     is converted by phpredis at the cluster client layer. */
  tap_equal(2, $cluster->del([$key1, $key2]), 'delete keys');
  tap_assert($cluster->pfadd($key1, ['one', 'two', 'three']), 'pfadd reports new elements');
  tap_refute($cluster->pfadd($key1, ['three']), 'pfadd reports no new elements');
  tap_equal(3, $cluster->pfcount($key1), 'pfcount reports correct cardinality');
  tap_assert($cluster->pfmerge($key2, [$key1]), 'we can pfmerge into a destination key');
  tap_equal(3, $cluster->pfcount($key2), 'our merged key has the correct count');

  /* Same publish-with-no-subscriber pattern as the \Redis baseline. The channel
     name routes the command in cluster mode, so use a tag-bound name. */
  tap_equal(0, $cluster->publish($tag . '_chan_' . $suffix, 'message'),
            'there are no listners to a random channel');

  /* cleanup */
  tap_equal(2, $cluster->del([$key1, $key2]), 'delete keys');

  /* close connection */
  $cluster->close();
}

test_basic_cluster();
