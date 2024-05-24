<?PHP
/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* Purpose:
 *    Checks that all known INI values can be injected via environment variable
 */
function failure(string $msg)
{
    die(__FILE__ . " : ERROR : " . $msg . "\n");
}

$names = newrelic_get_all_ini_envvar_names();
if (0 == count($names)) {
    trigger_error("Mapping of INI -> ENVVAR names could not be captured!");
    exit(1);
}

$template = "../../agent/scripts/newrelic.ini.template";
$out_ini_file = "newrelic.ini-for-test";
if (file_exists($out_ini_file)) {
    unlink($out_ini_file);
}

// if (!copy($template, $dup_template)) {
//     failure("Could not create copy of INI template $template to $dup_template!");
// }

/* Create shell script to test env var injection */
$test_script = "test_envvar_inject.sh";
if (file_exists($test_script)) {
    unlink($test_script);
}

$script_fp = fopen($test_script, "w");
if (!$script_fp) {
    unlink($out_ini_file);
    failure("Could not create test script $test_script!");
}

fwrite($script_fp, "#!/bin/sh\n");
foreach ($names as $k => $v) {
    fwrite($script_fp, "export $v=$v\n");
}

fwrite($script_fp, "php ../../agent/newrelic-install-inject-envvars.php $out_ini_file\n");
fclose($script_fp);

$output = shell_exec("sh " . $test_script);
unlink($test_script);
if (!$output) {
    failure("Error running test script $test_script:\n" . $output);
}

/* if any ini directives ever need to be ignored use this array */
$ignore_list = array();

/* check that all expceted agent INI values were injected */
$ini_values = parse_ini_file($out_ini_file);
unlink($out_ini_file);
foreach ($names as $k => $v) {
    /* skip directives which are not part of newrelic.ini */
    if (in_array($k, $ignore_list)) {
        continue;
    }

    /* each ini value should be the env var name */
    if (!in_array($k, array_keys($ini_values))) {
        failure("Missing expected injected value for $k");
    }

    if ($ini_values[$k] != $names[$k]) {
        failure("Injected value for $k wrong:  expected \"$names[$k]\", actual \"$ini_values[$k]\"");
    }
}

echo "Injection script test PASS\n";
exit(0);
