<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that adding custom tracers with strange names does not blow up.
*/

/*INI
newrelic.code_level_metrics.enabled = true
*/

/*EXPECT
zip
zap
*/

/*EXPECT_METRICS_EXIST
Custom/MY_function, 1
Custom/MY_class::MY_method, 1
Supportability/api/add_custom_tracer, 4
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/MY_function",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {
        "code.lineno": "??",
        "code.filepath": "__FILE__",
        "code.function": "MY_function"
      }
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/MY_class::MY_method",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {
        "code.lineno": "??",
        "code.namespace": "MY_class",
        "code.filepath": "__FILE__",
        "code.function": "MY_method"
      }
    ]
]
*/

/*EXPECT_TRACED_ERRORS null*/

function MY_function($x) {
    echo $x;
}

class MY_class {
    public static function MY_method($x) {
        echo $x;
    }
}

/*
 * Note that capitalization has been changed to test case insensitive lookup.
 *
 * Note that the metrics contain the capitalization of the actual code, not
 * the parameters we are given.
 */
newrelic_add_custom_tracer("my_FUNCTION");
newrelic_add_custom_tracer("my_CLASS::my_METHOD");
newrelic_add_custom_tracer("$)O#()@::@)@)@@");
newrelic_add_custom_tracer("a::b::c");

MY_function("zip\n");
MY_class::MY_method("zap\n");
