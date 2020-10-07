<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report metrics when memcache keys are replaced using the
Memcache procedural api.
*/

/*SKIPIF
<?php require('skipif.inc');
*/

/*EXPECT
ok - connect to server
ok - add key 1
ok - add key 2
ok - add key 3
ok - replace key 1 (2 args)
ok - replace key 2 (3 args)
ok - replace key 3 (4 args)
ok - check values
ok - delete key 1
ok - delete key 2
ok - delete key 3
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"Datastore/all"},                          [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                     [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Memcached/all"},                [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Memcached/allOther"},           [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/add"},      [ 3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/add",
      "scope":"OtherTransaction/php__FILE__"},          [ 3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/connect"},  [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/connect",
      "scope":"OtherTransaction/php__FILE__"},          [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/delete"},   [ 3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/delete",
      "scope":"OtherTransaction/php__FILE__"},          [ 3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get"},      [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get",
      "scope":"OtherTransaction/php__FILE__"},          [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/replace"},  [ 3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/replace",
      "scope":"OtherTransaction/php__FILE__"},          [ 3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [ 1, "??", "??", "??", "??", "??"]]
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

  $keys = array(randstr(KEYLEN), randstr(KEYLEN), randstr(KEYLEN));

  $before = array(
    $keys[0] => 'aaa (before)',
    $keys[1] => 'bbb (before)',
    $keys[2] => 'ccc (before)'
  );

  $after = array(
    $keys[0] => 'aaa (after)',
    $keys[1] => 'bbb (after)',
    $keys[2] => 'ccc (after)'
  );

  tap_assert(memcache_add($memcache, $keys[0], $before[$keys[0]]), 'add key 1');
  tap_assert(memcache_add($memcache, $keys[1], $before[$keys[1]]), 'add key 2');
  tap_assert(memcache_add($memcache, $keys[2], $before[$keys[2]]), 'add key 3');

  tap_assert(memcache_replace($memcache, $keys[0], $after[$keys[0]]), 'replace key 1 (2 args)');
  tap_assert(memcache_replace($memcache, $keys[1], $after[$keys[1]], false), 'replace key 2 (3 args)');
  tap_assert(memcache_replace($memcache, $keys[2], $after[$keys[2]], false, 30), 'replace key 3 (4 args)');

  // all keys should be present and updated
  tap_equal_unordered($after, memcache_get($memcache, $keys), 'check values');

  tap_assert(memcache_delete($memcache, $keys[0]), 'delete key 1');
  tap_assert(memcache_delete($memcache, $keys[1]), 'delete key 2');
  tap_assert(memcache_delete($memcache, $keys[2]), 'delete key 3');

  memcache_close($memcache);
}

test_memcache();
