<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should ignore errors when that a custom error handler handles
but record errors that are not handled.
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

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Let this serve as a deprecation",
      "E_USER_DEPRECATED",
      {
        "stack_trace": "??",
        "agentAttributes": "??",
        "intrinsics": "??"
      },
      "?? transaction ID"
    ]
  ]
]
*/

/*EXPECT_ERROR_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": "??",
    "events_seen": 1
  },
  [
    [
      {
        "type": "TransactionError",
        "timestamp": "??",
        "error.class": "E_USER_DEPRECATED",
        "error.message": "Let this serve as a deprecation",
        "transactionName": "OtherTransaction\/php__FILE__",
        "duration": "??",
        "nr.transactionGuid": "??",
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??",
        "spanId": "??"
      },
      {},
      {}
    ]
  ]
]
*/

class Classy
{
    // property declaration
    public $var = NULL;
}

function errorHandlerOne($errno, $errstr, $errfile, $errline)
{

    if (preg_match('/^Attempt to read property ".+?" on/', $errstr)) {
            echo ("Nothing to see here, suppressing attempt to read property\n");
        return true; // suppresses this error
    }
    
    if (preg_match(
        '/^(Undefined index|Undefined array key|Trying to access array offset on)/',
        $errstr
    )) {
        echo ("Nothing to see here, suppressing undefined array key\n");
        return true; // suppresses this error
    }
    
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

// generate "Attempt to read property" error
$bar = false;
echo ($bar->var);

// generate "Undefined array key" error
$missingOneArray = array(2=>'two', 4=>'four');
echo $missingOneArray[1];

//trigger an error that is not handled by the custom error handler
trigger_error("Let this serve as a deprecation", E_USER_DEPRECATED); 


