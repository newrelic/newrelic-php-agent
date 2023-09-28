<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Memcache metrics when keys are added using the Memcache
procedural api.
*/

/*SKIPIF
<?php require('skipif.inc');

if (version_compare(PHP_VERSION, "8.2", ">=")) {
  die("skip: test for PHP 8.2 separate\n");
}
*/

/*INI
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
*/

/*EXPECT
ok - connect to server
ok - add key 1
ok - add key 2
ok - add key 3
ok - check values
ok - add duplicate
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
    [{"name":"Datastore/all"},                                        [9, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [9, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Memcached/all"},                              [9, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Memcached/allOther"},                         [9, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/add"},                    [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/add",
      "scope":"OtherTransaction/php__FILE__"},                        [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/connect"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/connect",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/delete"},                 [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/delete",
      "scope":"OtherTransaction/php__FILE__"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/disabled"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]]
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

  $data = array(
    randstr(KEYLEN) => 'aaa',
    randstr(KEYLEN) => 'bbb',
    randstr(KEYLEN) => 'ccc'
  );

  // convenience vars
  $keys = array_keys($data);
  $vals = array_values($data);

  tap_assert(memcache_add($memcache, $keys[0], $vals[0]), 'add key 1');
  tap_assert(memcache_add($memcache, $keys[1], $vals[1], false), 'add key 2');
  tap_assert(memcache_add($memcache, $keys[2], $vals[2], false, 30), 'add key 3');

  // all keys should be present
  tap_equal_unordered($data, memcache_get($memcache, $keys), 'check values');

  // adding a duplicate should fail
  tap_refute(memcache_add($memcache, $keys[0], $vals[0]), 'add duplicate');

  tap_assert(memcache_delete($memcache, $keys[0]), 'delete key 1');
  tap_assert(memcache_delete($memcache, $keys[1]), 'delete key 2');
  tap_assert(memcache_delete($memcache, $keys[2]), 'delete key 3');

  memcache_close($memcache);
}

test_memcache();
