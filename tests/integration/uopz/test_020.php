<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
uopz_get_exit_status
*/

/*SKIPIF
<?php include("skipif.inc") ?>
*/

/*INI
opcache.enable_cli=0
xdebug.enable=0
*/

/*EXPECT
int(10)
*/

require __DIR__.'/load.inc';

/*
 *  PHP may optimise away any oplines after a direct exit() call. Therefore
 * we'll do it in an eval scope at runtime.
 */
eval('exit(10);');

var_dump(uopz_get_exit_status());
