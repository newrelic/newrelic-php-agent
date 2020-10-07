<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
newrelic_get_browser_timing_footer(false) should return the
Real User Monitoring (RUM) footer without a containing script tag.
*/

/*EXPECT
ok - does not start with script tag
ok - does not end with script tag
ok - content looks like RUM footer
ok - subsequent calls return empty string
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');

define('OPENTAG', '<script type="text/javascript">');
define('CLOSETAG', '</script>');

// Need to get the header before we can get the footer.
newrelic_get_browser_timing_header(false);

$footer = newrelic_get_browser_timing_footer(false);
$prefix = substr($footer, 0, strlen(OPENTAG));
$suffix = substr($footer, -strlen(CLOSETAG));

tap_not_equal(OPENTAG,  $prefix, 'does not start with script tag');
tap_not_equal(CLOSETAG, $suffix, 'does not end with script tag');

tap_not_equal(false, strpos($footer, 'NREUM'), 'content looks like RUM footer');

$footer2 = newrelic_get_browser_timing_footer(false);
tap_equal('', $footer2, 'subsequent calls return empty string');
