<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');

if (version_compare(phpversion(), '7.2.0', '<=')) {
    die("skip: PHP > 7.2.0 required\n");
}

if (!unpack_guzzle(7)) {
    die("skip: guzzle 7 installation required\n");
}
