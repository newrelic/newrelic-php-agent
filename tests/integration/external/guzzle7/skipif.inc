<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');

if (version_compare(phpversion(), '8.1', '<')) {
    die("skip: PHP >= 8.1 required\n");
}

/*
if (version_compare(phpversion(), '8.2', '>')) {
    die("skip: PHP < 8.3 required\n");
}
*/

if (!unpack_guzzle(7)) {
    die("warn: guzzle 7 installation required\n");
}
