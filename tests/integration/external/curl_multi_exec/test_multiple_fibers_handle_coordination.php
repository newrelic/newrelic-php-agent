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

Ensuring parenting is a little tricky.
Since the agent creates an on the fly segment to parent curl_multi_exec calls, we cannot directly access the 
particular span_id of each curl_multi_exec segment to ensure the parenting in the EXPECT blocks.
Additionally, the transaction tracer functionality doesn't record any span attributes, it is Transaction only,
so we can't pull any span_ids out of that to correlate. Here's what we'll do.
1. Using EXPECT ensure there is a distinct curl_multi_exec that corresponds to each specific fiber that
has unique function names associated with each fiber.
2. Create specific function names to correspond to each curl_multi_exec fiber execution
3. Using the Transaction trace, verify the distinct urls names called by curl_multi_exec that was called by the expected
 function name for that fiber.
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
found segment name: Custom/executing_fiber_6
ok - executing_fiber_6 should have exactly ONE child.
ok - executing_fiber_6 should have a child named 'Custom/executing_fiber_inner'.
ok - executing_fiber_6 should have exactly ONE curl_exec_multi child.
ok - executing_fiber_6 should have match: 3 equal to total count: 3.
found segment name: Custom/executing_fiber_7
ok - executing_fiber_7 should have exactly ONE child.
ok - executing_fiber_7 should have a child named 'Custom/executing_fiber_inner'.
ok - executing_fiber_7 should have exactly ONE curl_exec_multi child.
ok - executing_fiber_7 should have match: 3 equal to total count: 3.
found segment name: Custom/executing_fiber_8
ok - executing_fiber_8 should have exactly ONE child.
ok - executing_fiber_8 should have a child named 'Custom/executing_fiber_inner'.
ok - executing_fiber_8 should have exactly ONE curl_exec_multi child.
ok - executing_fiber_8 should have match: 3 equal to total count: 3.
found segment name: Custom/executing_fiber_9
ok - executing_fiber_9 should have exactly ONE child.
ok - executing_fiber_9 should have a child named 'Custom/executing_fiber_inner'.
ok - executing_fiber_9 should have exactly ONE curl_exec_multi child.
ok - executing_fiber_9 should have match: 3 equal to total count: 3.
found segment name: Custom/executing_fiber_10
ok - executing_fiber_10 should have exactly ONE child.
ok - executing_fiber_10 should have a child named 'Custom/executing_fiber_inner'.
ok - executing_fiber_10 should have exactly ONE curl_exec_multi child.
ok - executing_fiber_10 should have match: 3 equal to total count: 3.
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
        "name": "Custom\/executing_fiber_6",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {}
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "ENV[GUID_FIBER_EXEC_INNER_6]",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/executing_fiber_inner",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_FIBER_EXEC_6]"
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
        "guid": "ENV[GUID_FLUFF_FUNC_6]",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/fluff_func",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_FIBER_EXEC_INNER_6]"
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
        "parentId": "ENV[GUID_FIBER_EXEC_INNER_6]"
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
        "name": "Custom\/executing_fiber_7",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {}
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "ENV[GUID_FIBER_EXEC_INNER_7]",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/executing_fiber_inner",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_FIBER_EXEC_7]"
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
        "guid": "ENV[GUID_FLUFF_FUNC_7]",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/fluff_func",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_FIBER_EXEC_INNER_7]"
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
        "parentId": "ENV[GUID_FIBER_EXEC_INNER_7]"
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
        "name": "Custom\/executing_fiber_8",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {}
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "ENV[GUID_FIBER_EXEC_INNER_8]",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/executing_fiber_inner",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_FIBER_EXEC_8]"
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
        "guid": "ENV[GUID_FLUFF_FUNC_8]",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/fluff_func",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_FIBER_EXEC_INNER_8]"
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
        "parentId": "ENV[GUID_FIBER_EXEC_INNER_8]"
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
        "name": "Custom\/executing_fiber_9",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {}
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "ENV[GUID_FIBER_EXEC_INNER_9]",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/executing_fiber_inner",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_FIBER_EXEC_9]"
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
        "parentId": "ENV[GUID_FIBER_EXEC_INNER_9]"
      },
      {},
      {}
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "ENV[GUID_FLUFF_FUNC_9]",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/fluff_func",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_FIBER_EXEC_INNER_9]"
      },
      {
        "fiber_id": 9
      },
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
        "name": "Custom\/executing_fiber_10",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {}
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "ENV[GUID_FIBER_EXEC_INNER_10]",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/executing_fiber_inner",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_FIBER_EXEC_10]"
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
        "guid": "ENV[GUID_FLUFF_FUNC_10]",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/fluff_func",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_FIBER_EXEC_INNER_10]"
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
        "parentId": "ENV[GUID_FIBER_EXEC_INNER_10]"
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

