<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
New settings should take precedence over deprecated settings and attributes
should be included in the footer.

New setting: *.attributes.enabled 
Old setting: *.capture_attributes
*/

/*INI
newrelic.browser_monitoring.attributes.enabled = true
newrelic.browser_monitoring.capture_attributes = false
*/

die("warn: test outcome depends on the values in runtime environment");

/*EXPECT
non-empty attributes hash
*/

/* the headers must be requested before the footers can be accessed */
newrelic_get_browser_timing_header(true);

newrelic_add_custom_parameter("hat", "who");

$footer = newrelic_get_browser_timing_footer(true);
if(strpos($footer, '"atts":"SxUUEFsfFF4CQhENQ0dfDhAcGQ=="')) {
  echo("non-empty attributes hash");
} else {
    echo $footer;
}
