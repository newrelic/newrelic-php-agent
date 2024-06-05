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
require "newrelic-install-php-cfg-mappings.php";

/* Verify that ini/envvar mapping was injected and is defined */
if (!defined('INI_ENVVAR_MAP')) {
    failure("INI/ENVVAR mapping was not detected - cannot proceed!");
}

/* Verify input INI file exists */
if (3 != $argc) {
    failure(" missing required arguments\n" .
            "Usage: newrelic-install-inject-envvars.php <input INI> <output INI>\n" .
            "    <input INI>   Existing INI file\n" .
            "    <output INI>  Output INI file with injected env var values\n\n");
}

$ini_filename = $argv[1];
if (!file_exists($ini_filename)) {
    failure("Input INI file \"$ini_filename\" does not exist!");
}

$out_ini_filename = $argv[2];
if (file_exists($out_ini_filename)) {
    failure("Output INI file \"$out_ini_filename\" exists - will not overwrite!");
}

$data = file_get_contents(($ini_filename));
if (!$data) {
    failure("Could not read INI file \"$ini_filename\"!");
}

$pattern = array();
$replace = array();
$modified = array();

/* Go through all INI keys and see if the environment variable is defined */
foreach (INI_ENVVAR_MAP as $ini_name => $env_name) {
    $env_value = getenv($env_name);
    if (false != $env_value) {
        array_push($pattern, generate_regex($ini_name));
        array_push($replace, "$ini_name=$env_value");
        array_push($modified, $env_name);
    }
}

if (0 < count($pattern)) {
    /* replace all values which exist in existing INI file */
    $data = preg_replace($pattern, $replace, $data);

    /* append values which will did not already exist in file */
    $missing = "";
    foreach(INI_ENVVAR_MAP as $ini_name => $env_name) {
        $env_value = getenv($env_name);
        if (false != $env_value) {
            if (!preg_match(generate_regex($ini_name), $data)) {
                $missing .= "/* Value injected from env var $env_name*/\n";
                $missing .= "$ini_name=$env_value\n";
            }
        }
    }
    $data .= $missing;

    echo "opening $out_ini_filename\n";
    $fh = fopen($out_ini_filename, "w");
    if (!$fh) {
        failure("Unable to write out modified INI file $out_ini_filename");
    }
    fwrite($fh, $data);
    fclose($fh);

    echo "$out_ini_filename created with values from the following environment variables:\n";
    foreach ($modified as $value) {
        echo "   $value\n";
    }
} else {
    echo "$out_ini_filename contains no modified values as no relevant environment variables detected";
}
