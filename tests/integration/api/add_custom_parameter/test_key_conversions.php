<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests how the agent converts custom parameter keys to strings.
*/

/*INI
newrelic.cross_application_tracer.enabled = 0
newrelic.distributed_tracing_enabled=0
*/

/*EXPECT_ANALYTICS_EVENTS
 [
  "?? agent run id",
  "?? sampling information",
  [
    [
      {
        "type": "Transaction",
        "name": "OtherTransaction/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "totalTime": "??",
        "error": false
      },
      {
        "foo": "IS_STRING",
        "(Resource)": "IS_RESOURCE",
        "(Object)": "IS_OBJECT",
        "(null)": "IS_NULL",
        "42": "IS_LONG",
        "42.00000": "IS_DOUBLE",
        "True": "IS_BOOL",
        "(Array)": "IS_ARRAY"
      },
      {
      }
    ]
  ]
]
*/

class MyClass {}

function test_add_custom_parameters() {
  newrelic_add_custom_parameter(array(),                  "IS_ARRAY");
  newrelic_add_custom_parameter(true,                     "IS_BOOL");
  newrelic_add_custom_parameter(42.0,                     "IS_DOUBLE");
  newrelic_add_custom_parameter(42,                       "IS_LONG");
  newrelic_add_custom_parameter(null,                     "IS_NULL");
  newrelic_add_custom_parameter(new MyClass(),            "IS_OBJECT");
  newrelic_add_custom_parameter(fopen('php://temp', 'r'), "IS_RESOURCE");
  newrelic_add_custom_parameter("foo",                    "IS_STRING");
}

test_add_custom_parameters();
