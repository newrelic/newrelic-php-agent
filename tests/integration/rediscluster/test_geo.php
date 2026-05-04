<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore/Redis metrics for \RedisCluster geo operations,
mirroring the coverage of tests/integration/redis/test_geo.php.
*/

/*SKIPIF
<?php
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

/*EXPECT_METRICS_EXIST
Datastore/all
Datastore/allOther
Datastore/Redis/all
Datastore/Redis/allOther
Datastore/operation/Redis/geoadd
Datastore/operation/Redis/geodist
Datastore/operation/Redis/geohash
Datastore/operation/Redis/geopos
Datastore/operation/Redis/georadius
Datastore/operation/Redis/georadius_ro
Datastore/operation/Redis/georadiusbymember
Datastore/operation/Redis/georadiusbymember_ro
Datastore/operation/Redis/exists
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/operation/Redis/connect
Datastore/operation/Redis/pconnect
Datastore/operation/Redis/open
Datastore/operation/Redis/popen
Datastore/operation/Redis/select
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

function test_geo_cluster() {
  global $REDIS_CLUSTER_HOST, $REDIS_CLUSTER_PORT;

  $cluster = new RedisCluster(null,
    [$REDIS_CLUSTER_HOST . ':' . $REDIS_CLUSTER_PORT],
    1.5, 1.5, true);

  /* generate a unique key to use for this test run */
  $key = randstr(16);
  if ($cluster->exists($key)) {
    die("skip: key already exists: $key\n");
  }

  $seattle = [-122.335167, 47.608013];
  $cupertino = [-122.032182, 37.322998];
  $tokyo = [139.839478, 35.652832];

  tap_equal(1, $cluster->geoadd($key, $seattle[0], $seattle[1], 'Seattle'), 'add Seattle to a geo sorted set');
  tap_equal(1, $cluster->geoadd($key, $cupertino[0], $cupertino[1], 'Cupertino'), 'add Cupertino to geo sorted set');
  tap_equal(1, $cluster->geoadd($key, $tokyo[0], $tokyo[1], 'Tokyo'), 'add Tokyo to geo sorted set');

  tap_equal(711.0, round($cluster->geodist($key, 'Seattle', 'Cupertino', 'mi')), 'query distance between two cities');
  tap_equal(["xn76y4khb10"], $cluster->geohash($key, "Tokyo"), 'query Tokyo geohash string');

  list($redis_lat, $redis_lng) = $cluster->geopos($key, 'Seattle')[0];
  tap_equal([round($seattle[0], 2), round($seattle[1], 2)], [round($redis_lat, 2), round($redis_lng, 2)],
            'verify Seattle geopos in redis');

  list($lat, $long) = $seattle;
  tap_equal_unordered_values(['Seattle', 'Cupertino'], $cluster->georadius($key, $lat, $long, 1000, 'mi'), 'query cities within 1000mi of Seattle');
  tap_equal_unordered_values(['Seattle', 'Cupertino'], $cluster->georadius_ro($key, $lat, $long, 1000, 'mi'), 'query cities within 1000mi of Seattle [readonly cmd]');

  tap_equal_unordered_values(['Seattle', 'Cupertino', 'Tokyo'], $cluster->georadiusbymember($key, 'Seattle', 5000, 'mi'), 'query cities within 5000mi of seattle by member');
  tap_equal_unordered_values(['Seattle', 'Cupertino', 'Tokyo'], $cluster->georadiusbymember_ro($key, 'Seattle', 5000, 'mi'), 'query cities within 5000mi of seattle by member [readonly cmd]');

  $cluster->close();
}

test_geo_cluster();
