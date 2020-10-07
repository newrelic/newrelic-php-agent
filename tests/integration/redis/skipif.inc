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
  $redis->close();
} catch (RedisException $e) {
  die("skip: " . $e->getMessage() . "\n");
}
