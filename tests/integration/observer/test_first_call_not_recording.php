<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A function whose first-ever call happens while the transaction is being
ignored must still be instrumented once a fresh, recording transaction
starts later in the same PHP request. This is a regression test for the
nr_php_fcall_register_handlers registration gate: the Observer API only
invokes that gate once per op_array per PHP request, on the function's
first call, and caches whatever handlers it returns for that op_array's
entire life during PHP request - so gating registration on the live
recording state meant a function first called while not recording could
never be instrumented again.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0 not supported\n");
}
*/

/*INI
newrelic.transaction_tracer.threshold = 0
*/

/*EXPECT_METRICS_EXIST
Custom/f
*/

newrelic_add_custom_tracer("f");

function f() {
    echo "f\n";
}

function main() {
    newrelic_ignore_transaction(); // turn recording off for this New Relic transaction
    f(); // zend_observer_fcall_init's callback is invoked for f();
         // it is the only chance to register fcall_begin/fcall_end
         // handlers for f() in this PHP request
    newrelic_end_transaction(); // end New Relic transaction, which is not recording
    newrelic_start_transaction(ini_get("newrelic.appname")); // Start New Relic transaction, recording is back on
    f(); // fcall_begin/fcall_end handlers for f() should be invoked here and
         // f() should be instrumented, even though its first-ever call was
         // made while the transaction was not recording
}

main();
