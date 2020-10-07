<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
newrelic_get_browser_timing_footer(non-zero) should return the RUM footer
wrapped in a script tag.
*/

/*EXPECT
ok - starts with script tag
ok - ends with script tag
ok - content looks like RUM footer
ok - subsequent calls return empty string
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');

define('OPENTAG', '<script type="text/javascript">');
define('CLOSETAG', '</script>');

// Need to get the header before we can get the footer.
newrelic_get_browser_timing_header(true);

$footer = newrelic_get_browser_timing_footer(42);
$prefix = substr($footer, 0, strlen(OPENTAG));
$suffix = substr($footer, -strlen(CLOSETAG));

tap_equal(OPENTAG,  $prefix, 'starts with script tag');
tap_equal(CLOSETAG, $suffix, 'ends with script tag');

tap_not_equal(false, strpos($footer, 'NREUM'), 'content looks like RUM footer');

$footer2 = newrelic_get_browser_timing_footer(42);
tap_equal('', $footer2, 'subsequent calls return empty string');
