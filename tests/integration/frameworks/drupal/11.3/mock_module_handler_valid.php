<?php

/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* Verify agent behavior on valid hookLists */

namespace Drupal\Core\Extension {
    interface ModuleHandlerInterface
    {
        public function invokeAllWith($hook_str, $callback);
    }
    class ModuleHandler implements ModuleHandlerInterface
    {
        protected array $hookLists = array(
          'hookname' => array('classname' => 'modulename'),
          'hookname_b' => array('classname_b' => 'modulename_b'),
          'hookname_c' => array('classname_c' => 'modulename_c'),
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
