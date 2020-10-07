<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests newrelic_record_custom_event() with NAN values as type, parameter array
key, and parameter array value.

Type of NAN dutifully reports the string "NAN" as the type, while NAN keys and
values just get ignored, and an empty event is sent.
*/

/*INI
newrelic.custom_insights_events.enabled = 1
*/

/*EXPECT_CUSTOM_EVENTS
[
  "?? agent run id",
  "?? sampling information",
  [
    [
      {
        "type": "NAN",
        "timestamp": "??"
      },
      {
        "testKey": "testValue"
      },
      {
      }
    ],
    [
      {
        "type": "testType",
        "timestamp": "??"
      },
      {
      },
      {
      }
    ],
    [
      {
        "type": "testType",
        "timestamp": "??"
      },
      {
      },
      {
      }
    ]
  ]
]
*/

newrelic_record_custom_event(NAN, array("testKey"=>"testValue"));
newrelic_record_custom_event("testType", array(NAN=>"testValue"));
newrelic_record_custom_event("testType", array("testKey"=>NAN));
