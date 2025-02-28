<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Verify agent does not crash when module name is not a string.
*/

/*INI
newrelic.framework = drupal8
*/

/*EXPECT_TRACED_ERRORS null */

/*EXPECT_ERROR_EVENTS null */

/*EXPECT
*/

require_once __DIR__ . '/mock_module_handler_invalid_module.php';

// This specific API is needed for us to instrument the ModuleHandler
class Drupal
{
  public function moduleHandler()
  {
    return new Drupal\Core\Extension\ModuleHandler();
  }
}

// Create module handler
$drupal = new Drupal();
$handler = $drupal->moduleHandler();
