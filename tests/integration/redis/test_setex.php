<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics for Redis increment operations.
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT
ok - set key to expire in 1s
ok - retrieve key before expiry
ok - check key ttl
ok - retrieve expired key
ok - delete expired key
ok - set key to expire in 1500ms
ok - retreive key before expiry
ok - retreive key ttl in miliseconds
ok - remove key expiration
ok - verify expiry removal
ok - set timestamp expiry
ok - verify expiry set
ok - delete key
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
    [{"name":"Datastore/all"},                         [15, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                    [15, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                   [15, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},              [15, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del"},         [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del",
      "scope":"OtherTransaction/php__FILE__"},         [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/expireat"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/expireat",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/get"},         [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/get",
      "scope":"OtherTransaction/php__FILE__"},         [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/persist"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/persist",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/psetex"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/psetex",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/pttl"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/pttl",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/setex"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/setex",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/ttl"},         [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/ttl",
      "scope":"OtherTransaction/php__FILE__"},         [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"}, [1, "??", "??", "??", "??", "??"]]
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

  /* generate a unique key to use for this test run */
  $key = randstr(16);
  if ($redis->exists($key)) {
    echo "key already exists: ${key}\n";
    exit(1);
  }

  tap_assert($redis->setex($key, 1, 'bar'), 'set key to expire in 1s');
  tap_equal('bar', $redis->get($key), 'retrieve key before expiry');
  tap_assert($redis->ttl($key) == 1, 'check key ttl');
  sleep(2);
  tap_refute($redis->get($key), 'retrieve expired key');
  tap_equal(0, $redis->del($key), 'delete expired key');  // it is no longer there: auto expiration

  tap_assert($redis->psetex($key, 1500, 'pbar'), 'set key to expire in 1500ms');
  tap_equal('pbar', $redis->get($key), 'retreive key before expiry');

  $pttl = $redis->pttl($key);
  tap_assert($pttl >= 0 && $pttl <= 1500, 'retreive key ttl in miliseconds');

  tap_assert($redis->persist($key), 'remove key expiration');
  tap_equal(-1, $redis->ttl($key), 'verify expiry removal');

  tap_assert($redis->expireat($key, time() + 1), 'set timestamp expiry');
  tap_assert($redis->ttl($key) >= 0, 'verify expiry set');

  tap_equal(1, $redis->del($key), 'delete key');

  $redis->close();
}

test_redis();
