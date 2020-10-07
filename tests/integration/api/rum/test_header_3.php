<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
newrelic_get_browser_timing_header(false) should return the RUM loader without a
containing script tag.
*/

/*EXPECT
ok - does not start with script tag
ok - does not end with script tag
ok - content looks like RUM loader
ok - subsequent calls return empty string
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');

define('OPENTAG', '<script');      // no closing '>' to allow for attributes
define('CLOSETAG', '</script>');

$header = newrelic_get_browser_timing_header(false);
$prefix = substr($header, 0, strlen(OPENTAG));
$suffix = substr($header, -strlen(CLOSETAG));

tap_not_equal(OPENTAG,  $prefix, 'does not start with script tag');
tap_not_equal(CLOSETAG, $suffix, 'does not end with script tag');

tap_not_equal(false, strpos($header, 'NREUM'), 'content looks like RUM loader');

$header2 = newrelic_get_browser_timing_header(false);
tap_equal('', $header2, 'subsequent calls return empty string');
