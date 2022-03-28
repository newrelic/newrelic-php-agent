<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report metrics for Memcached string operations.
*/

/*SKIPIF
<?php require('skipif.inc'); ?>
*/

/*EXPECT
ok - setByKey
ok - prependByKey
ok - appendByKey
ok - assertEqualByKey
ok - deleteByKey
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/all"},                         [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                    [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Memcached/all"},               [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Memcached/allOther"},          [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/delete"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/delete",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/replace"}, [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/replace",
      "scope":"OtherTransaction/php__FILE__"},         [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/set"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/set",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"}, [1, "??", "??", "??", "??", "??"]]
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
  $memcached->setOption (memcached::OPT_COMPRESSION, false);

  $key = randstr(KEYLEN);
  $server_key = 'server key';

  $test = new TestCase($memcached);
  $test->setByKey($server_key, $key, 'bar');
  $test->prependByKey($server_key, $key, 'foo ');
  $test->appendByKey($server_key, $key, ' baz');
  $test->assertEqualByKey('foo bar baz', $server_key, $key);
  $test->deleteByKey($server_key, $key);

  $memcached->quit();
}

main();
