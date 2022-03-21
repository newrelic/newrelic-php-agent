<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report metrics when a Memcache key is incremented or
decremented using the Memcache procedural api.
*/

/*SKIPIF
<?php require('skipif.inc');
*/

/*EXPECT
ok - connect to server
ok - set key
ok - increment key
ok - decrement key
ok - check value
ok - delete key
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                          [6 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"Datastore/allOther"},                     [6 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"Datastore/Memcached/all"},                [6 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"Datastore/Memcached/allOther"},           [6 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"Datastore/operation/Memcached/connect"},  [1 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"Datastore/operation/Memcached/connect",
      "scope":"OtherTransaction/php__FILE__"},          [1 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"Datastore/operation/Memcached/decr"},     [1 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"Datastore/operation/Memcached/decr",
      "scope":"OtherTransaction/php__FILE__"},          [1 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"Datastore/operation/Memcached/delete"},   [1 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"Datastore/operation/Memcached/delete",
      "scope":"OtherTransaction/php__FILE__"},          [1 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"Datastore/operation/Memcached/get"},      [1 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"Datastore/operation/Memcached/get",
      "scope":"OtherTransaction/php__FILE__"},          [1 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"Datastore/operation/Memcached/incr"},     [1 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"Datastore/operation/Memcached/incr",
      "scope":"OtherTransaction/php__FILE__"},          [1 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"Datastore/operation/Memcached/set"},      [1 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"Datastore/operation/Memcached/set",
      "scope":"OtherTransaction/php__FILE__"},          [1 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"OtherTransaction/all"},                   [1 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"OtherTransactionTotalTime"},              [1 ,"??" ,"??" ,"??" ,"??" ,"??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1 ,"??" ,"??" ,"??" ,"??" ,"??"]]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

define('KEYLEN', 8);   // should be long enough to prevent collisions

function test_memcache() {
  global $MEMCACHE_HOST, $MEMCACHE_PORT;

  $memcache = memcache_connect($MEMCACHE_HOST, $MEMCACHE_PORT);
  tap_not_equal(false, $memcache, 'connect to server');

  $key = randstr(KEYLEN);

  tap_assert(memcache_set($memcache, $key, 1), 'set key');
  tap_equal(6, memcache_increment($memcache, $key, 5), 'increment key');
  tap_equal(5, memcache_decrement($memcache, $key, 1), 'decrement key');

  /*
   * Prior to Memcache 3.0.3, Memcache::get() always returned a string
   * for scalar values. After, Memcache::get() tries to return a value
   * of the same type as given to Memcache::set().
   *
   * See http://pecl.php.net/package-info.php?package=memcache&version=3.0.3
   */
  if (version_compare(phpversion('memcache'), '3.0.3', '>=')) {
    tap_equal(5, $memcache->get($key), 'check value');
  } else {
    tap_equal('5', $memcache->get($key), 'check value');
  }

  tap_assert(memcache_delete($memcache, $key), 'delete key');

  memcache_close($memcache);
}

test_memcache();
