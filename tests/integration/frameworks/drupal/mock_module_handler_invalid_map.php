<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* Verify agent behavior when hookImplemementationsMap is not an array */

namespace Drupal\Core\Extension {
  interface ModuleHandlerInterface
  {
    public function invokeAllWith($hook_str, $callback);
  }
  class ModuleHandler implements ModuleHandlerInterface
  {
    protected string $hookImplementationsMap = 'just a string';

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
