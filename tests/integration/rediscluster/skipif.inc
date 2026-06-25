<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

global $REDIS_CLUSTER_HOST, $REDIS_CLUSTER_PORT;

if (!extension_loaded('redis')) {
  die("skip: redis extension required\n");
}

if (!class_exists('RedisCluster')) {
  die("skip: \\RedisCluster class required (build phpredis with cluster support)\n");
}

if (!isset($minimum_extension_version)) {
  $minimum_extension_version = '4.3.0';
}
$extension_version = phpversion('redis');
if (version_compare($extension_version, $minimum_extension_version) < 0) {
  die("skip: redis extension version >= " . $minimum_extension_version
    . " required, detected version: " . $extension_version . "\n");
}

if (!isset($minimum_redis_datastore_version)) {
  /*
   * `CLUSTER ADDSLOTSRANGE` (used by the docker-compose redisclusterdb service
   * to bootstrap a single-node cluster) was introduced in Redis 7.0.
   */
  $minimum_redis_datastore_version = '7.0.0';
}

try {
  $cluster = new RedisCluster(null,
    [$REDIS_CLUSTER_HOST . ':' . $REDIS_CLUSTER_PORT],
    5.0, 5.0);
  /*
   * \RedisCluster::info() returns a per-master map keyed by "host:port".
   * Pull the version from the first master.
   */
  $info = $cluster->info($REDIS_CLUSTER_HOST . ':' . $REDIS_CLUSTER_PORT);
  $datastore_version = $info['redis_version'] ?? null;
  if (null === $datastore_version) {
    die("skip: could not determine Redis Cluster server version\n");
  }
  if (version_compare($datastore_version, $minimum_redis_datastore_version) < 0) {
    die("skip: Redis Cluster server version >= " . $minimum_redis_datastore_version
      . " required, detected version: " . $datastore_version . "\n");
  }
} catch (RedisClusterException $e) {
  die("skip: " . $e->getMessage() . "\n");
}
