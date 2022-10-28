<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics for Redis basic operations.
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '5.4', '<')) {
    die("skip: PHP > 5.3 required\n");
}
require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT
ok - set initial key
ok - set second key
ok - count set bits in key
ok - find first clear bit in key
ok - find first set bit in key
ok - verify first bit clear
ok - verify second bit set
ok - clear third bit
ok - set fourth bit
ok - verify new key value
ok - perform bit operations on two keys
ok - verify value after ANDing
ok - delete test keys
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [15, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [15, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                                  [15, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},                             [15, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/bitcount"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/bitcount",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/bitop"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/bitop",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/bitpos"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/bitpos",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/get"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/get",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/getbit"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/getbit",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/set"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/set",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/setbit"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/setbit",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/



require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/redis.inc');

function test_bitops() {
  global $REDIS_HOST, $REDIS_PORT;

  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT);

  /* generate a unique key to use for this test run */
  $key1 = randstr(16);
  $key2 = "${key1}_b";
  $dkey = "${key1}_d";
  if ($redis->exists([$key1, $key2, $dkey])) {
    die("skip: key(s) already exist: ${key1}, ${key2}, ${dkey}\n");
  }

  /* T - 01010100; E - 01000101; N - 01001110 */
  tap_assert($redis->set($key1, 'TEN'), 'set initial key');
  tap_assert($redis->set($key2, 'HAT'), 'set second key');

  tap_equal(10, $redis->bitcount($key1), 'count set bits in key');
  tap_equal(0, $redis->bitpos($key1, 0), 'find first clear bit in key');
  tap_equal(1, $redis->bitpos($key1, 1), 'find first set bit in key');

  tap_equal(0, $redis->getbit($key1, 0), 'verify first bit clear');
  tap_equal(1, $redis->getbit($key1, 1), 'verify second bit set');

  /* T - 01010100 -> L - 01001100 */
  tap_equal(1, $redis->setbit($key1, 3, 0), 'clear third bit');
  tap_equal(0, $redis->setbit($key1, 4, 1), 'set fourth bit');
  tap_equal('LEN', $redis->get($key1), 'verify new key value');

  /* |L| 01001100 |E| 01000101 |N| 01000100 &
     |H| 01001000 |A| 01000001 |T| 01010100
     |H| 01001000 |A| 01000001 |D| 01000100 */
  tap_equal(3, $redis->bitop('AND', $dkey, $key1, $key2), 'perform bit operations on two keys');
  tap_equal('HAD', $redis->get($dkey), 'verify value after ANDing');

  tap_equal(3, $redis->del([$key1, $key2, $dkey]), 'delete test keys');

  /* close connection */
  $redis->close();
}

test_bitops();
