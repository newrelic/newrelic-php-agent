<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should generate Datastore metrics for MongoCollection::find().
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
    [{"name":"Datastore/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MongoDB/all"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MongoDB/allOther"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MongoDB/find"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MongoDB/test.produce/find"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MongoDB/test.produce/find",
      "scope":"OtherTransaction/php__FILE__"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},         [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath (dirname ( __FILE__ )) . '/mongo.inc');

/* See http://php.net/manual/en/mongocollection.find.php */
function test_find($db) {
  $produce = new MongoCollection($db, 'produce');
  $fruitQuery = array('Type' => 'Fruit');
  $cursor = $produce->find($fruitQuery);
  foreach ($cursor as $doc) {
    var_dump($doc);
  }
}

function main() {
  $client = new MongoClient(mongo_server());
  $db = $client->selectDB('test');

  test_find($db);
}

main();