function fluff_func($fiber_id) {
    env_var_for_expects("GUID_FLUFF_FUNC_" . $fiber_id, newrelic_get_linking_metadata()['span.id'] ?? '');
    newrelic_add_custom_span_parameter("fiber_id", $fiber_id);
}

function executing_fiber_inner($fiber_id, $handle_set_id) {
    global $multi_handles, $curl_handles, $results;

    echo "Fiber $fiber_id executing curl_multi_exec for handle set $handle_set_id\n";
    env_var_for_expects("GUID_FIBER_EXEC_INNER_" . $fiber_id, newrelic_get_linking_metadata()['span.id'] ?? '');


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
    $first_fluff = true;
    $first_sleep = true;
    do {
        curl_multi_exec($mh, $active);
        if ($info = curl_multi_info_read($mh)) {
            // First time this is not false means we have newly completed handles
            if ($first_fluff) {
            fluff_func($fiber_id);
            $first_fluff = false;
          }
        }
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

// Create some functions for the executing fibers so we can ensure parentage
// Create actual functions in the function table
for ($i = 6; $i <= 10; $i++) {

    $function_code = "
    function executing_fiber_$i(\$fiber_id, \$handle_set_id) {
        env_var_for_expects(\"GUID_FIBER_EXEC_\" . \$fiber_id, newrelic_get_linking_metadata()['span.id'] ?? '');
        return executing_fiber_inner(\$fiber_id, \$handle_set_id);
    }";

    eval($function_code);
}

function test_multiple_fibers_coordination() {
    global $results;
    //global $executing_fiber_functions;

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
        $function_name = "executing_fiber_$i";
        //${$function_name} = function() use ($i, $handle_set_id) {
       // $$function_name = function() use ($i, $handle_set_id) {
         //   return executing_fiber($i, $handle_set_id);
        //};
        //$executing_fibers[$i] = new Fiber($$function_name());
       // echo "function_name:  = $function_name\n";
        //var_dump($executing_fiber_functions[$i]);
        //echo "after vardump: " . $function_name . "\n";
        $executing_fibers[$i] = new Fiber(function() use ($i, $handle_set_id, $function_name) {
            return $function_name($i, $handle_set_id);
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

for ($i = 6; $i <= 10; $i++) {
  $handle_set_id = $i - 5; // Maps fiber 6->handle set 1, fiber 7->handle set 2, etc.
  $executing_fiber_segments = $txn->getTrace()->findSegmentsByName('Custom/executing_fiber_' . $i);
  $count = iterator_count($executing_fiber_segments);
  if ($count != 1) {
      tap_equal(1, $count, "executing_fiber_$i should have exactly ONE segment with this name.");
  }
  foreach ($executing_fiber_segments as $executing_fiber_segment) {
    echo "found segment name: " . $executing_fiber_segment->name . "\n";

    tap_equal(1, count($executing_fiber_segment->children), "executing_fiber_$i should have exactly ONE child.");
    tap_equal(0, strcmp("Custom/executing_fiber_inner", $executing_fiber_segment->children[0]->name), "executing_fiber_$i should have a child named 'Custom/executing_fiber_inner'.");
    $executing_fiber_inner_segment = $executing_fiber_segment->children[0];
    $curl_multi_exec_children_count = 0;
    $executing_fiber_inner_segment_children = $executing_fiber_inner_segment->children;
    foreach ($executing_fiber_inner_segment_children as $executing_fiber_inner_segment_child) {
      if (strcmp("curl_multi_exec", $executing_fiber_inner_segment_child->name) == 0) {
        // Found a curl_multi_exec child segment
        $curl_multi_exec_children_count++;
        tap_equal(1, $curl_multi_exec_children_count, "executing_fiber_$i should have exactly ONE curl_exec_multi child.");
        $curl_multi_exec_children = $executing_fiber_inner_segment_child->children;
        $match_count = 0;
        $total_children = count($curl_multi_exec_children);
        foreach ($curl_multi_exec_children as $child) {
          if (str_starts_with($child->name, 'External/url'. $handle_set_id . '_') && str_ends_with($child->name, "_$i/all")) {
            $match_count++;
          }
        }
        tap_equal($match_count, $total_children, "executing_fiber_$i should have match: $match_count equal to total count: $total_children.");
      }
    } 
  }
}
