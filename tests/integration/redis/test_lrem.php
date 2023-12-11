<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics including instance information for Redis
list operations.
*/

/*SKIPIF
<?php
if (version_compare(phpversion('redis'), '5.3.7', '>')) {
  die("skip: Redis <= 5.3.7 required\n");
}

require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 1
newrelic.datastore_tracer.instance_reporting.enabled = 1
*/

/*EXPECT
ok - add new elements
ok - remove first occurence of B
ok - verify list elements
*/

/*EXPECT_METRICS_EXIST
Datastore/operation/Redis/lrem
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/integration.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/redis.inc');

function test_redis() {
  global $REDIS_HOST, $REDIS_PORT;

  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT);

  /* generate unique key names to use for this test run */
  $key = randstr(16);
  $key2 = "{$key}_b";
  if ($redis->exists([$key, $key2])) {
    die("skip: key(s) already exist: $key, $key2\n");
  }

  /* Ensure the keys don't persist (too much) longer than the test. */
  $redis->expire($key, 30 /* seconds */);
  $redis->expire($key2, 30 /* seconds */);

  tap_equal(4, $redis->rpush($key, 'A', 'B', 'B', 'C'), 'add new elements');

  /* Redis->lRemove is depreacted, but use it once to verity it works */
  tap_equal(1, @$redis->lRemove($key, 'B', 1), 'remove first occurence of B');
  tap_equal(['A', 'B', 'C'], $redis->lrange($key, 0, -1), 'verify list elements');
}
test_redis();
