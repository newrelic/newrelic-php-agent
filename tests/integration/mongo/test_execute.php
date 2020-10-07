<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should generate Datastore metrics for MongoDB::execute().
*/

/*SKIPIF
<?php require('skipif.inc'); ?>
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/all"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MongoDB/all"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MongoDB/allOther"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MongoDB/execute"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MongoDB/execute",
      "scope":"OtherTransaction/php__FILE__"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__ )) . '/mongo.inc');

$client = new MongoClient(mongo_server());
$db = $client->selectDB('test');
$db->execute("17.0;");
