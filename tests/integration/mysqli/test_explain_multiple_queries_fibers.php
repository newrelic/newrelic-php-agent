<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should generate explain plans for multiple different query types
executed across different fibers, testing various information_schema queries.
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

/*EXPECT_REGEX
Fiber A: Checking for STATISTICS table
Fiber B: Checking for TABLES table
Fiber C: Counting information_schema tables
Found .* tables
Found table: TABLES
Found table: STATISTICS
All fiber queries completed successfully
*/

/*EXPECT_METRICS_EXIST
Datastore/all, 3
Datastore/allOther, 3
Datastore/MySQL/all, 3
Datastore/MySQL/allOther, 3
Datastore/statement/MySQL/tables/select, 3
Datastore/operation/MySQL/select, 3
Supportability/PHP/Fiber/used
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
      "guid": "ENV[GUID_FIBER_CHECK_A]",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/fiber_table_check",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_FIBER_A]"
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
      "parentId": "ENV[GUID_FIBER_CHECK_A]",
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
      "guid": "ENV[GUID_FIBER_CHECK_B]",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/fiber_table_check",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_FIBER_B]"
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
      "parentId": "ENV[GUID_FIBER_CHECK_B]",
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
      "guid": "ENV[GUID_FIBER_COUNT_C]",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/fiber_table_count",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_FIBER_C]"
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
      "parentId": "ENV[GUID_FIBER_COUNT_C]",
      "span.kind": "client",
      "component": "MySQL"
    },
    {},
    {
      "peer.address": "unknown:unknown",
      "db.statement": "SELECT COUNT(*) as table_count FROM information_schema.tables WHERE table_schema=?"
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
          " in fiber_table_check called at __FILE__ (??)",
          " in {closure} called at ? (?)",
          " in Fiber::resume called at __FILE__ (??)"
        ]
      }
    ],
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT COUNT(*) as table_count FROM information_schema.tables WHERE table_schema=?",
      "Datastore/statement/MySQL/tables/select",
      1,
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
              "TABLE_SCHEMA",
              null,
              null,
              null,
              "Using where; Skip_open_table; Scanned 1 database"
            ]
          ]
        ],
        "backtrace": [
          " in mysqli_stmt_execute called at __FILE__ (??)",
          " in fiber_table_count called at __FILE__ (??)",
          "??",
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

function fiber_table_check($link, $fiber_name, $table_name)
{

    env_var_for_expects("GUID_FIBER_CHECK_". $fiber_name, newrelic_get_linking_metadata()['span.id'] ?? '');

    echo "Fiber $fiber_name: Checking for $table_name table\n";

    $query = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?";
    $stmt = mysqli_prepare($link, $query);
    if (FALSE === $stmt) {
        echo "$fiber_name prepare error: " . mysqli_error($link) . "\n";
        return;
    }

    if (FALSE === mysqli_stmt_bind_param($stmt, 's', $table_name)) {
        echo "$fiber_name bind error: " . mysqli_stmt_error($stmt) . "\n";
        return;
    }

    // Suspend before executing query
    Fiber::suspend();

    if (FALSE === mysqli_stmt_execute($stmt)) {
        echo "$fiber_name execute error: " . mysqli_stmt_error($stmt) . "\n";
        return;
    }

    if (FALSE === mysqli_stmt_bind_result($stmt, $result)) {
        echo "$fiber_name bind result error: " . mysqli_stmt_error($stmt) . "\n";
        return;
    }

    if (mysqli_stmt_fetch($stmt)) {
        echo "Found table: " . $result . "\n";
    } else {
        echo "Table $table_name not found\n";
    }

    mysqli_stmt_close($stmt);
}

function fiber_table_count($link, $fiber_name, $schema_name)
{
    env_var_for_expects("GUID_FIBER_COUNT_" . $fiber_name, newrelic_get_linking_metadata()['span.id'] ?? '');

    echo "Fiber $fiber_name: Counting $schema_name tables\n";

    $query = "SELECT COUNT(*) as table_count FROM information_schema.tables WHERE table_schema=?";
    $stmt = mysqli_prepare($link, $query);
    if (FALSE === $stmt) {
        echo "$fiber_name prepare error: " . mysqli_error($link) . "\n";
        return;
    }

    if (FALSE === mysqli_stmt_bind_param($stmt, 's', $schema_name)) {
        echo "$fiber_name bind error: " . mysqli_stmt_error($stmt) . "\n";
        return;
    }

    // Suspend before executing query
    Fiber::suspend();

    if (FALSE === mysqli_stmt_execute($stmt)) {
        echo "$fiber_name execute error: " . mysqli_stmt_error($stmt) . "\n";
        return;
    }

    if (FALSE === mysqli_stmt_bind_result($stmt, $count)) {
        echo "$fiber_name bind result error: " . mysqli_stmt_error($stmt) . "\n";
        return;
    }

    if (mysqli_stmt_fetch($stmt)) {
        echo "Found $count tables\n";
    }

    mysqli_stmt_close($stmt);
}

// Create database connection
$link = mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if (mysqli_connect_errno()) {
    echo mysqli_connect_error() . "\n";
    exit(1);
}

// Create multiple fibers with different query types
$fiber_a = new Fiber(function() use ($link) {
    env_var_for_expects("GUID_FIBER_A", newrelic_get_linking_metadata()['span.id'] ?? '');
    return fiber_table_check($link, 'A', 'STATISTICS');
});

$fiber_b = new Fiber(function() use ($link) {
    env_var_for_expects("GUID_FIBER_B", newrelic_get_linking_metadata()['span.id'] ?? '');
    return fiber_table_check($link, 'B', 'TABLES');
});

$fiber_c = new Fiber(function() use ($link) {
    env_var_for_expects("GUID_FIBER_C", newrelic_get_linking_metadata()['span.id'] ?? '');
    return fiber_table_count($link, 'C', 'information_schema');
});

// Start all fibers
$fiber_a->start();
$fiber_b->start();
$fiber_c->start();

// Resume all fibers in sequence
$fibers = [$fiber_c, $fiber_b, $fiber_a];
for ($round = 0; $round < 3; $round++) {
    foreach ($fibers as $fiber) {
        if ($fiber->isSuspended()) {
            $fiber->resume();
        }
    }
}

echo "All fiber queries completed successfully\n";

mysqli_close($link);
