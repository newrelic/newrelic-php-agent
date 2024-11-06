<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* Mock enough bits of the Module Handler Interface for hook tests. */
namespace Drupal\Core\Extension {
    interface ModuleHandlerInterface {
        public function invokeAllWith($hook_str, $callback);
    }
    class ModuleHandler implements ModuleHandlerInterface {
        public function __construct(bool $except=false) {
            if ($except) {
                throw new Exception("Constructor told to except");
            }
        }
        public function invokeAllWith($hook_str, $callback) {
            if ($hook_str == "hook_1") {
                $module = "module_a";
                $callback($module . "_" . $hook_str, $module);
                $module = "module_b";
                $callback($module . "_" . $hook_str, $module);
            } else if ($hook_str == "hook_2") {
                $module = "module_b";
                $callback($module . "_" . $hook_str, $module);
            } else if ($hook_str == "hook_3") {
                $module = "module_a";
                $callback($module . "_" . $hook_str, $module);
            } else if ($hook_str == "hook_4") {
                $module = "module_b";
                $callback($module . "_" . $hook_str, $module);
            }
        }
    }
}
