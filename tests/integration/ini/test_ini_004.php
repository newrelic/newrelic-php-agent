<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Name the web transaction when a specially named and register class method
executes. The regular expressions here are deliberately mangled to see what
happens. For the list of files, we reference ourself with a pcre.
*/

/*INI
newrelic.webtransaction.name.functions = CLI/PHP_FLAGS_ARGS,Foobar::interesting_method,bar,baz,,,
newrelic.webtransaction.name.files = .*exercise_ini_3.php,**,[,bat/,baz,,,
*/

/*EXPECT
Foobar::interesting_method() called
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"Custom/Foobar::interesting_method"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/Foobar::interesting_method",
      "scope":"OtherTransaction/Function/Foobar::interesting_method"},
                                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/Function/Foobar::interesting_method"}, 
                                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/Function/Foobar::interesting_method"}, 
                                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/add_custom_tracer"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/InstrumentedFunction/Foobar::interesting_method"},
                                                        [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

class Foobar {
  static function interesting_method() {
    echo "Foobar::interesting_method() called\n";
  }
}

newrelic_add_custom_tracer("Foobar::interesting_method");

Foobar::interesting_method();
