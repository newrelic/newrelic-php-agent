<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should properly rollup Wordpress site specific tables.
*/

/*SKIPIF
<?php

if (!extension_loaded("sqlite3")) {
  die("skip: sqlite3 extension required.");
}

*/

/*INI
newrelic.framework = wordpress
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/all"},                                 [9, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/allOther"},                            [9, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [9, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [9, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/create"},                    [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/drop"},                      [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/insert"},                    [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/wp_*_comments/create"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/wp_*_comments/drop"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/wp_*_comments/insert"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/wp_/create"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/wp_/drop"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/wp_/insert"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/wp_2/create"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/wp_2/drop"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/wp_2/insert"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/WordPress/forced"},            [1, 0, 0, 0, 0, 0]],
    [{"name":"Datastore/statement/SQLite/wp_*_comments/create",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/wp_*_comments/drop",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/wp_*_comments/insert",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/wp_/create",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/wp_/drop",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/wp_/insert",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/wp_2/create",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/wp_2/drop",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/wp_2/insert",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/



function test_wordpress_site_specific_tables()
{
    $conn = new SQLite3(":memory:");
    $conn->exec("CREATE TABLE wp_22_comments (id INT, desc VARCHAR(10));");
    $conn->exec("CREATE TABLE wp_2 (id INT, desc VARCHAR(10));");
    $conn->exec("CREATE TABLE wp_ (id INT, desc VARCHAR(10));");

    $conn->exec("INSERT INTO wp_22_comments VALUES (1, 'one');");
    $conn->exec("INSERT INTO wp_2 VALUES (1, 'one');");
    $conn->exec("INSERT INTO wp_ VALUES (1, 'one');");

    $conn->exec("DROP TABLE wp_22_comments;");
    $conn->exec("DROP TABLE wp_2;");
    $conn->exec("DROP TABLE wp_;");
}

test_wordpress_site_specific_tables();
