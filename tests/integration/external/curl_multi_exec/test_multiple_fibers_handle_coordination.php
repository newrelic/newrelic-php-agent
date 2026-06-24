<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that curl_multi_exec works correctly when 5 fibers add 3 curl handles each and 5 other fibers execute curl_multi_exec,
demonstrating complex fiber coordination with HTTP requests.
Fiber 6 should execute curl_multi_exec for handle set 1 (3 handles)
Fiber 7 should execute curl_multi_exec for handle set 2 (3 handles)
Fiber 8 should execute curl_multi_exec for handle set 3 (3 handles)
Fiber 9 should execute curl_multi_exec for handle set 4 (3 handles)
Fiber 10 should execute curl_multi_exec for handle set 5 (3 handles)
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '8.1', '<')) {
    die("skip: PHP >= 8.1 required\n");
}
if (!extension_loaded("curl")) {
    die("skip: curl extension required\n");
}
?>
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.cross_application_tracer.enabled = false
newrelic.fibers.disabled = false
*/

/*EXPECT
Fiber 1 adding curl handles
Fiber 2 adding curl handles
Fiber 3 adding curl handles
Fiber 4 adding curl handles
Fiber 5 adding curl handles
Fiber 6 executing curl_multi_exec for handle set 1
Fiber 7 executing curl_multi_exec for handle set 2
Fiber 8 executing curl_multi_exec for handle set 3
Fiber 9 executing curl_multi_exec for handle set 4
Fiber 10 executing curl_multi_exec for handle set 5
Handle set 1 result: 
Handle set 2 result: 
Handle set 3 result: 
Handle set 4 result: 
Handle set 5 result: 
All curl operations completed successfully
Fiber had 3 matching children
Fiber had 3 matching children
Fiber had 3 matching children
Fiber had 3 matching children
Fiber had 3 matching children
*/

/*EXPECT_METRICS_EXIST
External/url1_1_6/all, 1
External/url1_2_6/all, 1
External/url1_3_6/all, 1
External/url2_1_7/all, 1
External/url2_2_7/all, 1
External/url2_3_7/all, 1
External/url3_1_8/all, 1
External/url3_2_8/all, 1
External/url3_3_8/all, 1
External/url4_1_9/all, 1
External/url4_2_9/all, 1
External/url4_3_9/all, 1
External/url5_1_10/all, 1
External/url5_2_10/all, 1
External/url5_3_10/all, 1
*/

