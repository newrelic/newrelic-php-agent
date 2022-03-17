<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics for Redis list operations.
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '8.0.0', '<')) {
    die("skip: PHP >= 8.0.0 required\n");
}
require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT_REGEX
ok - append A
ok - append B
ok - append C
((?s).*?)Deprecated: Method Redis::lGet((?s).*?)
ok - retrieve element 0
((?s).*?)Deprecated: Method Redis::lGet((?s).*?)
ok - retrieve element 1
((?s).*?)Deprecated: Method Redis::lGet((?s).*?)
ok - retrieve element 2
((?s).*?)Deprecated: Method Redis::lGet((?s).*?)
ok - retrieve last element
((?s).*?)Deprecated: Method Redis::lGet((?s).*?)
ok - retrieve invalid element
ok - retrieve element 0
ok - retrieve element 1
ok - retrieve element 2
((?s).*?)Deprecated: Method Redis::lRemove((?s).*?)
ok - remove first occurrence of B
((?s).*?)Deprecated: Method Redis::lGet((?s).*?)
ok - A was not removed
((?s).*?)Deprecated: Method Redis::lGet((?s).*?)
ok - C was not removed
((?s).*?)Deprecated: Method Redis::lGet((?s).*?)
ok - B was removed
ok - remove missing element
ok - replace list head
((?s).*?)Deprecated: Method Redis::lGet((?s).*?)
ok - list head was replaced
ok - delete list
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
    [{"name":"ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                         [17,"??","??","??","??","??"]],
    [{"name":"Datastore/allOther"},                    [17,"??","??","??","??","??"]],
    [{"name":"Datastore/Redis/all"},                   [17,"??","??","??","??","??"]],
    [{"name":"Datastore/Redis/allOther"},              [17,"??","??","??","??","??"]],
    [{"name":"Datastore/operation/Redis/connect"},     [1,"??","??","??","??","??"]],
    [{"name":"Datastore/operation/Redis/connect",
      "scope":"OtherTransaction/php__FILE__"},         [1,"??","??","??","??","??"]],
    [{"name":"Datastore/operation/Redis/del"},         [1,"??","??","??","??","??"]],
    [{"name":"Datastore/operation/Redis/del",
      "scope":"OtherTransaction/php__FILE__"},         [1,"??","??","??","??","??"]],
    [{"name":"Datastore/operation/Redis/lrem"},        [1,"??","??","??","??","??"]],
    [{"name":"Datastore/operation/Redis/lrem",
      "scope":"OtherTransaction/php__FILE__"},         [1,"??","??","??","??","??"]],
    [{"name":"Datastore/operation/Redis/lremove"},     [1,"??","??","??","??","??"]],
    [{"name":"Datastore/operation/Redis/lremove",
      "scope":"OtherTransaction/php__FILE__"},         [1,"??","??","??","??","??"]],
    [{"name":"Datastore/operation/Redis/lget"},        [9,"??","??","??","??","??"]],
    [{"name":"Datastore/operation/Redis/lget",
      "scope":"OtherTransaction/php__FILE__"},         [9,"??","??","??","??","??"]],
    [{"name":"Datastore/operation/Redis/lindex"},      [3,"??","??","??","??","??"]],
    [{"name":"Datastore/operation/Redis/lindex",
      "scope":"OtherTransaction/php__FILE__"},         [3,"??","??","??","??","??"]],
    [{"name":"Datastore/operation/Redis/lset"},        [1,"??","??","??","??","??"]],
    [{"name":"Datastore/operation/Redis/lset",
      "scope":"OtherTransaction/php__FILE__"},         [1,"??","??","??","??","??"]],
    [{"name":"Errors/OtherTransaction/php__FILE__"},   [1,"??","??","??","??","??"]],
    [{"name":"Errors/all"},                            [1,"??","??","??","??","??"]],
    [{"name":"Errors/allOther"},                       [1,"??","??","??","??","??"]],
    [{"name":"OtherTransaction/all"},                  [1,"??","??","??","??","??"]],
    [{"name":"OtherTransaction/php__FILE__"},          [1,"??","??","??","??","??"]],
    [{"name":"OtherTransactionTotalTime"},             [1,"??","??","??","??","??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"}, [1,"??","??","??","??","??"]]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/redis.inc');

function test_redis() {
  global $REDIS_HOST, $REDIS_PORT;

  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT);

  /* Generate a unique key to use for this test run */
  $key = randstr(16);
  if ($redis->exists($key)) {
    echo "key already exists: ${key}\n";
    exit(1);
  }

  /* Ensure the key doesn't persist (too much) longer than the test. */
  $redis->expire($key, 30 /* seconds */);

  tap_equal(1, $redis->rPush($key, 'A'), 'append A');
  tap_equal(2, $redis->rPush($key, 'B'), 'append B');
  tap_equal(3, $redis->rPush($key, 'C'), 'append C');

  tap_equal('A', $redis->lGet($key, 0), 'retrieve element 0');
  tap_equal('B', $redis->lGet($key, 1), 'retrieve element 1');
  tap_equal('C', $redis->lGet($key, 2), 'retrieve element 2');
  tap_equal('C', $redis->lGet($key, -1), 'retrieve last element');
  tap_refute($redis->lGet($key, 10), 'retrieve invalid element');

  tap_equal('A', $redis->lIndex($key, 0), 'retrieve element 0');
  tap_equal('B', $redis->lIndex($key, 1), 'retrieve element 1');
  tap_equal('C', $redis->lIndex($key, 2), 'retrieve element 2');

  tap_equal(1, $redis->lRemove($key, 'B', 1), 'remove first occurrence of B');
  tap_equal('A', $redis->lGet($key, 0), 'A was not removed');
  tap_equal('C', $redis->lGet($key, 1), 'C was not removed');
  tap_refute($redis->lGet($key, 2), 'B was removed');
  tap_equal(0, $redis->lRem($key, 'B', 1), 'remove missing element');

  tap_assert($redis->lSet($key, 0, 'AA'), 'replace list head');
  tap_equal('AA', $redis->lGet($key, 0), 'list head was replaced');

  /* Cleanup the key used by this test run */
  tap_equal(1, $redis->del($key), 'delete list');

  $redis->close();
}

test_redis();
