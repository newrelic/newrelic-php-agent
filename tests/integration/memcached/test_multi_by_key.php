<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report metrics for multi-key Memcached operations: get, set,
and delete.

NOTE: deleteMultiByKey and fetchAll are not instrumented by the agent.
*/

/*SKIPIF
<?php require('skipif.inc'); ?>
*/

/*INI
*/

/*EXPECT
ok - setMultiByKey
ok - assertEqualByKey
ok - getDelayedByKey
ok - fetchAll
ok - getDelayedByKey
ok - fetchAll
ok - getDelayedByKey
ok - fetchAll
ok - deleteMultiByKey
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Memcached/all"},                              [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Memcached/allOther"},                         [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/instance/Memcached/ENV[MEMCACHE_HOST]/11211"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get"},                    [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get",
      "scope":"OtherTransaction/php__FILE__"},                        [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/set"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/set",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]]
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
  $items = array(
    randstr(KEYLEN) => 'value0',
    randstr(KEYLEN) => 'value1',
    randstr(KEYLEN) => 'value2'
  );
  $keys = array_keys($items);

  $test = new TestCase($memcached);
  $test->setMultiByKey($server_key, $items);
  $test->assertEqualByKey($items, $server_key, $keys);

  // Tricky bit follows. First, fetchAll returns an array of arrays. Second, we
  // invoke $memcached->fetchAll() rather than $test->fetchAll(). The latter
  // would work fine, except it would print some TAP output. That output would
  // be redundant with the comparison we perform below, as well as uninformative
  // because it would be a check for truthiness rather than the exact comparison
  // we want.

  $expectedFetchAll = array();
  foreach ($items as $k => $v) {
    $expectedFetchAll[] = array('key' => $k, 'value' => $v);
  }

  // 2-arg form
  $test->getDelayedByKey($server_key, $keys);
  tap_equal_unordered($expectedFetchAll, $memcached->fetchAll(), 'fetchAll');

  // 3-arg form
  $test->getDelayedByKey($server_key, $keys, false);
  tap_equal_unordered($expectedFetchAll, $memcached->fetchAll(), 'fetchAll');

  // 4-arg form
  $test->getDelayedByKey($server_key, $keys, false, NULL);
  tap_equal_unordered($expectedFetchAll, $memcached->fetchAll(), 'fetchAll');

  $test->deleteMultiByKey($server_key, $keys);
  $memcached->quit();
}

main();
