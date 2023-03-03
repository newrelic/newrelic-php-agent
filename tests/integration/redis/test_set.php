<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics for Redis set operations.
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

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [25, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [25, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                                  [25, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},                             [25, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del",
      "scope":"OtherTransaction/php__FILE__"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/sadd"},                       [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/sadd",
      "scope":"OtherTransaction/php__FILE__"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/scard"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/scard",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/sdiff"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/sdiff",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/sdiffstore"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/sdiffstore",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/sinter"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/sinter",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/sinterstore"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/sinterstore",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/sismember"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/sismember",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/smembers"},                   [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/smembers",
      "scope":"OtherTransaction/php__FILE__"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/smove"},                      [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/smove",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/spop"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/spop",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/srandmember"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/srandmember",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/srem"},                       [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/srem",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/sunion"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/sunion",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/sunionstore"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/sunionstore",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/



use NewRelic\Integration\Transaction;

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/integration.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/redis.inc');

function test_setops() {
  global $REDIS_HOST, $REDIS_PORT;

  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT);

  /* generate a unique key to use for this test run */
  $key1 = randstr(16);
  $key2 = "{$key1}_b";
  $dkey = "{$key1}_d";
  if ($redis->exists([$key1, $key2, $dkey])) {
    die("skip: key(s) already exist: $key1, $key2, $dkey\n");
  }

  tap_equal(1, $redis->sadd($key1, 'foo'), 'add new element to set');
  tap_equal(0, $redis->sadd($key1, 'foo'), 'add existing element to set');
  tap_equal(2, $redis->sadd($key2, 'foo', 'bar'), 'add two elements to second set');
  tap_equal(2, $redis->scard($key2), 'get second set cardinality');

  tap_equal(['bar'], $redis->sdiff([$key2, $key1]), 'get difference between two sets');
  tap_equal(1, $redis->sdiffstore($dkey, $key2, $key1), 'store difference of two sets');
  tap_equal(['bar'], $redis->smembers($dkey), 'read stored set difference');

  tap_equal(['foo'], $redis->sinter([$key1, $key2]), 'get set intersection');
  tap_equal(1, $redis->sinterstore($dkey, $key1, $key2), 'store intersection of two sets');
  tap_equal(['foo'], $redis->smembers($dkey), 'read stored set intersection');

  tap_equal_unordered_values(['foo', 'bar'], $redis->sunion([$key1, $key2]), 'get set union');
  tap_equal(2, $redis->sunionstore($dkey, $key1, $key2), 'store set union');
  tap_equal_unordered_values(['foo', 'bar'], $redis->smembers($dkey), 'read stored set union');

  tap_assert($redis->sismember($key1, 'foo'), 'check set membership');

  tap_assert($redis->smove($key2, $key1, 'bar'), 'move an element from one set to another');
  tap_refute($redis->smove($key2, $key1, 'bar'), 'move a nonexistent element from one set to another');
  tap_assert(in_array($redis->srandmember($key1), ['foo', 'bar']), 'get a random set element');
  tap_assert(in_array($redis->spop($key1), ['foo', 'bar']), 'pop an element off a set');

  tap_equal(1, $redis->srem($key2, 'foo'), 'remove existing member from a set');
  tap_equal(0, $redis->srem($key2, 'foo'), 'remove nonexistent member from a set');

  tap_equal(1, $redis->del($key1), 'delete first key');
  tap_equal(0, $redis->del($key1), 'verify second key already deleted');
  tap_equal(1, $redis->del($dkey), 'remove destination key');

  /* close connection */
  $redis->close();
}

test_setops();
