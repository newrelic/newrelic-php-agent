<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Internal functions disabled with the disable_functions INI directive
must return false when checked with function_exists().
*/

/*INI
disable_functions = strlen
error_reporting = E_ERROR | E_FATAL_ERROR
*/

/*EXPECT
bool(false)
NULL
*/

var_dump(function_exists('strlen'));
var_dump(strlen('foo'));
