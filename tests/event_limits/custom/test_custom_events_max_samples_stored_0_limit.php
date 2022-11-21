<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests newrelic_record_custom_event() and checking that if the 
max samples is set to 0 that no events are sent.  

NOTE: Must be run with -max_custom_events 0 passed to
      the integration runner so it gets the proper
      collector response for this test to pass.

*/

/*INI
newrelic.custom_events.max_samples_stored = 0
newrelic.custom_insights_events.enabled = 1
*/

/*EXPECT_CUSTOM_EVENTS
null
*/

/* create event that should be dropped */
for ($i=0; $i < 3000; $i++) {
  newrelic_record_custom_event("alpha", array("beta"=>$i));
}