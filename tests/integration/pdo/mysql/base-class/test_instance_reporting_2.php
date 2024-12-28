<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When PDO base class constructor is used to create connection object
to a database on a localhost via a unix socket,
and database instance reporting is enabled, the agent should
 - not generate errors
 - record a datastore metrics
 - record a datastore instance metric
 - record a datastore span event with instance information
Moreover, when the query execution time exceeds the explain threshold,
the agent should record a slow sql trace with database instance information.
*/

/*SKIPIF
<?php require(realpath (dirname ( __FILE__ )) . '/../../skipif_mysql.inc');
*/

/*ENVIRONMENT
DATASTORE_PRODUCT=MySQL
DATASTORE_COLLECTION=test
*/

/*INI
;comment=Set explain_threshold to 0 to ensure that the slow query is recorded.
newrelic.transaction_tracer.explain_threshold = 0
*/

/*EXPECT_ERROR_EVENTS null*/

/*EXPECT
ok - create table
ok - drop table
*/

/*EXPECT_METRICS_EXIST
Datastore/all, 2
Datastore/allOther, 2
Datastore/instance/ENV[DATASTORE_PRODUCT]/__HOST__/ENV[MYSQL_SOCKET], 2
Datastore/ENV[DATASTORE_PRODUCT]/all, 2
Datastore/ENV[DATASTORE_PRODUCT]/allOther, 2
Datastore/operation/ENV[DATASTORE_PRODUCT]/create, 1
Datastore/statement/ENV[DATASTORE_PRODUCT]/ENV[DATASTORE_COLLECTION]/create, 1
Datastore/operation/ENV[DATASTORE_PRODUCT]/drop, 1
Datastore/statement/ENV[DATASTORE_PRODUCT]/ENV[DATASTORE_COLLECTION]/drop, 1
Supportability/TxnData/SlowSQL, 1
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL id",
      "DROP TABLE ENV[DATASTORE_COLLECTION];",
      "Datastore/statement/ENV[DATASTORE_PRODUCT]/ENV[DATASTORE_COLLECTION]/drop",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "backtrace": [
          "/ in PDO::exec called at .*\/",
          " in test_instance_reporting called at __FILE__ (??)"
        ],
        "host": "__HOST__",
        "port_path_or_id": "ENV[MYSQL_SOCKET]",
        "database_name": "ENV[MYSQL_DB]"
      }
    ],
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL id",
      "CREATE TABLE ENV[DATASTORE_COLLECTION] (id INT, description VARCHAR(?));",
      "Datastore/statement/ENV[DATASTORE_PRODUCT]/ENV[DATASTORE_COLLECTION]/create",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "backtrace": [
          "/ in PDO::exec called at .*\/",
          " in test_instance_reporting called at __FILE__ (??)"
        ],
        "host": "__HOST__",
        "port_path_or_id": "ENV[MYSQL_SOCKET]",
        "database_name": "ENV[MYSQL_DB]"
      }
    ]
  ]
]
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore/statement/ENV[DATASTORE_PRODUCT]/ENV[DATASTORE_COLLECTION]/create",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??",
      "span.kind": "client",
      "component": "ENV[DATASTORE_PRODUCT]"
    },
    {},
    {
      "peer.hostname": "__HOST__",
      "peer.address": "__HOST__:ENV[MYSQL_SOCKET]",
      "db.instance": "ENV[MYSQL_DB]",
      "db.statement": "CREATE TABLE ENV[DATASTORE_COLLECTION] (id INT, description VARCHAR(?));"
    }
  ],
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore/statement/ENV[DATASTORE_PRODUCT]/ENV[DATASTORE_COLLECTION]/drop",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??",
      "span.kind": "client",
      "component": "ENV[DATASTORE_PRODUCT]"
    },
    {},
    {
      "peer.hostname": "__HOST__",
      "peer.address": "__HOST__:ENV[MYSQL_SOCKET]",
      "db.instance": "ENV[MYSQL_DB]",
      "db.statement": "DROP TABLE ENV[DATASTORE_COLLECTION];"
    }
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../test_instance_reporting.inc');
require_once(realpath (dirname ( __FILE__ )) . '/../../../../include/config.php');

$DSN = 'mysql:';
$DSN .= 'unix_socket=' . $MYSQL_SOCKET . ';';
$DSN .= 'dbname=' . $MYSQL_DB . ';';

test_instance_reporting(new PDO($DSN, $MYSQL_USER, $MYSQL_PASSWD), 0);
