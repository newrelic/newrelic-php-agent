<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
With newrelic.preload_framework_library_detection disabled, the agent doesn't
detect libraries from opcache's preloaded scripts at transaction start.

This is test_library_detection_survives_ignored_preload.php with the setting
turned off: the same preload script ends the transaction the agent starts for
the preload request, then preloads MongoDB's signature file. Only the
transaction-start pass could detect MongoDB in this test's transaction, so
with it disabled the library must not be detected.

The output is matched with a regex rather than compared in full, for the same
reason as in test_library_detection_skips_scripts_not_preloaded.php: run as
root in a coverage build, the preload child reports gcov errors on stderr.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.4", "<")) {
  die("skip: PHP < 7.4 not supported\n");
}
if (!extension_loaded('Zend OPcache')) {
  die("skip: OPcache extension required\n");
}
*/

/*INI
opcache.enable=1
opcache.enable_cli=1
opcache.preload=config/ignored_preload.php
opcache.preload_user=www-data
newrelic.preload_framework_library_detection=0
*/

/*EXPECT_REGEX
(?m)^ok - signature file preloaded$
*/

/*EXPECT_METRICS_DONT_EXIST
Supportability/library/MongoDB/detected
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');

// The class is defined without autoloading only if the preload ran.
tap_assert(class_exists('MongoDB\Operation\Aggregate', false),
           "signature file preloaded");
