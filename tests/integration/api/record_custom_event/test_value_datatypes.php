<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests newrelic_record_custom_event() with different parameter array value data
types.
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
        "booleanParam": true,
        "stringParam": "toastIsDelicious",
        "floatParam": 1.25,
        "integerParam": 1
      },
      {}
    ]
  ]
]
*/

newrelic_record_custom_event("testType", array("integerParam"=>1, 
                                               "floatParam"=>1.25, 
                                               "stringParam"=>"toastIsDelicious", 
                                               "booleanParam"=>true));
