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
^\s*(PHP )?Notice:\s*Undefined variable: usernmae in .*? on line [0-9]+\s*$
*/

/*EXPECT_TRACED_ERRORS
null
*/

/*EXPECT_ERROR_EVENTS
null
*/

function provoke_notice() {
  $username = 'foo';

  // Misspell username to cause a notice.
  if ($usernmae) {
    return 1;
  }
  return 0;
}

provoke_notice();
