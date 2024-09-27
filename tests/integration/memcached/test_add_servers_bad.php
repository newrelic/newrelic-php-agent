<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should gracefully handle when malformed server entries
are added via Memcached::addServers()
*/

/*SKIPIF
<?php require('skipif.inc'); ?>
*/

/*INI
*/

/*EXPECT_REGEX

.*(PHP )?Warning:.*could not add entry.*

.*(PHP )?Warning:.*could not add entry.*

*/

/*EXPECT_ERROR_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": "??",
    "events_seen": 1
  },
  [
    [
      {
        "type": "TransactionError",
        "timestamp": "??",
        "error.class": "E_WARNING",
        "error.message": "Memcached::addServers(): could not add entry #2 to the server list",
        "transactionName": "OtherTransaction\/php__FILE__",
        "duration": "??",
        "nr.transactionGuid": "??",
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??",
        "spanId": "??"
      },
      {},
      {}
    ]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/memcache.inc');

$memcached = new Memcached();
$memcached->addServers(array(array(1)));
$memcached->addServers(array(array("host1")));
$memcached->addServers(array(array(1, "host1")));
//$memcahed->addServers("string"); crashes PHP
$memcached->quit();
