<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should generate explain plans when queries are executed inside fibers.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (version_compare(phpversion(), '8.1', '<')) {
    die("skip: PHP >= 8.1 required\n");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
newrelic.fibers.disabled = false
*/

/*EXPECT
STATISTICS
TABLES
*/

/*EXPECT_METRICS_EXIST
Datastore/all, 2
Datastore/allOther, 2
Datastore/MySQL/all, 2
Datastore/MySQL/allOther, 2
Datastore/statement/MySQL/tables/select, 2
Datastore/operation/MySQL/select, 2
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
  [
    {
      "traceId": "??",
      "duration": "??",
      "transactionId": "??",
      "name": "OtherTransaction\/php__FILE__",
      "guid": "ENV[GUID_ROOT]",
      "type": "Span",
      "category": "generic",
      "priority": "??",
      "sampled": true,
      "nr.entryPoint": true,
      "timestamp": "??",
      "transaction.name": "OtherTransaction\/php__FILE__"
    },
    {},
    {}
  ],
  [
    {
      "category": "generic",
      "type": "Span",
      "guid": "ENV[GUID_TEST_FIBER_1]",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/test_fiber_query",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_FIBER_1]"
    },
    {},
    {}
  ],
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore\/statement\/MySQL\/tables\/select",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_FIBER_1]",
      "span.kind": "client",
      "component": "MySQL"
    },
    {},
    {
      "peer.address": "unknown:unknown",
      "db.statement": "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?"
    }
  ],
  [
    {
      "category": "generic",
      "type": "Span",
      "guid": "ENV[GUID_TEST_FIBER_2]",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/test_fiber_query",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_FIBER_2]"
    },
    {},
    {}
  ],
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore\/statement\/MySQL\/tables\/select",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_FIBER_2]",
      "span.kind": "client",
      "component": "MySQL"
    },
    {},
    {
      "peer.address": "unknown:unknown",
      "db.statement": "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?"
    }
  ]
]
*/

/*EXPECT_ERROR_EVENTS
null
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?",
      "Datastore/statement/MySQL/tables/select",
      2,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "explain_plan": [
          [
            "id",
            "select_type",
            "table",
            "type",
            "possible_keys",
            "key",
            "key_len",
            "ref",
            "rows",
            "Extra"
          ],
          [
            [
              1,
              "SIMPLE",
              "tables",
              "ALL",
              null,
              "TABLE_NAME",
              null,
              null,
              null,
              "Using where; Skip_open_table; Scanned 1 database"
            ]
          ]
        ],
        "backtrace": [
          " in mysqli_stmt_execute called at __FILE__ (??)",
          " in test_fiber_query called at __FILE__ (??)",
          " in {closure} called at ? (?)",
          " in Fiber::resume called at __FILE__ (??)"
        ]
      }
    ]
  ]
]
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/helpers.php');

env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

function test_fiber_query($link, $table_name, $fiber_id)
{

    env_var_for_expects("GUID_TEST_FIBER_" . $fiber_id, newrelic_get_linking_metadata()['span.id'] ?? '');
    $query = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?";

    $stmt = mysqli_prepare($link, $query);
    if (FALSE === $stmt) {
        echo mysqli_error($link) . "\n";
        return;
    }

    if (FALSE === mysqli_stmt_bind_param($stmt, 's', $table_name)) {
        echo mysqli_stmt_error($stmt) . "\n";
        return;
    }

    // Suspend within the fiber before executing the query
    Fiber::suspend();

    if (FALSE === mysqli_stmt_execute($stmt)) {
        echo mysqli_stmt_error($stmt) . "\n";
        return;
    }

    if (FALSE === mysqli_stmt_bind_result($stmt, $result)) {
        echo mysqli_stmt_error($stmt) . "\n";
        return;
    }

    while (mysqli_stmt_fetch($stmt)) {
        echo $result . "\n";
    }

    mysqli_stmt_close($stmt);
}

// Create connection
$link = mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if (mysqli_connect_errno()) {
    echo mysqli_connect_error() . "\n";
    exit(1);
}

// Create fibers for database queries
$fiber1 = new Fiber(function() use ($link) {
    env_var_for_expects("GUID_FIBER_1", newrelic_get_linking_metadata()['span.id'] ?? '');
    return test_fiber_query($link, 'STATISTICS', 1);
});

$fiber2 = new Fiber(function() use ($link) {
    env_var_for_expects("GUID_FIBER_2", newrelic_get_linking_metadata()['span.id'] ?? '');
    return test_fiber_query($link, 'TABLES', 2);
});

// Start and execute fibers
$fiber1->start();
$fiber2->start();

// Resume fibers to complete the queries
$fiber1->resume();
$fiber2->resume();

mysqli_close($link);
