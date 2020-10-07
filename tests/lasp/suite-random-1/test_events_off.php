<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that nothing in the LASP code interferes with turning the custom events
configuration off.
*/

/*INI
newrelic.custom_insights_events.enabled = 0
*/

/*EXPECT_CUSTOM_EVENTS
null
*/

newrelic_record_custom_event("testType", array("integerParam"=>1,
                                               "floatParam"=>1.25,
                                               "stringParam"=>"toastIsDelicious",
                                               "booleanParam"=>true));
