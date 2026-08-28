<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that demonstrates 100 fibers nested deep, ensuring proper span hierarchy
and fiber handling in extreme nesting scenarios.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.1", "<")) {
  die("skip: PHP 8.1+ required\n");
}
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
newrelic.fibers.disabled = false
newrelic.span_events.max_samples_stored = 10000
*/

/*EXPECT_METRICS_EXIST
Supportability/PHP/Fiber/used
*/

/*EXPECT_ERROR_EVENTS
null
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
      "type": "Span",
      "traceId": "??",
      "transactionId": "??",
      "sampled": true,
      "priority": "??",
      "name": "Custom\/nested_fiber",
      "guid": "??",
      "timestamp": "??",
      "duration": "??",
      "category": "generic",
      "parentId": "ENV[GUID_FIBER_0]"
    },
    {
        "level": 1
    },
    {}
  ],
  [
    {
      "type": "Span",
      "traceId": "??",
      "transactionId": "??",
      "sampled": true,
      "priority": "??",
      "name": "Custom\/nested_fiber",
      "guid": "??",
      "timestamp": "??",
      "duration": "??",
      "category": "generic",
      "parentId": "ENV[GUID_FIBER_49]"
    },
    {
        "level": 50
    },
    {}
  ],
  [
    {
      "type": "Span",
      "traceId": "??",
      "transactionId": "??",
      "sampled": true,
      "priority": "??",
      "name": "Custom\/nested_fiber",
      "guid": "??",
      "timestamp": "??",
      "duration": "??",
      "category": "generic",
      "parentId": "ENV[GUID_FIBER_99]"
    },
    {
      "level": 100
    },
    {}
  ]
]
*/