/*EXPECT_ERROR_EVENTS
null
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "ENV[GUID_FIBER_EXEC_6]",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/executing_fiber",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {
        "fiber_id": 6
      },
      {}
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "curl_multi_exec",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_FIBER_EXEC_6]"
      },
      {},
      {}
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url1_1_6\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "curl"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "http:\/\/url1_1_6\/",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url1_2_6\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "curl"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "http:\/\/url1_2_6\/",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url1_3_6\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "curl"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "http:\/\/url1_3_6\/",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "ENV[GUID_FIBER_EXEC_7]",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/executing_fiber",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {
        "fiber_id": 7
      },
      {}
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "curl_multi_exec",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_FIBER_EXEC_7]"
      },
      {},
      {}
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url2_1_7\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "curl"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "http:\/\/url2_1_7\/",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url2_2_7\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "curl"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "http:\/\/url2_2_7\/",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url2_3_7\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "curl"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "http:\/\/url2_3_7\/",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "ENV[GUID_FIBER_EXEC_8]",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/executing_fiber",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {
        "fiber_id": 8
      },
      {}
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "curl_multi_exec",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_FIBER_EXEC_8]"
      },
      {},
      {}
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url3_1_8\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "curl"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "http:\/\/url3_1_8\/",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url3_2_8\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "curl"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "http:\/\/url3_2_8\/",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url3_3_8\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "curl"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "http:\/\/url3_3_8\/",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "ENV[GUID_FIBER_EXEC_9]",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/executing_fiber",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {
        "fiber_id": 9
      },
      {}
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "curl_multi_exec",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_FIBER_EXEC_9]"
      },
      {},
      {}
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url4_1_9\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "curl"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "http:\/\/url4_1_9\/",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url4_2_9\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "curl"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "http:\/\/url4_2_9\/",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url4_3_9\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "curl"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "http:\/\/url4_3_9\/",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "ENV[GUID_FIBER_EXEC_10]",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/executing_fiber",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {
        "fiber_id": 10
      },
      {}
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "curl_multi_exec",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_FIBER_EXEC_10]"
      },
      {},
      {}
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url5_1_10\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "curl"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "http:\/\/url5_1_10\/",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url5_2_10\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "curl"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "http:\/\/url5_2_10\/",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url5_3_10\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "curl"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "http:\/\/url5_3_10\/",
        "http.statusCode": 0
      }
    ]
]    
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php'); 
require_once(realpath(dirname(__FILE__)) . '/../../../include/integration.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');


use NewRelic\Integration\Transaction;

// Global storage for multi handles and results
$multi_handles = [];
$curl_handles = [];
$results = [];

function handle_adding_fiber($fiber_id) {
    global $multi_handles, $curl_handles;

    echo "Fiber $fiber_id adding curl handles\n";

    // Create multi handle
    $mh = curl_multi_init();
    $handles = [];

    // Create 3 curl handles
    for ($i = 1; $i <= 3; $i++) {
        $url = "url" . $fiber_id . "_" . $i;
        $ch = curl_init($url);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
        curl_multi_add_handle($mh, $ch);
        $handles[] = $ch;
    }

    // Store for the executing fiber to use
    $multi_handles[$fiber_id] = $mh;
    $curl_handles[$fiber_id] = $handles;

    Fiber::suspend();

    return $mh;
}

function executing_fiber($fiber_id, $handle_set_id) {
    global $multi_handles, $curl_handles, $results;

    echo "Fiber $fiber_id executing curl_multi_exec for handle set $handle_set_id\n";
    env_var_for_expects("GUID_FIBER_EXEC_" . $fiber_id, newrelic_get_linking_metadata()['span.id'] ?? '');
    newrelic_add_custom_span_parameter("fiber_id", $fiber_id);


    // Wait for the handle set to be available
    while (!isset($multi_handles[$handle_set_id])) {
        Fiber::suspend();
    }

    $mh = $multi_handles[$handle_set_id];
    $handles = $curl_handles[$handle_set_id];

    // Update URLs for all handles to include the executing fiber ID
    foreach ($handles as $i => $ch) {
        $current_url = curl_getinfo($ch, CURLINFO_EFFECTIVE_URL);
        curl_setopt($ch, CURLOPT_URL, $current_url . "_" . $fiber_id);
    }

    Fiber::suspend();

    // Execute the curl_multi_exec
    $active = 0;
    do {
        curl_multi_exec($mh, $active);
        Fiber::suspend($active);
    } while ($active > 0);

    // Get the results - just take the first handle's result for simplicity
    try {
        $content = curl_multi_getcontent($handles[0]);
        $results[$handle_set_id] = trim($content);
    } catch (Exception $e) {
        $results[$handle_set_id] = "Error: " . $e->getMessage();
    }

    // Clean up all handles
    foreach ($handles as $ch) {
        curl_multi_remove_handle($mh, $ch);
    }
    curl_multi_close($mh);

    Fiber::suspend();
}

function test_multiple_fibers_coordination() {
    global $results;

    // Create 5 handle-adding fibers
    $adding_fibers = [];
    for ($i = 1; $i <= 5; $i++) {
        $adding_fibers[$i] = new Fiber(function() use ($i) {
            return handle_adding_fiber($i);
        });
    }

    // Create 5 executing fibers
    $executing_fibers = [];
    for ($i = 6; $i <= 10; $i++) {
        $handle_set_id = $i - 5; // Maps fiber 6->handle set 1, fiber 7->handle set 2, etc.
        $executing_fibers[$i] = new Fiber(function() use ($i, $handle_set_id) {
            return executing_fiber($i, $handle_set_id);
        });
    }

    // Start all adding fibers
    foreach ($adding_fibers as $fiber) {
        $fiber->start();
    }

    // Start all executing fibers
    foreach ($executing_fibers as $fiber) {
        $fiber->start();
    }

    // Resume adding fibers to let them create handles
    foreach ($adding_fibers as $fiber) {
        if (!$fiber->isTerminated()) {
            $fiber->resume();
        }
    }

    // Resume executing fibers multiple times to let them process
    for ($round = 0; $round < 10; $round++) {
        foreach ($executing_fibers as $fiber) {
            if (!$fiber->isTerminated()) {
                $result = $fiber->resume();
                // Keep resuming while curl is active
                while (isset($result) && $result > 0 && !$fiber->isTerminated()) {
                    $result = $fiber->resume();
                }
            }
        }

        // Give adding fibers a chance if they're still running
        foreach ($adding_fibers as $fiber) {
            if (!$fiber->isTerminated()) {
                $fiber->resume();
            }
        }
    }

    // Wait a bit for any remaining async operations to complete
    usleep(100000); // 100ms

    // Final cleanup round
    foreach ($executing_fibers as $fiber) {
        if (!$fiber->isTerminated()) {
            $fiber->resume();
        }
    }

    // Print results
    for ($i = 1; $i <= 5; $i++) {
        if (isset($results[$i])) {
            echo "Handle set $i result: " . $results[$i] . "\n";
        } else {
            echo "Handle set $i result: No result available\n";
        }
    }

    echo "All curl operations completed successfully\n";
}

$main_fiber = new Fiber('test_multiple_fibers_coordination');
$main_fiber->start();

$txn = new Transaction;
$curl_multi_execs = $txn->getTrace()->findSegmentsByName('curl_multi_exec');
foreach ($curl_multi_execs as $segment) {
    $matching_children = true;
    $children = $segment->children;
    $child_index = 0;
    $prev_child_name = '';
      foreach ($children as $child) {
        if (!empty($prev_child_name) && strncmp($prev_child_name, $child->name, 5) != 0) {
          $matching_children = false;
        }
        $child_index++;
    }
    echo "Fiber had $child_index " . ($matching_children ? "matching" : "NOT matching") . " children\n";
}
