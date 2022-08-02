<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics for Redis sorted set operations.
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '5.4', '<')) {
    die("skip: PHP > 5.3 required\n");
}
$minimum_redis_datastore_version = '5.0.0';
require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT
ok - add three elements to sorted set
ok - pop maximum element off sorted set
ok - pop minimum element off sorted set
ok - we deleted our test key
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
    [{"name":"Datastore/all"},                              [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                         [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                        [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},                   [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect",
      "scope":"OtherTransaction/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del",
      "scope":"OtherTransaction/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists",
      "scope":"OtherTransaction/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zadd"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zadd",
      "scope":"OtherTransaction/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zpopmax"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zpopmax",
      "scope":"OtherTransaction/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zpopmin"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/zpopmin",
      "scope":"OtherTransaction/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},      [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

use NewRelic\Integration\Transaction;

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/integration.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/redis.inc');

function test_sorted_sets() {
  global $REDIS_HOST, $REDIS_PORT;

  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT);

  $key = randstr(16);
  if ($redis->exists($key)) {
    die("skip: key already exists: ${key}\n");
  }

  tap_equal(3, $redis->zadd($key, 0, 'min', 1, 'med', 2, 'max'), 'add three elements to sorted set');

  tap_equal(['max' => 2.0],  $redis->zpopmax($key), 'pop maximum element off sorted set');
  tap_equal(['min' => 0.0], $redis->zpopmin($key), 'pop minimum element off sorted set');

  tap_equal(1, $redis->del($key), 'we deleted our test key');

  $redis->close();
}

test_sorted_sets();
