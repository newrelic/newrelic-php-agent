<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Under opcache.preload, the agent detects libraries at transaction start only
from preloaded scripts, not from every script opcache has cached.

Opcache also caches scripts the current request never runs, such as scripts
from earlier requests, or here, a script compiled with opcache_compile_file().
Matching those against signature files would detect libraries the request
doesn't use. A script that a request does execute is detected when it runs.

The test compiles MongoDB's signature file into opcache without preloading or
executing it, then starts a new transaction, which looks at opcache's scripts
again. The library must not be detected.
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
opcache.preload=config/preload_no_library.php
opcache.preload_user=www-data
*/

/*EXPECT
ok - signature file compiled
ok - signature file cached
*/

/*EXPECT_METRICS_DONT_EXIST
Supportability/library/MongoDB/detected
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');

$signature_file = __DIR__ . '/vendor/mongodb/mongodb/src/Client.php';

// Cache the signature file in opcache without preloading or executing it.
tap_assert(opcache_compile_file($signature_file), "signature file compiled");
tap_assert(opcache_is_script_cached($signature_file), "signature file cached");

// Starting a transaction looks at opcache's scripts again, now with the
// signature file cached.
newrelic_end_transaction();
newrelic_start_transaction(ini_get("newrelic.appname"));
