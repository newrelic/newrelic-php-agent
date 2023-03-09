<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report metrics for Memcache::replace().
*/

/*SKIPIF
<?php require('skipif.inc');

if (version_compare(PHP_VERSION, "8.2", "<")) {
  die("skip: PHP 8.2 exclusive\n");
}
*/

/*INI
*/

/*EXPECT_REGEX
^.*\s*Deprecated:\s+Creation of dynamic property Memcache::\$connection is deprecated in\s.* on line\s.*
ok - connect to server
ok - add key 1
ok - add key 2
ok - add key 3
ok - replace key 1 \(2 args\)
ok - replace key 2 \(3 args\)
ok - replace key 3 \(4 args\)
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
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Memcached/all"},                              [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Memcached/allOther"},                         [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/add"},                    [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/add",
      "scope":"OtherTransaction/php__FILE__"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/connect"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/connect",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/delete"},                 [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/delete",
      "scope":"OtherTransaction/php__FILE__"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/replace"},                [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/replace",
      "scope":"OtherTransaction/php__FILE__"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name": "Errors/OtherTransaction/php__FILE__"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name": "Errors/all"},                                          [1, "??", "??", "??", "??", "??"]],
    [{"name": "Errors/allOther"},                                     [1, "??", "??", "??", "??", "??"]],
    [{"name": "ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/all"},  [1, "??", "??", "??", "??", "??"]],
    [{"name": "ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/



require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

define('KEYLEN', 8);   /* Should be long enough to prevent collisions. */

function test_memcache() {
  global $MEMCACHE_HOST, $MEMCACHE_PORT;

  $memcache = new Memcache();
  $memcache->addServer($MEMCACHE_HOST, $MEMCACHE_PORT);     /* Prevents a warning during connect. */
  tap_assert($memcache->connect($MEMCACHE_HOST), 'connect to server');

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

  tap_assert($memcache->add($keys[0], $before[$keys[0]]), 'add key 1');
  tap_assert($memcache->add($keys[1], $before[$keys[1]]), 'add key 2');
  tap_assert($memcache->add($keys[2], $before[$keys[2]]), 'add key 3');

  tap_assert($memcache->replace($keys[0], $after[$keys[0]]), 'replace key 1 (2 args)');
  tap_assert($memcache->replace($keys[1], $after[$keys[1]], false), 'replace key 2 (3 args)');
  tap_assert($memcache->replace($keys[2], $after[$keys[2]], false, 30), 'replace key 3 (4 args)');

  /* All keys should be present and updated. */
  tap_equal_unordered($after, $memcache->get($keys), 'check values');

  tap_assert($memcache->delete($keys[0]), 'delete key 1');
  tap_assert($memcache->delete($keys[1]), 'delete key 2');
  tap_assert($memcache->delete($keys[2]), 'delete key 3');

  $memcache->close();
}

test_memcache();
