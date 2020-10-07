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

/*EXPECT_SCRUBBED
Fatal error: Uncaught RuntimeException: uopz is disabled by configuration (uopz.disable) in __FILE__:??
Stack trace:
#0 __FILE__(??): uopz_set_return()
#1 {main}
  thrown in __FILE__ on line ??
*/

require __DIR__.'/load.inc';

uopz_set_return();
