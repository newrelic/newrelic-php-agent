<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Memcache metrics when keys are set using the Memcache
procedural api.
*/

/*SKIPIF
<?php require('skipif.inc');

if (version_compare(PHP_VERSION, "8.2", ">=")) {
  die("skip: test for PHP 8.2 separate\n");
}
*/

/*INI
*/

/*EXPECT
ok - connect to server
ok - set key 1 (2 args)
ok - get key 1
ok - set key 2 (3 args)
ok - get key 2
ok - set key 3 (4 args)
ok - get key 3
ok - multi-get
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
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Memcached/all"},                              [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Memcached/allOther"},                         [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/connect"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/connect",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/delete"},                 [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/delete",
      "scope":"OtherTransaction/php__FILE__"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get"},                    [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get",
      "scope":"OtherTransaction/php__FILE__"},                        [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/set"},                    [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/set",
      "scope":"OtherTransaction/php__FILE__"},                        [3, "??", "??", "??", "??", "??"]],
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




require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

define('KEYLEN', 8);   /* Should be long enough to prevent collisions. */

function test_memcache() {
  global $MEMCACHE_HOST, $MEMCACHE_PORT;

  $memcache = memcache_connect($MEMCACHE_HOST, $MEMCACHE_PORT);
  tap_not_equal(false, $memcache, 'connect to server');

  $data = array(
    randstr(KEYLEN) => 'aaa',
    randstr(KEYLEN) => 'bbb',
    randstr(KEYLEN) => 'ccc'
  );

  $keys = array_keys($data);

  /* Two argument form. */
  list($k1, $v1) = array($keys[0], $data[$keys[0]]);
  tap_assert(memcache_set($memcache, $k1, $v1), 'set key 1 (2 args)');
  tap_equal($v1, memcache_get($memcache, $k1), 'get key 1');

  /* Three argument form. */
  list($k2, $v2) = array($keys[1], $data[$keys[1]]);
  tap_assert(memcache_set($memcache, $k2, $v2, false), 'set key 2 (3 args)');
  tap_equal($v2, memcache_get($memcache, $k2), 'get key 2');

  /* Four argument form. */
  list($k3, $v3) = array($keys[2], $data[$keys[2]]);
  tap_assert(memcache_set($memcache, $k3, $v3, false, 30), 'set key 3 (4 args)');
  tap_equal($v3, memcache_get($memcache, $k3), 'get key 3');

  tap_equal_unordered($data, memcache_get($memcache, $keys), 'multi-get');

  /* Only test the one argument form, the two argument form is deprecated. */
  tap_assert(memcache_delete($memcache, $keys[0]), 'delete key 1');
  tap_assert(memcache_delete($memcache, $keys[1]), 'delete key 2');
  tap_assert(memcache_delete($memcache, $keys[2]), 'delete key 3');

  memcache_close($memcache);
}

test_memcache();
