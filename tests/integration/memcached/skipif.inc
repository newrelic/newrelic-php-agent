<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

if (!extension_loaded('memcached')) {
  die("skip: memcached extension required\n");
}

require('memcache.inc');

$memcached = new Memcached();
$memcached->addServer($MEMCACHE_HOST, $MEMCACHE_PORT);

/*
 * Memcached creates connections on demand, so we need to perform
 * a no-op query to verify a connection can be established successfully.
 */
$memcached->getVersion();
if (Memcached::RES_SUCCESS !== $memcached->getResultCode()) {
  die("skip: unable to connect to memcache server\n");
}

$memcached->quit();
