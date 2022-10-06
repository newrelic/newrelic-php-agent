<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report metrics for keyed Memcached operations: get, set, add,
increment, decrement, replace, and delete.

NOTE: incrementByKey and decrementByKey are not instrumented by the agent.
*/

/*SKIPIF
<?php require('skipif.inc'); ?>
*/

/*INI
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
*/

/*EXPECT
ok - addByKey
ok - setByKey
ok - replaceByKey
ok - assertEqualByKey
ok - assertEqualByKey
ok - deleteByKey
ok - deleteByKey
ok - deleteByKey
ok - addByKey
ok - incrementByKey
ok - incrementByKey
ok - decrementByKey
ok - decrementByKey
ok - assertEqualByKey
ok - deleteByKey
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
    [{"name":"Datastore/all"},                         [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                    [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Memcached/all"},               [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Memcached/allOther"},          [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/add"},     [ 2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/add",
      "scope":"OtherTransaction/php__FILE__"},         [ 2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/delete"},  [ 4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/delete",
      "scope":"OtherTransaction/php__FILE__"},         [ 4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get"},     [ 3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get",
      "scope":"OtherTransaction/php__FILE__"},         [ 3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/replace"}, [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/replace",
      "scope":"OtherTransaction/php__FILE__"},         [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/set"},     [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/set",
      "scope":"OtherTransaction/php__FILE__"},         [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                  [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},          [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},             [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"}, [ 1, "??", "??", "??", "??", "??"]]
  ]
]
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/memcache.inc');

define('KEYLEN', 8);   // should be long enough to prevent collisions.

function main()
{
  global $MEMCACHE_HOST, $MEMCACHE_PORT;

  $memcached = new Memcached();
  $memcached->addServer($MEMCACHE_HOST, $MEMCACHE_PORT);

  $server_key = 'server_key';
  $key1 = randstr(KEYLEN);
  $key2 = randstr(KEYLEN);

  /* Basic operations */
  $test = new TestCase($memcached);
  $test->addByKey($server_key, $key1, 'foo');
  $test->setByKey($server_key, $key2, 'bar');
  $test->replaceByKey($server_key, $key2, 'baz');
  $test->assertEqualByKey('foo', $server_key, $key1);
  $test->assertEqualByKey('baz', $server_key, $key2);
  $test->deleteByKey($server_key, $key1);
  $test->deleteByKey($server_key, $key2);
  $test->refuteDeleteByKey($server_key, $key2);

  $key3 = randstr(KEYLEN);

  /* Increment/decrement */
  $test = new TestCase($memcached);
  $test->addByKey($server_key, $key3, 0);
  $test->incrementByKey($server_key, $key3);
  $test->incrementByKey($server_key, $key3, 2);
  $test->decrementByKey($server_key, $key3);
  $test->decrementByKey($server_key, $key3, 2);
  $test->assertEqualByKey(0, $server_key, $key3);
  $test->deleteByKey($server_key, $key3);

  $memcached->quit();
}

main();
