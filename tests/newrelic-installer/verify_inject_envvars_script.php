<?PHP
/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* Purpose:
 *    Checks that INI name to env var name mappings are correct
 */
function failure(string $msg) {
    die(__FILE__ . " : ERROR : " . $msg . "\n");
}

$names = newrelic_get_all_ini_envvar_names();
if (0 == count($names)) {
    trigger_error("Mapping of INI -> ENVVAR names could not be captured!");
    exit(1);
}

/* Verify input INI file exists */
if (2 != $argc) {
    failure("Must supply existing mapping file as first argument!");
}

$mapping_filename = $argv[1];

include $mapping_filename;

$keys1 = array_keys(INI_ENVVAR_MAP);
$keys2 = array_keys($names);

$diff1 = array_diff($keys1, $keys2);
$diff2 = array_diff($keys2, $keys1);

$matches = true;
if (0 != count($diff1)) {
    echo "Keys in current mapping not in agent:\n";
    foreach ($diff1 as $n) {
        echo "   $n\n";
    }
    $matches = false;
}
if (0 != count($diff2)) {
    echo "Keys in agent not in current mapping:\n";
    foreach ($diff2 as $n) {
        echo "   $n\n";
    }
    $matches = false;
}

if ($matches) {
    echo "Agent and mapping match!\n";
    exit(0);
} else {
    exit(1);
}

?>
