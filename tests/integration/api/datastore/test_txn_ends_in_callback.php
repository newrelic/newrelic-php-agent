<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A transaction that the callback of newrelic_record_datastore_segment() ends and
starts again SHALL NOT break the agent, and the segment SHALL NOT be recorded
in either transaction.

The use of freed memory this guards against is only detected under valgrind.
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
*/

/*EXPECT
transaction restarted
done
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/operation/MySQL/select
*/

/*EXPECT_TRACED_ERRORS
null
*/

newrelic_record_datastore_segment(
    function () {
      newrelic_end_transaction();
      newrelic_start_transaction(ini_get("newrelic.appname"));
      echo "transaction restarted\n";
    },
    array(
        'product' => 'MySQL',
        'collection' => 'users',
        'operation' => 'select',
    ));
echo "done\n";
