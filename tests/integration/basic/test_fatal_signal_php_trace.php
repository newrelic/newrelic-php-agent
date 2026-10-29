<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent's fatal signal handler writes the PHP stack of the thread to the agent log before the process ends with the signal. The log
is standard error here, so that the runner sees it (the runner accepts a script that ends with a signal). The script has the shell send
it SIGSEGV from inside two PHP functions and waits; the PHP stack in the log has them. (The posix extension is not in every image.)
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

/*EXPECT_REGEX
received signal +11[^\n]*\n(.*\n)*PHP execution trace follows\.\.\.
#0 [a-z_]+\(\)
#1 crash_in_the_inner_function\(\) called at \[[^\n]*test_fatal_signal_php_trace\.php:\d+\]
#2 crash_in_the_outer_function\(\) called at \[[^\n]*test_fatal_signal_php_trace\.php:\d+\]
*/

function crash_in_the_inner_function() {
  exec('kill -SEGV ' . getmypid());
  // the signal ends the process in exec() or here
  sleep(5);
}

function crash_in_the_outer_function() {
  crash_in_the_inner_function();
}

crash_in_the_outer_function();
