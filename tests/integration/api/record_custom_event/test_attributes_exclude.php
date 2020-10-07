<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Makes sure the newrelic.attributes.exclude is ignored by
newrelic_record_custom_event().
*/

/*INI
newrelic.attributes.exclude=testAttribute
newrelic.custom_insights_events.enabled = 1
*/

/*EXPECT_CUSTOM_EVENTS
[
  "?? agent run id",
  "?? sampling information",
  [
    [
      {
        "type": "testAttribute",
        "timestamp": "??"
      },
      {
        "testAttribute": "testAttribute"
      },
      {
      }
    ]
  ]
]
*/

newrelic_record_custom_event("testAttribute", array("testAttribute"=>"testAttribute"));
