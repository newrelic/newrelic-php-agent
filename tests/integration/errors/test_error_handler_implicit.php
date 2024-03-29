<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should NOT capture and report error types  
when a custom error handler exists but it returns true for that error type.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.0", "<")) {
  die("skip: PHP < 7.0.0 not supported\n");
}
*/

/*INI
display_errors=1
log_errors=0
*/

/*EXPECT_REGEX
Hello from happy path!
*/

/*EXPECT_TRACED_ERRORS null */

/*EXPECT_ERROR_EVENTS null */

function errorHandlerOne($errno, $errstr, $errfile, $errline)
{
    switch ($errno) {
        case E_USER_DEPRECATED:
            return;
        }
    return false;
}

// set to the user defined error handler
$old_error_handler = set_error_handler("errorHandlerOne");

trigger_error("Let this serve as a deprecation", E_USER_DEPRECATED);  

echo("Hello from happy path!");         


