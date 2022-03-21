<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should capture and report unhandled exceptions with large error messages.
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Uncaught exception 'Exception' with message 'this is a very large error message that will extend beyond a 256 character limit by rambling about the size of the error, as well as inserting random characters. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz It will also repeat... this is a very large error message that will extend beyond a 256 character limit by rambling about the size of the error, as well as inserting random characters. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz It will also repeat...this is a very large error message that will extend beyond a 256 character limit by rambling about the size of the error, as well as inserting random characters. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz It will also repeat... this is a very large error message that will extend beyond a 256 character limit by rambling about the size of the error, as well as inserting random characters. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz It will also repeat...this is a very large error message that will extend beyond a 256 character limit by rambling about the size of the error, as well as inserting random characters. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz It will also repeat... this is a very large error message that will extend beyond a 256 character limit by rambling about the size of the error, as well as inserting random characters. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz It will also repeat...' in __FILE__:??",
      "Exception",
      {
        "stack_trace": [
          " in alpha called at __FILE__ (??)"
        ],
        "agentAttributes": "??",
        "intrinsics": "??"
      }
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
        "error.class": "Exception",
        "error.message": "Uncaught exception 'Exception' with message 'this is a very large error message that will extend beyond a 256 character limit by rambling about the size of the error, as well as inserting random characters. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz It will also repeat... this is a very large error message that will extend beyond a 256 character limit by rambling about the size of the error, as well as inserting random characters. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz It will also repeat...this is a very large error message that will extend beyond a 256 character limit by rambling about the size of the error, as well as inserting random characters. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz It will also repeat... this is a very large error message that will extend beyond a 256 character limit by rambling about the size of the error, as well as inserting random characters. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz It will also repeat...this is a very large error message that will extend beyond a 256 character limit by rambling about the size of the error, as well as inserting random characters. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz It will also repeat... this is a very large error message that will extend beyond a 256 character limit by rambling about the size of the error, as well as inserting random characters. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz It will also repeat...' in __FILE__:??",
        "transactionName": "OtherTransaction\/php__FILE__",
        "duration": "??",
        "nr.transactionGuid": "??"
      },
      {},
      {}
    ]
  ]
]
*/

function alpha() {
  throw new Exception('this is a very large error message that will extend beyond a 256 character limit by rambling about the size of the error, as well as inserting random characters. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz It will also repeat... this is a very large error message that will extend beyond a 256 character limit by rambling about the size of the error, as well as inserting random characters. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz It will also repeat...this is a very large error message that will extend beyond a 256 character limit by rambling about the size of the error, as well as inserting random characters. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz It will also repeat... this is a very large error message that will extend beyond a 256 character limit by rambling about the size of the error, as well as inserting random characters. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz It will also repeat...this is a very large error message that will extend beyond a 256 character limit by rambling about the size of the error, as well as inserting random characters. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz It will also repeat... this is a very large error message that will extend beyond a 256 character limit by rambling about the size of the error, as well as inserting random characters. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz It will also repeat...');
}

// Attempt to get rid of our exception handler.
restore_exception_handler();

alpha();
