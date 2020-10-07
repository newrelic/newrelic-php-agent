<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
newrelic_record_datastore_segment() should still execute the callback when
recording is disabled.
*/

/*EXPECT
int(42)
*/

newrelic_end_transaction(false);

var_dump(newrelic_record_datastore_segment(function () {
  // Make sure this function takes at least 1 microsecond to ensure that a trace
  // node is generated.
  time_nanosleep(0, 1000);
  return 42;
}, array(
  'product' => 'custom',
)));
