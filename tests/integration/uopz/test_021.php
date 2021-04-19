<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Disable uopz.
*/

/*SKIPIF
<?php include("skipif.inc") ?>
*/

/*INI
uopz.disable=1
*/

/*EXPECT_REGEX
(PHP )?Fatal error: .?Uncaught RuntimeException: uopz is disabled by configuration \(uopz\.disable\) in .*:\d+
Stack trace:
#0 .*\(\d+\): uopz_set_return\(\)
#1 {main}
  thrown in .* on line \d+
*/

require __DIR__.'/load.inc';

uopz_set_return();
