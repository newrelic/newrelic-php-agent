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
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
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
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [47, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [47, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                                  [47, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},                             [47, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/append"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/append",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/dbsize"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/dbsize",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del"},                        [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del",
      "scope":"OtherTransaction/php__FILE__"},                        [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/eval"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/eval",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/evalsha"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/evalsha",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exec"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exec",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/get"},                        [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/get",
      "scope":"OtherTransaction/php__FILE__"},                        [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/getrange"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/getrange",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/getset"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/getset",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/keys"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/keys",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/mget"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/mget",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/mset"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/mset",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/msetnx"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/msetnx",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/pfadd"},                      [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/pfadd",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/pfcount"},                    [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/pfcount",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/pfmerge"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/pfmerge",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/ping"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/ping",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/publish"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/publish",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/randomkey"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/randomkey",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/rawcommand"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/rawcommand",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/rename"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/rename",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/renamenx"},                   [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/renamenx",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/set"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/set",
      "scope":"OtherTransaction/php__FILE__"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/setnx"},                      [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/setnx",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/setrange"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/setrange",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/strlen"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/strlen",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/time"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/time",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/type"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/type",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/unlink"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/unlink",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/disabled"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/disabled"},         [1, "??", "??", "??", "??", "??"]]
  ]
]
*/



require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/redis.inc');

function test_basic() {
  global $REDIS_HOST, $REDIS_PORT;

  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT);

  /* generate a unique key to use for this test run */
  $key1 = randstr(16);
  $key2 = "{$key1}_b";
  if ($redis->exists([$key1, $key2])) {
    die("skip: key(s) already exist: $key1, $key2\n");
    exit(1);
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
