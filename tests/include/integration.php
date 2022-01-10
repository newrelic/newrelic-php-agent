<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Autoload the newrelic/integration library.
 */

if (version_compare(PHP_VERSION, "8.1", "<")) {
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
}
else
{
  $files = array(
    'DatastoreInstance.php',
    'Metric.php',
    'SlowSQL.php',
    'Trace/Segment.php81.php',
    'Trace/FilterIterator.php81.php',
    'Trace/StringTable.php',
    'Trace.php',
    'Transaction.php',
  );
}

foreach ($files as $file) {
  require_once __DIR__.'/newrelic-integration/src/'.$file;
}
