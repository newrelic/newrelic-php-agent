<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore/instance/Redis/unknown/unknown for
\RedisCluster operations.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 1
newrelic.datastore_tracer.instance_reporting.enabled = 1
*/

/*EXPECT
ok - set key
ok - get key
ok - delete key
*/

/*EXPECT_METRICS_EXIST
Datastore/all
Datastore/allOther
Datastore/Redis/all
Datastore/Redis/allOther
Datastore/instance/Redis/unknown/unknown
Datastore/operation/Redis/set
Datastore/operation/Redis/get
Datastore/operation/Redis/del
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

function test_instance_basic_cluster() {
  global $REDIS_CLUSTER_HOST, $REDIS_CLUSTER_PORT;

  $cluster = new RedisCluster(null,
    [$REDIS_CLUSTER_HOST . ':' . $REDIS_CLUSTER_PORT],
    1.5, 1.5, true);

  $key = randstr(16);

  tap_assert($cluster->set($key, 'car'), 'set key');
  tap_equal('car', $cluster->get($key), 'get key');
  tap_equal(1, $cluster->del($key), 'delete key');

  $cluster->close();
}

test_instance_basic_cluster();
