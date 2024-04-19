<?php

/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* Mock enough bits of Yii2 infrastructure for naming tests. */

namespace yii\base {

    /* Web action */
    class Action {
        private $uniqid;
        
        public function __construct($argument) {
            $this->uniqid = $argument;
        }

        public function getUniqueId ( ) {
            return $this->uniqid;
        }

        public function runWithParams($params) {
            return;
        }
    }

    /* Console action */
    class InlineAction {
        private $uniqid;

        public function __construct($id) {
            $this->uniqid = $id;
        }

        public function getUniqueId ( ) {
            return $this->uniqid;
        }

        public function runWithParams($params) {
            return;
        }
    }
    echo "";
}
