<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

if (!extension_loaded('memcache')) {
  die("skip: memcache extension required\n");
}

require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

/*
 * memcache_connect throws a notice and a warning when a connection
 * attempt fails. Suppress these at the cost of potentially masking
 * a more serious error.
 */
$memcache = @memcache_connect($MEMCACHE_HOST, $MEMCACHE_PORT);

if (!$memcache) {
  die("skip: unable to connect to memcache server\n");
}

$memcache->close();
