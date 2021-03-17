<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should not capture and report errors configured via the
newrelic.error_collector.ignore_errors setting.
*/

/*INI
error_reporting = E_ALL | E_STRICT
newrelic.error_collector.ignore_errors = E_WARNING
display_errors=1
log_errors=0
*/

/*EXPECT_REGEX
^\s*(PHP )?Warning:\s*session_gc\(\):.*? on line [0-9]+\s*$
*/

/*EXPECT_TRACED_ERRORS
null
*/

/*EXPECT_ERROR_EVENTS
null
*/

function run_test() {
  session_gc();
}

run_test();
