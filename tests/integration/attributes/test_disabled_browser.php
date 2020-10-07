<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
By default, browser monitoring attributes should not show up.
*/

/*EXPECT
empty attributes hash
*/

/*
 * The headers must be requested before the footers can be accessed.
 */
newrelic_get_browser_timing_header(true);

newrelic_add_custom_parameter("hat", "who");

$footer = newrelic_get_browser_timing_footer(true);
if(strpos($footer, '"atts":"SxUUEFsfS0s="')) {
  echo("empty attributes hash");
} else {
    echo $footer;
}
