<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report metrics for Memcached compare-and-swap operations.
*/

/*SKIPIF
<?php
require('skipif.inc');

if (version_compare(PHP_VERSION, "7.0", ">=")) {
  die("skip: PHP 5 only test\n");
}
*/

/*EXPECT
ok - set
ok - get
ok - cas
ok - get
ok - delete
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
    [{"name":"Datastore/operation/Memcached/get"},     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get",
      "scope":"OtherTransaction/php__FILE__"},         [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/replace"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/replace",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
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

  $key = randstr(KEYLEN);
  $token = 'foobar';

  $test = new TestCase($memcached);
  $test->set($key, 'hot potato');
  tap_equal('hot potato', $test->get($key, NULL, $token), 'get');
  $test->cas($token, $key, 'cold potato');
  tap_equal('cold potato', $test->get($key, NULL, $token), 'get');
  $test->delete($key);

  $memcached->quit();
}

main();
