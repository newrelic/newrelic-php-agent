<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report instance metrics when multiple servers are
added at once
*/

/*SKIPIF
<?php require('skipif.inc'); ?>
*/

/*INI
*/

/*EXPECT_METRICS_EXIST
Datastore/instance/Memcached/__HOST__/my/socket, 1
*/

/*EXPECT_ERROR_EVENTS null */

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/memcache.inc');

$memcached = new Memcached();
$memcached->addServer("my/socket", 0);
$memcached->quit();
