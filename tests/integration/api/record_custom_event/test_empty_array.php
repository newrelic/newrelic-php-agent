<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests newrelic_record_custom_event() with an empty array of attributes. This
shouldn't work.
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
        "type": "alpha",
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

newrelic_record_custom_event("alpha", array());
