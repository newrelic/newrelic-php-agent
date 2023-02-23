<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report metrics for basic Memcached operations: get, set, add,
increment, decrement, replace, and delete.
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
ok - add
ok - set
ok - replace
ok - assertEqual
ok - assertEqual
ok - delete
ok - delete
ok - delete
ok - add
ok - increment
ok - increment
ok - decrement
ok - decrement
ok - assertEqual
ok - delete
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [15, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [15, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Memcached/all"},                              [15, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Memcached/allOther"},                         [15, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/add"},                    [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/add",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/decr"},                   [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/decr",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/delete"},                 [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/delete",
      "scope":"OtherTransaction/php__FILE__"},                        [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get"},                    [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get",
      "scope":"OtherTransaction/php__FILE__"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/incr"},                   [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/incr",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/replace"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/replace",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/set"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/set",
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
require_once(realpath (dirname ( __FILE__ )) . '/memcache.inc');

define('KEYLEN', 8);   // should be long enough to prevent collisions.

function main()
{
  global $MEMCACHE_HOST, $MEMCACHE_PORT;

  $memcached = new Memcached();
  $memcached->addServer($MEMCACHE_HOST, $MEMCACHE_PORT);

  $key1 = randstr(KEYLEN);
  $key2 = randstr(KEYLEN);

  // basic operations
  $test = new TestCase($memcached);
  $test->add($key1, 'foo');
  $test->set($key2, 'bar');
  $test->replace($key2, 'baz');
  $test->assertEqual('foo', $key1);
  $test->assertEqual('baz', $key2);
  $test->delete($key1);
  $test->delete($key2);
  $test->refuteDelete($key2);

  $key3 = randstr(KEYLEN);

  // increment/decrement
  $test = new TestCase($memcached);
  $test->add($key3, 0);
  $test->increment($key3);
  $test->increment($key3, 2);
  $test->decrement($key3);
  $test->decrement($key3, 2);
  $test->assertEqual(0, $key3);
  $test->delete($key3);

  $memcached->quit();
}

main();
