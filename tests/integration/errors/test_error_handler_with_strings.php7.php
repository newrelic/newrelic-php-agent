<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should ignore errors that match specific strings when a custom error 
handler exists that suppresses specific strings.
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
Nothing to see here, suppressing undefined property
*/

/*EXPECT_TRACED_ERRORS null */

/*EXPECT_ERROR_EVENTS null */

class Classy
{
    // property declaration
    public $var = NULL;
}

function errorHandlerOne($errno, $errstr, $errfile, $errline)
{    
    if (preg_match('/^(Undefined property)/', $errstr)) {
        echo ("Nothing to see here, suppressing undefined property\n");
        return true; // suppresses this error
    }

    return false;
}

// set to the user defined error handler
$old_error_handler = set_error_handler("errorHandlerOne");

$foo = new Classy();

//generate "Undefined property" error
echo ($foo->propName);
     


