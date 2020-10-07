<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent shouldn't touch PHP's default exception handler if the
no_exception_handler special flag is set.
*/

/*INI
newrelic.special = no_exception_handler
*/

/*EXPECT_TRACED_ERRORS
null
*/

/*EXPECT_ERROR_EVENTS
null
*/

function alpha() {
  throw new Exception('Sample Exception');
}

function beta() {
  alpha();
}

function gamma($password) {
  beta();
}

gamma('my super secret password that New Relic cannot know');
