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

if (version_compare(PHP_VERSION, "7.0", "<")) {
  die("skip: PHP 7 only test\n");
}
*/

/*INI
*/

/*EXPECT
ok - setByKey
ok - getByKey
ok - getByKey value
ok - casByKey
ok - getByKey
ok - getByKey value
ok - deleteByKey
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
    [{"name":"Datastore/instance/Memcached/ENV[MEMCACHE_HOST]/ENV[MEMCACHE_PORT]"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/delete"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/delete",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get"},                    [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get",
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

  $key = randstr(KEYLEN);
  $server_key = 'server key';

  $test = new TestCase($memcached);
  $test->setByKey($server_key, $key, 'hot potato');

  $values = $test->getByKey($server_key, $key, NULL, Memcached::GET_EXTENDED);
  tap_equal('hot potato', $values['value'], 'getByKey value');

  $test->casByKey($values['cas'], $server_key, $key, 'cold potato');

  $values = $test->getByKey($server_key, $key, NULL, Memcached::GET_EXTENDED);
  tap_equal('cold potato', $values['value'], 'getByKey value');

  $test->deleteByKey($server_key, $key);
  $memcached->quit();
}

main();
