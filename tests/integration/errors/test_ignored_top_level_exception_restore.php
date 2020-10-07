<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should not report errors for exceptions that bubble to the top level
even when the exception handler stack is altered and a previous exception
handler is called if it's set to ignore them.
*/

/*INI
newrelic.error_collector.ignore_user_exception_handler = 1
*/

/*EXPECT
In first exception handler
In second exception handler
*/

/*EXPECT_TRACED_ERRORS
null
*/

function first($ex) {
  echo "In first exception handler\n";
}

function second($ex) {
  echo "In second exception handler\n";
}

set_exception_handler('first');
set_exception_handler('second');
set_exception_handler('first');
restore_exception_handler();

function throw_it() {
  throw new RuntimeException('Hi!');
}

first(null);
throw_it();
