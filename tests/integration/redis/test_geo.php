<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics for Redis geo operations.
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
ok - add Seattle to a geo sorted set
ok - add Cupertino to geo sorted set
ok - add Tokyo to geo sorted set
ok - query distance between two cities
ok - query Tokyo geohash string
ok - verify Seattle geopos in redis
ok - query cities within 1000mi of Seattle
ok - query cities within 1000mi of Seattle [readonly cmd]
ok - query cities within 5000mi of seattle by member
ok - query cities within 5000mi of seattle by member [readonly cmd]
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [12, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [12, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                                  [12, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},                             [12, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/connect",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/geoadd"},                     [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/geoadd",
      "scope":"OtherTransaction/php__FILE__"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/geodist"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/geodist",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/geohash"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/geohash",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/geopos"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/geopos",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/georadius"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/georadius",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/georadius_ro"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/georadius_ro",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/georadiusbymember"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/georadiusbymember",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/georadiusbymember_ro"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/georadiusbymember_ro",
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




require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/redis.inc');

function test_redis_geo() {
  global $REDIS_HOST, $REDIS_PORT;

  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT);

  /* generate a unique key to use for this test run */
  $key = randstr(16);
  if ($redis->exists($key)) {
    die("skip: key already exists: $key\n");
  }

  $seattle = [-122.335167, 47.608013];
  $cupertino = [-122.032182, 37.322998];
  $tokyo = [139.839478, 35.652832];

  tap_equal(1, $redis->geoadd($key, $seattle[0], $seattle[1], 'Seattle'), 'add Seattle to a geo sorted set');
  tap_equal(1, $redis->geoadd($key, $cupertino[0], $cupertino[1], 'Cupertino'), 'add Cupertino to geo sorted set');
  tap_equal(1, $redis->geoadd($key, $tokyo[0], $tokyo[1], 'Tokyo'), 'add Tokyo to geo sorted set');

  /* Values calculated in another redis-cli session */
  tap_equal(711.0, round($redis->geodist($key, 'Seattle', 'Cupertino', 'mi')), 'query distance between two cities');
  tap_equal(["xn76y4khb10"], $redis->geohash($key, "Tokyo"), 'query Tokyo geohash string');

  list($redis_lat, $redis_lng) = $redis->geopos($key, 'Seattle')[0];
  tap_equal([round($seattle[0], 2), round($seattle[1], 2)], [round($redis_lat, 2), round($redis_lng, 2)],
            'verify Seattle geopos in redis');

  list ($lat, $long) = $seattle;
  tap_equal_unordered_values(['Seattle', 'Cupertino'], $redis->georadius($key, $lat, $long, 1000, 'mi'), 'query cities within 1000mi of Seattle');
  tap_equal_unordered_values(['Seattle', 'Cupertino'], $redis->georadius_ro($key, $lat, $long, 1000, 'mi'), 'query cities within 1000mi of Seattle [readonly cmd]');

  tap_equal_unordered_values(['Seattle', 'Cupertino', 'Tokyo'], $redis->georadiusbymember($key, 'Seattle', 5000, 'mi'), 'query cities within 5000mi of seattle by member');
  tap_equal_unordered_values(['Seattle', 'Cupertino', 'Tokyo'], $redis->georadiusbymember_ro($key, 'Seattle', 5000, 'mi'), 'query cities within 5000mi of seattle by member [readonly cmd]');

  /* close connection */
  $redis->close();
}

test_redis_geo();
