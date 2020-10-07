<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
If curl_exec is called multiple times, each New Relic header
should be distinct because each has it's own guid.
*/

/*SKIPIF
<?php
if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}
*/

/*EXPECT_REGEX
ok - headers should be unique
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.cross_application_tracer.enabled = false
*/
require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

function createCurlHandler($url, $test) {
    $ch = curl_init();

    curl_setopt($ch, CURLOPT_URL, $url);
    curl_setopt($ch, CURLINFO_HEADER_OUT, true);
    curl_setopt($ch, CURLOPT_NOBODY, true);
    curl_setopt($ch, CURLOPT_FAILONERROR,1);
    curl_setopt($ch, CURLOPT_FOLLOWLOCATION,1);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER,1);
    curl_setopt($ch, CURLOPT_TIMEOUT, 15);
    curl_setopt($ch, CURLOPT_HTTPHEADER, array(
        'X-QA-TEST: ' . $test,
    ));
    return $ch;
}

function executeCurlHandlerGetHeaders($ch) {
    curl_exec($ch);
    $string = curl_getinfo($ch, CURLINFO_HEADER_OUT);
    curl_close($ch);
    return $string;
}
$url = make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');

$first  = createCurlHandler($url, 42);
$second = createCurlHandler($url, 43);

$result1 = executeCurlHandlerGetHeaders($first);
$result2 = executeCurlHandlerGetHeaders($second);

tap_not_equal(0, strcmp($result1, $result2), "headers should be unique");

