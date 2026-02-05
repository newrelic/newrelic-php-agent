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
        protected array $hookLists = array(
          'hookname' => array('functionname' => 'modulename'),
          'hookname_b' => array('' => 'modulename_b'),
          'hookname_c' => array('functionname_c' =>  'modulename_c'),
        );

        // to avoid editor warnings
        public function invokeAllWith($hook_str, $callback)
        {
            return null;
        }

        // for debugging purposes
        public function dump()
        {
            var_dump($this->hookLists);
        }
    }
}
