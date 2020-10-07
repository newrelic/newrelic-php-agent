<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Autoload the newrelic/integration library.
 */

$files = array(
  'DatastoreInstance.php',
  'Metric.php',
  'SlowSQL.php',
  'Trace/Segment.php',
  'Trace/FilterIterator.php',
  'Trace/StringTable.php',
  'Trace.php',
  'Transaction.php',
);

foreach ($files as $file) {
  require_once __DIR__.'/newrelic-integration/src/'.$file;
}
