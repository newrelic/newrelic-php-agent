<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
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
ok - set key
ok - get key
ok - delete key
ok - delete missing key
ok - mset key
ok - mget key
ok - delete key
ok - reuse deleted key
ok - set duplicate key
ok - delete key
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                         [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                    [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                   [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},              [11, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del"},         [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del",
      "scope":"OtherTransaction/php__FILE__"},         [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/get"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/get",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/set"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/set",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/mget"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/mget",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/mset"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/mset",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/setnx"},       [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/setnx",
      "scope":"OtherTransaction/php__FILE__"},         [2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/redis.inc');

function test_basic() {
  global $REDIS_HOST, $REDIS_PORT;

  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT);

  /* generate a unique key to use for this test run */
  $key = randstr(16);
  if ($redis->exists($key)) {
    echo "key already exists: ${key}\n";
    exit(1);
  }

  /* the tests */
  tap_assert($redis->set($key, 'bar'), 'set key');
  tap_equal('bar', $redis->get($key), 'get key');
  tap_equal(1, $redis->del($key), 'delete key');
  tap_equal(0, $redis->del($key), 'delete missing key');

  tap_assert($redis->mset([$key => 'bar']), 'mset key');
  tap_equal(['bar'], $redis->mget([$key]), 'mget key');
  tap_equal(1, $redis->del($key), 'delete key');

  tap_assert($redis->setnx($key, 'bar'), 'reuse deleted key');
  tap_refute($redis->setnx($key, 'bar'), 'set duplicate key');

  /* cleanup the key used by this test run */
  tap_equal(1, $redis->del($key), 'delete key');

  /* close connection */
  $redis->close();
}

test_basic();
