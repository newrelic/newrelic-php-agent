<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should record Datastore metrics for the one argument form of
PDO::query().
*/

/*SKIPIF
<?php require(realpath (dirname ( __FILE__ )) . '/../../skipif_sqlite.inc');
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT_ERROR_EVENTS null*/

/*EXPECT
ok - create table
ok - insert one
ok - insert two
ok - insert three
ok - query (1-arg)
ok - drop table
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/all"},                                 [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/SQLite/allOther"},                            [6, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/create"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/drop"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/insert"},                    [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/SQLite/select"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/create"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/drop"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/insert"},               [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/select"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/create",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/drop",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/insert",
      "scope":"OtherTransaction/php__FILE__"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/SQLite/test/select",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../test_query_1.inc');

test_pdo_query(new PDO('sqlite::memory:'));
