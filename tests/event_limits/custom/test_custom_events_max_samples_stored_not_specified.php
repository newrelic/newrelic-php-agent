<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests newrelic_record_custom_event() and checking that if the 
max samples is not specified the default of 30k will be used
and that no more than that are sent.  

The actual test suggests spreading the events over a minute
but the integration runner cannot test more than a single harvest.
So this test simply sends the events in a burst and verifies
the number in the harvest matches what is expected.

NOTE: Must be run with -max_custom_events 30000 passed to
      the integration runner so it gets the proper
      collector response for this test to pass.

*/

/*INI
newrelic.custom_insights_events.enabled = 1
*/

/*EXPECT_CUSTOM_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": "??",
    "events_seen": 2500
  },
  "??"
]
*/

/* 30000 custom events/min = 2500 custom events/fast harvest (5s) */
/* create more than 2500 and see that only 2500 are harvested */
for ($i=0; $i < 3000; $i++) {
  newrelic_record_custom_event("alpha", array("beta"=>$i));
}