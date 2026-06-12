<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should generate multiple explain plans when queries are executed
in multiple nested fibers, demonstrating complex fiber hierarchies with database operations.
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
Level 1 fiber: STATISTICS
Level 2 fiber: TABLES
Level 3 fiber: COLUMNS
Level 4 fiber: KEY_COLUMN_USAGE
Level 5 fiber: TABLE_CONSTRAINTS
Nested fiber query execution completed
*/

/*EXPECT_METRICS_EXIST
Datastore/all, 5
Datastore/allOther, 5
Datastore/MySQL/all, 5
Datastore/MySQL/allOther, 5
Datastore/statement/MySQL/tables/select, 5
Datastore/operation/MySQL/select, 5
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
      "guid": "ENV[GUID_NESTED_FIBER_1]",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/execute_nested_query",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_MAIN_FIBER]"
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
      "parentId": "ENV[GUID_NESTED_FIBER_1]",
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
      "guid": "ENV[GUID_NESTED_FIBER_2]",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/execute_nested_query",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_NESTED_FIBER_CLOSURE_1]"
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
      "parentId": "ENV[GUID_NESTED_FIBER_2]",
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
      "guid": "ENV[GUID_NESTED_FIBER_3]",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/execute_nested_query",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_NESTED_FIBER_CLOSURE_2]"
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
      "parentId": "ENV[GUID_NESTED_FIBER_3]",
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
      "guid": "ENV[GUID_NESTED_FIBER_4]",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/execute_nested_query",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_NESTED_FIBER_CLOSURE_3]"
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
      "parentId": "ENV[GUID_NESTED_FIBER_4]",
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
      "guid": "ENV[GUID_NESTED_FIBER_5]",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/execute_nested_query",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_NESTED_FIBER_CLOSURE_4]"
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
      "parentId": "ENV[GUID_NESTED_FIBER_5]",
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
      5,
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
          " in execute_nested_query called at __FILE__ (??)",
          " in {closure} called at ? (?)",
          " in Fiber::resume called at __FILE__ (??)",
          " in execute_nested_query called at __FILE__ (??)",
          " in {closure} called at ? (?)",
          " in Fiber::resume called at __FILE__ (??)",
          " in execute_nested_query called at __FILE__ (??)",
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

function execute_nested_query($link, $level, $max_level)
{
    // Capture GUID for first level for span event verification
    env_var_for_expects("GUID_NESTED_FIBER_" . $level, newrelic_get_linking_metadata()['span.id'] ?? '');


    // Define different queries for each level
    $queries = [
        1 => [
            'query' => "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?",
            'param' => 'STATISTICS',
            'column' => 'TABLE_NAME'
        ],
        2 => [
            'query' => "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?",
            'param' => 'TABLES',
            'column' => 'TABLE_NAME'
        ],
        3 => [
            'query' => "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?",
            'param' => 'COLUMNS',
            'column' => 'TABLE_NAME'
        ],
        4 => [
            'query' => "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?",
            'param' => 'KEY_COLUMN_USAGE',
            'column' => 'TABLE_NAME'
        ],
        5 => [
            'query' => "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?",
            'param' => 'TABLE_CONSTRAINTS',
            'column' => 'TABLE_NAME'
        ]
    ];

    if ($level <= $max_level && isset($queries[$level])) {
        $query_info = $queries[$level];

        // Prepare and execute query for this level
        $stmt = mysqli_prepare($link, $query_info['query']);
        if (FALSE === $stmt) {
            echo "Level $level prepare error: " . mysqli_error($link) . "\n";
            return;
        }

        if (FALSE === mysqli_stmt_bind_param($stmt, 's', $query_info['param'])) {
            echo "Level $level bind error: " . mysqli_stmt_error($stmt) . "\n";
            return;
        }

        // Suspend fiber before query execution
        Fiber::suspend();

        if (FALSE === mysqli_stmt_execute($stmt)) {
            echo "Level $level execute error: " . mysqli_stmt_error($stmt) . "\n";
            return;
        }

        if (FALSE === mysqli_stmt_bind_result($stmt, $result)) {
            echo "Level $level bind result error: " . mysqli_stmt_error($stmt) . "\n";
            return;
        }

        // Display results for this level
        if (mysqli_stmt_fetch($stmt)) {
            echo "Level $level fiber: " . $result . "\n";
        }

        mysqli_stmt_close($stmt);

        // If we haven't reached max level, create nested fiber
        if ($level < $max_level) {
            $nested_fiber = new Fiber(function() use ($link, $level, $max_level) {
                env_var_for_expects("GUID_NESTED_FIBER_CLOSURE_" . $level, newrelic_get_linking_metadata()['span.id'] ?? '');
                return execute_nested_query($link, $level + 1, $max_level);
            });

            // Start nested fiber
            $nested_fiber->start();

            // Resume nested fiber immediately if suspended
            if ($nested_fiber->isSuspended()) {
                $nested_fiber->resume();
            }
        }
    }
}

// Create database connection
$link = mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if (mysqli_connect_errno()) {
    echo mysqli_connect_error() . "\n";
    exit(1);
}

// Create main fiber that starts the nested chain
$main_fiber = new Fiber(function() use ($link) {
    env_var_for_expects("GUID_MAIN_FIBER", newrelic_get_linking_metadata()['span.id'] ?? '');
    return execute_nested_query($link, 1, 5);
});

// Start the main fiber
$main_fiber->start();

// Resume the main fiber chain - keep resuming until complete
while ($main_fiber->isSuspended()) {
    $main_fiber->resume();
}

echo "Nested fiber query execution completed\n";

mysqli_close($link);
