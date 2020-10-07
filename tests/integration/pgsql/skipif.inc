<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

if (!extension_loaded("pgsql")) {
    die("skip: pgsql extension required\n");
}

$pg = @pg_connect($PG_CONNECTION);

if (!$pg) {
    die("skip: unable to connect to pgsql server\n");
}
