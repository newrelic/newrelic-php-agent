<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

require('redis.inc');

global $REDIS_HOST, $REDIS_PORT;
  
if (!extension_loaded("redis")) {
  die("skip: redis extension required");
}
  
try {
  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT, 5.0);
  $datastore_version = $redis->info("server")["redis_version"];
  $redis->close();
  if (version_compare($datastore_version, '4.0.0') < 0) {
    die("skip: Redis data store version >= 4.0.0 required, detected version: " . $datastore_version);
  }
} catch (RedisException $e) {
  die("skip: " . $e->getMessage() . "\n");
}
