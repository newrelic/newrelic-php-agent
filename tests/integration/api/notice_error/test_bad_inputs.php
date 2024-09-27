<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should not record a traced error when newrelic_notice_error is
called with invalid parameters.
*/

/*EXPECT
ok - 1 arg
ok - 2 args
ok - 2 args
ok - 3 args
ok - 4 args
ok - 4 args
ok - 4 args
ok - 4 args
ok - 5 args
ok - 6 args
*/

/*EXPECT_TRACED_ERRORS null */
/*EXPECT_ERROR_EVENTS null */

require_once(realpath(dirname( __FILE__ )) . '/../../../include/tap.php');

// First arg must be a string or an exception. This is tricky because most
// values are implicitly convertible to strings. Use a resource to prevent this.
tap_equal(null, newrelic_notice_error(curl_init()), "1 arg");

// Second arg must an exception.
tap_equal(null, newrelic_notice_error("", 42), "2 args");
tap_equal(null, newrelic_notice_error("", array()), "2 args");

// Three argument forms are not allowed.
tap_equal(null, newrelic_notice_error(42, "message", "file"), "3 args");

// Four argument form requires integer, string, string, integer
// This is like the five argument form but for PHP 8+ where the context is not supplied
tap_equal(null, newrelic_notice_error("", "message", "file", __LINE__), "4 args");
tap_equal(null, newrelic_notice_error(42, array(), "file", __LINE__), "4 args");
tap_equal(null, newrelic_notice_error("", "message", array(), __LINE__), "4 args");
tap_equal(null, newrelic_notice_error("", "message", "file", ""), "4 args");

// Five argument form requires second arg to be convertible to a string.
tap_equal(null, newrelic_notice_error("", curl_init()), "5 args");

// A maximum of five arguments is allowed.
tap_equal(null, newrelic_notice_error(42, "message", "file", __LINE__, null, null), "6 args");
