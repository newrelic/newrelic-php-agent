<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');

if (version_compare(phpversion(), '5.5.0', '<=')) {
    die("skip: PHP > 5.5.0 required\n");
}

if (!unpack_guzzle(6)) {
    die("skip: guzzle 6 installation required\n");
}
