<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should not capture and report runtime notices.
*/

/*INI
error_reporting = E_ALL | E_STRICT
display_errors=1
log_errors=0
*/

/*EXPECT_REGEX
^\s*(PHP )?Notice:\s*Sample E_USER_NOTICE in .*? on line [0-9]+\s*$
*/

/*EXPECT_TRACED_ERRORS
null
*/

/*EXPECT_ERROR_EVENTS
null
*/

trigger_error("Sample E_USER_NOTICE", E_USER_NOTICE);
