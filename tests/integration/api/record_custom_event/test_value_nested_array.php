<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests newrelic_record_custom_event() with a nested array as the value of the
parameters. It shouldn't work as intended, instead returning an empty event.
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
        "type":"testArrayType",
        "timestamp":"??"
      },
      {},
      {}
    ]
  ]
]
*/

newrelic_record_custom_event("testArrayType", 
  array("myparam"=>array("veryNested"=>1,"reallyNested"=>2)));
