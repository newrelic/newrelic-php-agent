<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report instance metrics when multiple servers are
added at once via Memcached::addServers()
*/

/*SKIPIF
<?php require('skipif.inc'); ?>
*/

/*INI
*/

/*EXPECT_METRICS_EXIST
Datastore/instance/Memcached/host1/1, 1
Datastore/instance/Memcached/host2/2, 1
Datastore/instance/Memcached/host3/11211, 1
Datastore/instance/Memcached/host4/1, 1
*/

/*EXPECT_ERROR_EVENTS null */

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/memcache.inc');

$memcached = new Memcached();
$memcached->addServers(array(
                       array("host1", 1),
                       array("host2", 2),
                       array("host3", 11211)));
$memcached->addServers(array());
$memcached->addServers(array(array("host4", 1, "test field")));
$memcached->quit();
