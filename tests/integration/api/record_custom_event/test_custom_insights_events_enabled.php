<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Makes sure the newrelic.custom_insights_events.enabled .ini configuration works.
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
        "type": "testType",
        "timestamp": "??"
      },
      {
        "testKey": "testValue"
      },
      {
      }
    ]
  ]
]
*/

newrelic_record_custom_event("testType", array("testKey"=>"testValue"));
