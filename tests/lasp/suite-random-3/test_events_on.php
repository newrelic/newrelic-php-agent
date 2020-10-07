<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests newrelic_record_custom_event() sends events when
Language Agent Security Policy (LASP) configuration
indicates custom_events:{enabled:true} and agent is configured to send events
*/

/*INI
newrelic.custom_insights_events.enabled = 1
*/

/*EXPECT_CUSTOM_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": "??",
    "events_seen": "??"
  },
  [
    [
      {
        "type": "testType",
        "timestamp": "??"
      },
      {
        "booleanParam": true,
        "stringParam": "toastIsDelicious",
        "floatParam": "??",
        "integerParam": "??"
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
