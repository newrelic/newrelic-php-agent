<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should not report errors for exceptions that bubble to the top level
and are handled by a named function if it's set to ignore them.
*/

/*INI
newrelic.error_collector.ignore_user_exception_handler = 1
*/

/*EXPECT
In exception handler
*/

/*EXPECT_TRACED_ERRORS
null
*/

function handler($ex) {
  echo "In exception handler\n";
}

set_exception_handler('handler');

function throw_it() {
  throw new RuntimeException('Hi!');
}

throw_it();
