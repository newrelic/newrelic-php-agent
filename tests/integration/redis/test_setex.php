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
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
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
ok - set timestamp expiry in milliseconds
ok - verify milliscond expiry set
ok - set an expiry in milliseconds
ok - verify millisecond expiry
ok - delete key
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name": "Supportability/Logging/Forwarding/PHP/disabled"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                         [19, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                    [19, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                   [19, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},              [19, "??", "??", "??", "??", "??"]],
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
    [{"name":"Datastore/operation/Redis/pexpire"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/pexpire",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/pexpireat"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/pexpireat",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/psetex"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/psetex",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/pttl"},        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/pttl",
      "scope":"OtherTransaction/php__FILE__"},         [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/setex"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/setex",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/ttl"},         [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/ttl",
      "scope":"OtherTransaction/php__FILE__"},         [4, "??", "??", "??", "??", "??"]],
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
    die("skip: key already exists: ${key}\n");
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

  tap_assert($redis->pexpireat($key, (time() + 60) * 1000), 'set timestamp expiry in milliseconds');
  tap_assert($redis->ttl($key) > 50, 'verify milliscond expiry set');

  tap_assert($redis->pexpire($key, 1500), 'set an expiry in milliseconds');
  tap_assert($redis->pttl($key) >= 1000, 'verify millisecond expiry');

  tap_equal(1, $redis->del($key), 'delete key');

  $redis->close();
}

test_redis();
