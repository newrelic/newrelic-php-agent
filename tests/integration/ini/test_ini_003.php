<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Set some php flags to get additional code coverage of the initialization file
parsing. The regular expressions here are deliberately mangled to see what
happens. For the list of files, we reference ourself with a pcre.
*/

/*INI
newrelic.webtransaction.name.functions = f_0,f_1,bogus_f_0,,,
newrelic.webtransaction.name.files = .*test_ini_003.php,**,[,bat/,baz,,,
*/

/*EXPECT
f_0() called
f_0() called
f_1() called
f_1() called
f_2() called
f_3() called
f_3() called
f_3() called
*/

// TODO(rrh): why don't we get a Custom/f_1 metric? (f_1 requested from .ini value)
/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"Custom/f_0"},                              [2, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/f_0",
      "scope":"OtherTransaction/Function/f_0"},          [2, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/f_3"},                              [3, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/f_3",
      "scope":"OtherTransaction/Function/f_0"},          [3, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/add_custom_tracer"},    [2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/Function/f_0"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/Function/f_0"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

class Foobar {
  static function interesting_method() {
    echo "Foobar::interesting_method() called\n";
  }
}

function f_0()
{
  echo "f_0() called\n";
}

function f_1()  /* added via newrelic.webtransaction.name.functions */
{
  echo "f_1() called\n";
}

function f_2()  /* not added as a custom tracer or special wrapped function */
{
  echo "f_2() called\n";
}

/*
 * Force a file to be loaded.
 * This will iterate through all wraprecs and wrap any
 * forward-instrumented functions of interest that were loaded in that
 * file, or have otherwise escaped detection.
 */
require_once('dummy.inc');

newrelic_add_custom_tracer("f_0");  /* traced AFTER  the function is defined (doesn't seem to matter) */
newrelic_add_custom_tracer("f_3");  /* traced BEFORE the function is defined (doesn't seem to matter) */

function f_3()
{
  echo "f_3() called\n";
}

f_0();
f_0();

/*
 * tracing f_1 requested from newrelic.webtransaction.name.functions
 */
f_1();
f_1();

f_2();

f_3();
f_3();
f_3();
