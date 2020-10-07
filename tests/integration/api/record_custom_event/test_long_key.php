<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests newrelic_record_custom_event() with a long parameter array key. Looks like
we discard the parameters, but record the event, printing the first 128
characters in the php_agent.log file:

warning: potential attribute discarded: key '01234567890123456789012345678901234
56789012345678901234567890123456789012345678901234567890123456789012345678901234
5678901234567' exceeds size limit 256
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
      {},
      {}
    ]
  ]
]
*/

newrelic_record_custom_event("testType", array("012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"=>"testValue"));
