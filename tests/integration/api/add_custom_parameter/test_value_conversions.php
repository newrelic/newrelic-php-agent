<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests how the agent converts custom parameter values to strings.
*/

/*INI
newrelic.cross_application_tracer.enabled = 0
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
        "error": true
      },
      {
        "IS_STRING": "foo",
        "IS_NULL": null,
        "IS_LONG": 42,
        "IS_DOUBLE": 42.0,
        "IS_BOOL": true
      },
      {
        "errorType": "E_WARNING",
        "errorMessage": "/newrelic_add_custom_parameter.*expects parameter to be scalar, resource given/"
      }
    ]
  ]
]
*/

class MyClass {};

function test_add_custom_parameters() {
  /*
   * Note that the order below is significant: the last failing
   * newrelic_add_custom_parameter call will be the one that is used for the
   * errorMessage above. At present, the last failure is the IS_RESOURCE case,
   * so the above message refers to the resource type.
   */

  newrelic_add_custom_parameter("IS_ARRAY", array());
  newrelic_add_custom_parameter("IS_BOOL", true);
  newrelic_add_custom_parameter("IS_DOUBLE", 42.0);
  newrelic_add_custom_parameter("IS_LONG", 42);
  newrelic_add_custom_parameter("IS_NULL", null);
  newrelic_add_custom_parameter("IS_OBJECT", new MyClass());
  newrelic_add_custom_parameter("IS_RESOURCE", curl_init());
  newrelic_add_custom_parameter("IS_STRING", "foo");
}

test_add_custom_parameters();
