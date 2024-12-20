<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should record database metrics for the FETCH_COLUMN variant of
PDO::query() when PDO's specialized subclass constructor is used to
create connection object.
*/

/*SKIPIF
<?php require(realpath (dirname ( __FILE__ )) . '/../../skipif_sqlite.inc');
require(realpath (dirname ( __FILE__ )) . '/../../skipif_pdo_subclasses.inc');
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
*/

/*EXPECT_ERROR_EVENTS null*/

/*EXPECT
ok - create table
ok - insert one
ok - insert two
ok - insert three
ok - fetch column
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
    [{"name":"Supportability/Logging/Forwarding/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/disabled"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../test_query_2_logging_off.inc');

test_pdo_query(new Pdo\Sqlite('sqlite::memory:'));
