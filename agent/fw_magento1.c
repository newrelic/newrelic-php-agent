/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_wrapper.h"
#include "fw_magento_common.h"
#include "fw_hooks.h"

NR_PHP_WRAPPER(nr_magento1_action_dispatch) {
  zval* this_var = NULL;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_MAGENTO1);

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  nr_magento_name_transaction(this_var TSRMLS_CC);

  NR_PHP_WRAPPER_CALL;

  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

void nr_magento1_enable(TSRMLS_D) {
  nr_php_wrap_user_function(
      NR_PSTR("Mage_Core_Controller_Varien_Action::dispatch"),
      nr_magento1_action_dispatch TSRMLS_CC);
}
