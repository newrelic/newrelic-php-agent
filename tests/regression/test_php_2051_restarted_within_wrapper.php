<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should react gracefully when a transaction is restarted within a wrapped
PHP function.
*/

/*INI
newrelic.framework = wordpress
*/

/*EXPECT
No alarms and no surprises.
*/

function restart() {
  $appname = ini_get("newrelic.appname");
  $license = ini_get("newrelic.license");

  $result = newrelic_set_appname($appname, $license, false);
}

/* Emulate enough of do_action() for the WordPress instrumentation to fire. */
function do_action($tag) {
  /* 
   * This has to be a call_user_func_array() call, because that's what the
   * WordPress instrumentation is listening for.
   */
  call_user_func_array('restart', array());
}

do_action('fake');
echo 'No alarms and no surprises.';
