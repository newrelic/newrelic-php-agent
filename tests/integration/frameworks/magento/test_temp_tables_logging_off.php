<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should properly truncate Magento 2 temporary tables.
*/

/*SKIPIF
<?php

if (!extension_loaded("sqlite3")) {
  die("skip: sqlite3 extension required.");
}

*/

/*INI
newrelic.framework = magento2
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/all"},                                 [12, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/allOther"},                            [12, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [12, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [12, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/create"},                    [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/drop"},                      [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/insert"},                    [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/search_/create"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/search_/drop"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/search_/insert"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/search_tmp_*\/create"},       [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/search_tmp_*\/drop"},         [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/search_tmp_*\/insert"},       [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/search_tmp_/create"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/search_tmp_/drop"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/search_tmp_/insert"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/Action/unknown"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/Action/unknown"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Magento2/forced"},             [1, 0, 0, 0, 0, 0]],
    [{"name":"Datastore/statement/SQLite/search_tmp_*\/create",
      "scope":"OtherTransaction/Action/unknown"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/search_tmp_*\/drop",
      "scope":"OtherTransaction/Action/unknown"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/search_tmp_*\/insert",
      "scope":"OtherTransaction/Action/unknown"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/search_/create",
      "scope":"OtherTransaction/Action/unknown"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/search_/drop",
      "scope":"OtherTransaction/Action/unknown"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/search_/insert",
      "scope":"OtherTransaction/Action/unknown"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/search_tmp_/create",
      "scope":"OtherTransaction/Action/unknown"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/search_tmp_/drop",
      "scope":"OtherTransaction/Action/unknown"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/search_tmp_/insert",
      "scope":"OtherTransaction/Action/unknown"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/disabled"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/



function test_magento_temp_tables()
{
    $conn = new SQLite3(":memory:");
    $conn->exec("CREATE TABLE search_tmp_5771897a542b48_79048580 (id INT, desc VARCHAR(10));");
    $conn->exec("CREATE TABLE search_tmp_5 (id INT, desc VARCHAR(10));");
    $conn->exec("CREATE TABLE search_tmp_ (id INT, desc VARCHAR(10));");
    $conn->exec("CREATE TABLE search_ (id INT, desc VARCHAR(10));");

    $conn->exec("INSERT INTO search_tmp_5771897a542b48_79048580 VALUES (1, 'one');");
    $conn->exec("INSERT INTO search_tmp_5 VALUES (1, 'one');");
    $conn->exec("INSERT INTO search_tmp_ VALUES (1, 'one');");
    $conn->exec("INSERT INTO search_ VALUES (1, 'one');");

    $conn->exec("DROP TABLE search_tmp_5771897a542b48_79048580;");
    $conn->exec("DROP TABLE search_tmp_5;");
    $conn->exec("DROP TABLE search_tmp_;");
    $conn->exec("DROP TABLE search_;");
}

test_magento_temp_tables();
