<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* Verify agent behavior when key is an empty string */

namespace Drupal\Core\Extension {
  interface ModuleHandlerInterface
  {
    public function invokeAllWith($hook_str, $callback);
  }
  class ModuleHandler implements ModuleHandlerInterface
  {
    protected array $hookImplementationsMap = array(
      'hookname' => array('classname' => array('methodname' => 'modulename')),
      'hookname_b' => array('' => array('methodname_b' => 'modulename_b')),
      'hookname_c' => array('classname_c' => array('methodname_c' => 'modulename_c')),
    );

    // to avoid editor warnings
    public function invokeAllWith($hook_str, $callback)
    {
      return null;
    }

    // for debugging purposes
    public function dump()
    {
      var_dump($this->hookImplementationsMap);
    }
  }
}
