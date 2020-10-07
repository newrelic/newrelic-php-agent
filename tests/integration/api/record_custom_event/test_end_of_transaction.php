<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that the newrelic_end_of_transaction() call keeps custom events from being
recorded.
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
        "type":"testType",
        "timestamp":"??"
      },
      {
        "testKey":"testValue1"
      },
      {}
    ]
  ]
]
*/

newrelic_record_custom_event("testType", array("testKey"=>"testValue1"));
newrelic_end_of_transaction();
newrelic_record_custom_event("testType", array("testKey"=>"testValue2"));
