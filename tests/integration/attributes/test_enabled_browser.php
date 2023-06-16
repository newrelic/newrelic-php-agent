<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When browser monitoring attributes are enabled, they should show up.
*/

/*INI
newrelic.browser_monitoring.attributes.enabled = true
*/

die("warn: test outcome depends on the values in runtime environment");

/*EXPECT
non-empty attributes hash
*/

/*
 * The headers must be requested before the footers can be accessed.
 */
newrelic_get_browser_timing_header(true);

newrelic_add_custom_parameter("hat", "who");

$footer = newrelic_get_browser_timing_footer(true);
if(strpos($footer, '"atts":"SxUUEFsfFF4CQhENQ0dfDhAcGQ=="')) {
  echo("non-empty attributes hash");
} else {
    echo $footer;
}
