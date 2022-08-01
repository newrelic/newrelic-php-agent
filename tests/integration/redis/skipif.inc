<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

require('redis.inc');

global $REDIS_HOST, $REDIS_PORT;

$extension = 'redis';
  
if (!extension_loaded($extension)) {
  die("skip: " . $extension . " extension required");
}
if (!isset($minimum_extension_version)) {
  $minimum_extension_version = '4.3.0';
}
$extension_version = phpversion($extension);
if (version_compare($extension_version, $minimum_extension_version) < 0) {
  die("skip: " . $extension . " extension version >= " . $minimum_extension_version . " required, detected version: " . $extension_version);
}

if (!isset($minimum_redis_datastore_version)) {
  $minimum_redis_datastore_version = '4.0.0';
}

try {
  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT, 5.0);
  $datastore_version = $redis->info("server")["redis_version"];
  $redis->close();
  if (version_compare($datastore_version, $minimum_redis_datastore_version) < 0) {
    die("skip: Redis data store version >= " . $minimum_redis_datastore_version . " required, detected version: " . $datastore_version);
  }
} catch (RedisException $e) {
  die("skip: " . $e->getMessage() . "\n");
}