/*EXPECT
Starting nested fiber: level 1
Starting nested fiber: level 2
Starting nested fiber: level 3
Starting nested fiber: level 4
Starting nested fiber: level 5
Starting nested fiber: level 6
Starting nested fiber: level 7
Starting nested fiber: level 8
Starting nested fiber: level 9
Starting nested fiber: level 10
Starting nested fiber: level 11
Starting nested fiber: level 12
Starting nested fiber: level 13
Starting nested fiber: level 14
Starting nested fiber: level 15
Starting nested fiber: level 16
Starting nested fiber: level 17
Starting nested fiber: level 18
Starting nested fiber: level 19
Starting nested fiber: level 20
Starting nested fiber: level 21
Starting nested fiber: level 22
Starting nested fiber: level 23
Starting nested fiber: level 24
Starting nested fiber: level 25
Starting nested fiber: level 26
Starting nested fiber: level 27
Starting nested fiber: level 28
Starting nested fiber: level 29
Starting nested fiber: level 30
Starting nested fiber: level 31
Starting nested fiber: level 32
Starting nested fiber: level 33
Starting nested fiber: level 34
Starting nested fiber: level 35
Starting nested fiber: level 36
Starting nested fiber: level 37
Starting nested fiber: level 38
Starting nested fiber: level 39
Starting nested fiber: level 40
Starting nested fiber: level 41
Starting nested fiber: level 42
Starting nested fiber: level 43
Starting nested fiber: level 44
Starting nested fiber: level 45
Starting nested fiber: level 46
Starting nested fiber: level 47
Starting nested fiber: level 48
Starting nested fiber: level 49
Starting nested fiber: level 50
Starting nested fiber: level 51
Starting nested fiber: level 52
Starting nested fiber: level 53
Starting nested fiber: level 54
Starting nested fiber: level 55
Starting nested fiber: level 56
Starting nested fiber: level 57
Starting nested fiber: level 58
Starting nested fiber: level 59
Starting nested fiber: level 60
Starting nested fiber: level 61
Starting nested fiber: level 62
Starting nested fiber: level 63
Starting nested fiber: level 64
Starting nested fiber: level 65
Starting nested fiber: level 66
Starting nested fiber: level 67
Starting nested fiber: level 68
Starting nested fiber: level 69
Starting nested fiber: level 70
Starting nested fiber: level 71
Starting nested fiber: level 72
Starting nested fiber: level 73
Starting nested fiber: level 74
Starting nested fiber: level 75
Starting nested fiber: level 76
Starting nested fiber: level 77
Starting nested fiber: level 78
Starting nested fiber: level 79
Starting nested fiber: level 80
Starting nested fiber: level 81
Starting nested fiber: level 82
Starting nested fiber: level 83
Starting nested fiber: level 84
Starting nested fiber: level 85
Starting nested fiber: level 86
Starting nested fiber: level 87
Starting nested fiber: level 88
Starting nested fiber: level 89
Starting nested fiber: level 90
Starting nested fiber: level 91
Starting nested fiber: level 92
Starting nested fiber: level 93
Starting nested fiber: level 94
Starting nested fiber: level 95
Starting nested fiber: level 96
Starting nested fiber: level 97
Starting nested fiber: level 98
Starting nested fiber: level 99
Starting nested fiber: level 100
Reached maximum nesting depth: 100
Ending nested fiber: level 100
Ending nested fiber: level 99
Ending nested fiber: level 98
Ending nested fiber: level 97
Ending nested fiber: level 96
Ending nested fiber: level 95
Ending nested fiber: level 94
Ending nested fiber: level 93
Ending nested fiber: level 92
Ending nested fiber: level 91
Ending nested fiber: level 90
Ending nested fiber: level 89
Ending nested fiber: level 88
Ending nested fiber: level 87
Ending nested fiber: level 86
Ending nested fiber: level 85
Ending nested fiber: level 84
Ending nested fiber: level 83
Ending nested fiber: level 82
Ending nested fiber: level 81
Ending nested fiber: level 80
Ending nested fiber: level 79
Ending nested fiber: level 78
Ending nested fiber: level 77
Ending nested fiber: level 76
Ending nested fiber: level 75
Ending nested fiber: level 74
Ending nested fiber: level 73
Ending nested fiber: level 72
Ending nested fiber: level 71
Ending nested fiber: level 70
Ending nested fiber: level 69
Ending nested fiber: level 68
Ending nested fiber: level 67
Ending nested fiber: level 66
Ending nested fiber: level 65
Ending nested fiber: level 64
Ending nested fiber: level 63
Ending nested fiber: level 62
Ending nested fiber: level 61
Ending nested fiber: level 60
Ending nested fiber: level 59
Ending nested fiber: level 58
Ending nested fiber: level 57
Ending nested fiber: level 56
Ending nested fiber: level 55
Ending nested fiber: level 54
Ending nested fiber: level 53
Ending nested fiber: level 52
Ending nested fiber: level 51
Ending nested fiber: level 50
Ending nested fiber: level 49
Ending nested fiber: level 48
Ending nested fiber: level 47
Ending nested fiber: level 46
Ending nested fiber: level 45
Ending nested fiber: level 44
Ending nested fiber: level 43
Ending nested fiber: level 42
Ending nested fiber: level 41
Ending nested fiber: level 40
Ending nested fiber: level 39
Ending nested fiber: level 38
Ending nested fiber: level 37
Ending nested fiber: level 36
Ending nested fiber: level 35
Ending nested fiber: level 34
Ending nested fiber: level 33
Ending nested fiber: level 32
Ending nested fiber: level 31
Ending nested fiber: level 30
Ending nested fiber: level 29
Ending nested fiber: level 28
Ending nested fiber: level 27
Ending nested fiber: level 26
Ending nested fiber: level 25
Ending nested fiber: level 24
Ending nested fiber: level 23
Ending nested fiber: level 22
Ending nested fiber: level 21
Ending nested fiber: level 20
Ending nested fiber: level 19
Ending nested fiber: level 18
Ending nested fiber: level 17
Ending nested fiber: level 16
Ending nested fiber: level 15
Ending nested fiber: level 14
Ending nested fiber: level 13
Ending nested fiber: level 12
Ending nested fiber: level 11
Ending nested fiber: level 10
Ending nested fiber: level 9
Ending nested fiber: level 8
Ending nested fiber: level 7
Ending nested fiber: level 6
Ending nested fiber: level 5
Ending nested fiber: level 4
Ending nested fiber: level 3
Ending nested fiber: level 2
Ending nested fiber: level 1
Nested fiber test completed successfully
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');

env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

function nested_fiber($level) {
    echo "Starting nested fiber: level $level\n";

    newrelic_add_custom_span_parameter("level", $level);

    // Base case: we've reached maximum depth
    if ($level >= 100) {
        echo "Reached maximum nesting depth: 100\n";
        Fiber::suspend();
        echo "Ending nested fiber: level $level\n";
        return $level;
    }

    // Create the next nested fiber
    $nested_fiber = new Fiber(function() use ($level) {
          // Store GUIDs for key levels for span event verification
        env_var_for_expects("GUID_FIBER_" . $level, newrelic_get_linking_metadata()['span.id'] ?? '');

        return nested_fiber($level + 1);
    });

    // Suspend before starting the nested fiber
    Fiber::suspend();

    // Start and wait for the nested fiber
    $result = $nested_fiber->start();

    // Resume the nested fiber if it's suspended
    if ($nested_fiber->isSuspended()) {
        $result = $nested_fiber->resume();
    }

    echo "Ending nested fiber: level $level\n";
    return $result;
}

// Start the nested fiber chain
$main_fiber = new Fiber(function() {
    env_var_for_expects("GUID_FIBER_0", newrelic_get_linking_metadata()['span.id'] ?? '');
    return nested_fiber(1);
});

$main_fiber->start();

// Resume the main fiber to trigger all nested fiber execution
while ($main_fiber->isSuspended()) {
    $main_fiber->resume();
}

echo "Nested fiber test completed successfully\n";
