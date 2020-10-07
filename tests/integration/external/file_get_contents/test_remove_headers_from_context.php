<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that newrelic_remove_headers_from_context removes all New Relic headers.
call.
*/

/*INI
newrelic.distributed_tracing_enabled = true 
*/

/*EXPECT
ok - no headers
ok - no headers
ok - no headers
ok - no headers
ok - no headers
ok - no headers
ok - customer header
ok - customer header
ok - customer header
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

function get_headers_from_context_after_remove($opts) {
    $context = stream_context_create($opts);
    newrelic_remove_headers_from_context($context);
    $opts = stream_context_get_options($context);
    $opts_http = $opts['http'];
    return $opts_http['header'];
}

/* No headers. */
$opts = array('http' => array('method' => 'GET', 'header' => ''));
$header = get_headers_from_context_after_remove($opts);
tap_equal('', $header, 'no headers');

/* Remove CAT headers. */
$opts = array('http' => array('method' => 'GET', 'header' => X_NEWRELIC_ID . ": id\r\n" . 
    X_NEWRELIC_TRANSACTION . ": transaction"));
$header = get_headers_from_context_after_remove($opts);
tap_equal('', $header, 'no headers');

/* Remove distributed tracing headers. */
$opts = array('http' => array('method' => 'GET', 'header' => DT_NEWRELIC . ": dt"));
$header = get_headers_from_context_after_remove($opts);
tap_equal('', $header, 'no headers');

/* Remove Synthetics  headers. */
$opts = array('http' => array('method' => 'GET', 'header' => X_NEWRELIC_SYNTHETICS . ": id"));
$header = get_headers_from_context_after_remove($opts);
tap_equal('', $header, 'no headers');

/* Remove CAT & Synthetics headers. */
$opts = array('http' => array('method' => 'GET', 'header' => X_NEWRELIC_ID. ": id\r\n" . 
    X_NEWRELIC_TRANSACTION . ": transaction\r\n" . 
    X_NEWRELIC_SYNTHETICS . ": synthetics"));
$header = get_headers_from_context_after_remove($opts);
tap_equal('', $header, 'no headers');

/* Remove DT & Synthetics headers. */
$opts = array('http' => array('method' => 'GET', 'header' => DT_NEWRELIC . ": dt\r\n" . 
    X_NEWRELIC_SYNTHETICS . ": synthetics"));
$header = get_headers_from_context_after_remove($opts);
tap_equal('', $header, 'no headers');

/* Remove CAT & Synthetics headers and keep custom header. */
$opts = array('http' => array('method' => 'GET', 'header' => X_NEWRELIC_ID. ": id\r\n" . 
    X_NEWRELIC_TRANSACTION . ": transaction\r\n" . 
    X_NEWRELIC_SYNTHETICS . ": synthetics\r\n" . 
    CUSTOMER_HEADER . ": custom"));
$header = get_headers_from_context_after_remove($opts);
tap_equal(CUSTOMER_HEADER . ': custom', $header, 'customer header');

/* Remove DT & Synthetics headers and keep custom header. */
$opts = array('http' => array('method' => 'GET', 'header' => DT_NEWRELIC . ": dt\r\n" . 
    X_NEWRELIC_SYNTHETICS . ": synthetics\r\n" . 
    CUSTOMER_HEADER . ": custom"));
$header = get_headers_from_context_after_remove($opts);
tap_equal(CUSTOMER_HEADER . ': custom', $header, 'customer header');

/* Keep custom header. */
$opts = array('http' => array('method' => 'GET', 'header' => CUSTOMER_HEADER . ": custom"));
$header = get_headers_from_context_after_remove($opts);
tap_equal(CUSTOMER_HEADER . ': custom', $header, 'customer header');
