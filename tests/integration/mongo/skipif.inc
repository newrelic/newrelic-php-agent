<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

if (!extension_loaded('mongo')) {
  die('skip: mongo extension required');
}

require('mongo.inc');

try {
  $client = new MongoClient(mongo_server());
  $client->close();
} catch (MongoConnectionException $e) {
  die('skip: ' . $e->getMessage() . "\n");
}
