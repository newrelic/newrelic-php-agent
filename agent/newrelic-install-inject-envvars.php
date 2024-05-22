<?PHP
/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* Purpose: Read a fresh newrelic.ini file and inject any values
 * from environment variables.
 * 
 * Not designed to work on a modified newrelic.ini and script
 * is not able to detect this scenario.
 * 
 * The mapping from INI directive name to corresponding 
 * environment variable name must be injected before using
 * in place of the comment below containing INI_ENVVAR_MAP
 *
 */
function failure(string $msg) {
    die(__FILE__ . " : ERROR : " . $msg . "\n");
}

function generate_regex(string $ini_name) {
    $ini_name_esc = str_replace(".", "\.", $ini_name);
    return "/^\s*;*\s*" . $ini_name_esc . "\s*=.*/im";
}

/* requires PHP 7+ */
 if (version_compare(PHP_VERSION, '7.0', '<')) {
    failure("requires PHP >= 7.0\n");
}

/* Mapping from INI name to environment variable name
 * Expected form:
 *    define('INI_ENVVAR_MAP', array(....));
 */
require "newrelic-php-cfg-mappings.php";

/* Verify that ini/envvar mapping was injected and is defined */
if (!defined('INI_ENVVAR_MAP')) {
    failure("INI/ENVVAR mapping was not detected - cannot proceed!");
}

/* Verify input INI file exists */
if (2 != $argc) {
    failure("Must supply INI file name as first argument!");
}

$ini_filename = $argv[1];
if (!file_exists($ini_filename)) {
    failure("INI file \"$ini_filename\" does not exist!");
}

$data = file_get_contents(($ini_filename));
if (!$data) {
    failure("Could not read INI file \"$ini_filename\"!");
}

$pattern = array();
$replace = array();

/* Go through all INI keys and see if the environment variable is defined */
foreach (INI_ENVVAR_MAP as $ini_name => $env_name) {
    array_push($pattern, generate_regex($ini_name));
    array_push($replace, "$ini_name=$env_name");
}

$data = preg_replace($pattern, $replace, $data);

$fh = fopen($ini_filename, "w");
if (!$fh) {
    failure("Unable to write out modified INI file $ini_filename");
}
fwrite($fh, $data);
fclose($fh);

