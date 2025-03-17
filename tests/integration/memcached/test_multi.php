<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report metrics for multi-key Memcached operations: get, set,
and delete.

NOTE: deleteMulti and fetchAll are not instrumented by the agent.
*/

/*SKIPIF
<?php require('skipif.inc'); ?>
*/

/*INI
*/

/*EXPECT
ok - setMulti
ok - assertEqual
ok - getDelayed
ok - fetchAll
ok - getDelayed
ok - fetchAll
ok - getDelayed
ok - fetchAll
ok - deleteMulti
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
    [{"name":"Datastore/instance/Memcached/ENV[MEMCACHE_HOST]/11211"},[1, "??", "??", "??", "??", "??"]],
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
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},           [1, "??", "??", "??", "??", "??"]]
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

function getMulti($memcached, $expected)
{
  $actual = $memcached->getMulti(array_keys($expected));
  tap_equal_unordered($expected, $actual, __FUNCTION__);
}

function getDelayed($memcached, $items)
{
  $keys = array_keys($items);
  $expected = array();
  foreach ($items as $k => $v) {
    $expected[] = array('key' => $k, 'value' => $v);
  }

  /* 1-arg form */
  $memcached->getDelayed($keys);
  $actual = $memcached->fetchAll();
  tap_equal_unordered($expected, $actual, __FUNCTION__);

  /* 2-arg form */
  $memcached->getDelayed($keys, false);
  $actual = $memcached->fetchAll();
  tap_equal_unordered($expected, $actual, __FUNCTION__.' (2)');

  /* 3-arg form */
  $memcached->getDelayed($keys, false, NULL);
  $actual = $memcached->fetchAll();
  tap_equal_unordered($expected, $actual, __FUNCTION__.' (3)');
}

function deleteMulti($memcached, $items)
{
  $expected = array_fill_keys(array_keys($items), true);
  $actual = $memcached->deleteMulti(array_keys($items));
  tap_equal_unordered($expected, $actual, 'deleteMulti');
}

function main()
{
  global $MEMCACHE_HOST, $MEMCACHE_PORT;

  $memcached = new Memcached();
  $memcached->addServer($MEMCACHE_HOST, $MEMCACHE_PORT);

  $items = array(
    randstr(KEYLEN) => 'value0',
    randstr(KEYLEN) => 'value1',
    randstr(KEYLEN) => 'value2'
  );
  $keys = array_keys($items);

  $test = new TestCase($memcached);
  $test->setMulti($items);
  $test->assertEqual($items, $keys);

  /*
   * First, fetchAll returns an array of arrays.
   * Second, we invoke $memcached->fetchAll() rather than $test->fetchAll().
   * The latter would work fine, except it would print some TAP output.
   * That output would be redundant with the comparison we perform below,
   * as well as uninformative, because it would be a check for truthiness
   * rather than the exact comparison we want.
   */

  $expectedFetchAll = array();
  foreach ($items as $k => $v) {
    $expectedFetchAll[] = array('key' => $k, 'value' => $v);
  }

  /* 1-arg form */
  $test->getDelayed($keys);
  tap_equal_unordered($expectedFetchAll, $memcached->fetchAll(), 'fetchAll');

  /* 2-arg form */
  $test->getDelayed($keys, false);
  tap_equal_unordered($expectedFetchAll, $memcached->fetchAll(), 'fetchAll');

  /* 3-arg form */
  $test->getDelayed($keys, false, NULL);
  tap_equal_unordered($expectedFetchAll, $memcached->fetchAll(), 'fetchAll');

  $test->deleteMulti($keys);
  $memcached->quit();
}

main();
