<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
newrelic_get_browser_timing_header(true) should return the RUM loader wrapped in
a script tag.
*/

/*EXPECT
ok - starts with script tag
ok - ends with script tag
ok - content looks like RUM loader
ok - subsequent calls return empty string
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');

define('OPENTAG', '<script type="text/javascript">');
define('CLOSETAG', '</script>');

$header = newrelic_get_browser_timing_header(true);
$prefix = substr($header, 0, strlen(OPENTAG));
$suffix = substr($header, -strlen(CLOSETAG));

tap_equal(OPENTAG,  $prefix, 'starts with script tag');
tap_equal(CLOSETAG, $suffix, 'ends with script tag');

tap_not_equal(false, strpos($header, 'NREUM'), 'content looks like RUM loader');

$header2 = newrelic_get_browser_timing_header(true);
tap_equal('', $header2, 'subsequent calls return empty string');
