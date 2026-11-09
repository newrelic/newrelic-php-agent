<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent's fatal signal handler writes at most 1000 frames of the PHP stack. A deeper stack is cut, and a line says so, so that a trace
of a deep recursion does not look complete. The script has the shell send itself SIGSEGV at the bottom of a recursion 1100 frames deep.
*/

/*SKIPIF
<?php
if (!function_exists('exec')) {
  die("skip: exec() is not available\n");
}
exec('kill -0 ' . getmypid(), $output, $status);
if (0 !== $status) {
  die("skip: the shell cannot signal this process\n");
}
*/

/*INI
newrelic.logfile = "stderr"
*/

/*EXPECT_HARVEST no */

/*EXPECT_REGEX
PHP execution trace follows\.\.\.
(#[0-9]+ [^\n]*\n)+\.\.\. \(more frames not shown: the trace stops after 1000 frames\)
*/

function recurse($depth) {
  if (0 === $depth) {
    exec('kill -SEGV ' . getmypid());
    // the signal ends the process in exec() or here
    sleep(5);
    return;
  }
  recurse($depth - 1);
}

recurse(1100);
