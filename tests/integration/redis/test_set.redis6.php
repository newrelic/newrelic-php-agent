<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics for Redis set operations.
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '5.4', '<')) {
    die("skip: PHP > 5.3 required\n");
}
$minimum_redis_datastore_version='6.2.0';
$minimum_extension_version='5.3.5';
require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT
ok - check membership of multiple elements
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
    [{"name":"Datastore/all"},                         [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                    [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                   [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},              [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/sadd"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/sadd",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/smismember"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/smismember",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"}, [1, "??", "??", "??", "??", "??"]]
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

  /* generate a unique key to use for this test run */
  $key1 = randstr(16);
  if ($redis->exists([$key1]) > 0) {
    echo "key already exists: ${key1}\n";
    exit(1);
  }

  $redis->sadd($key1, 'foo');
  tap_equal([1, 0], $redis->smismember($key1, 'foo', 'bar'), 'check membership of multiple elements');

  /* close connection */
  $redis->close();
}

test_sorted_sets();
