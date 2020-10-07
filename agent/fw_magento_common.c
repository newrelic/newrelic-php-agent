/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_zval.h"
#include "nr_txn.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

#include "fw_magento_common.h"

static const char* nr_magento_name_to_string(const zval* zv) {
  if (nr_php_is_zval_valid_string(zv)) {
    return Z_STRVAL_P(zv);
  }
  return NULL;
}

void nr_magento_name_transaction(zval* action_obj TSRMLS_DC) {
  zval* request = NULL;
  zval* module = NULL;
  zval* controller = NULL;
  zval* action = NULL;
  const char* modulename = NULL;
  const char* controllername = NULL;
  const char* actionname = NULL;
  char* txn_name = NULL;

  if (!nr_php_is_zval_valid_object(action_obj)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "Magento: invalid action");
    goto leave;
  }

  /*
   * Magento 1:
   * request is an object of type Mage_Core_Controller_Request_Http.
   *
   * Magento 2:
   * request is an object of type Magento\Framework\App\Request\Http, which
   * extends Magento\Framework\HTTP\PhpEnvironment\Request (where these methods
   * are defined), which extends Zend\Http\PhpEnvironment\Request.
   * It's turtles all the way down.
   */
  request = nr_php_call(action_obj, "getrequest");
  if (!nr_php_is_zval_valid_object(request)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "Magento: get request object failed");
    goto leave;
  }

  module = nr_php_call(request, "getModuleName");
  controller = nr_php_call(request, "getControllerName");
  action = nr_php_call(request, "getActionName");

  modulename = nr_magento_name_to_string(module);
  controllername = nr_magento_name_to_string(controller);
  actionname = nr_magento_name_to_string(action);

  nrl_verbosedebug(NRL_FRAMEWORK, "Magento: module=%s controller=%s action=%s",
                   NRSAFESTR(modulename), NRSAFESTR(controllername),
                   NRSAFESTR(actionname));

  if (!modulename && !controllername && !actionname) {
    nrl_verbosedebug(NRL_WARNING, "Magento: transaction naming failed");
    goto leave;
  }

  txn_name = nr_formatf("%s/%s/%s", modulename ? modulename : "NoModule",
                        controllername ? controllername : "NoController",
                        actionname ? actionname : "NoAction");

  /*
   * If successful, txn_name is of the form "customer/account/index", built from
   * the module, controller, and action.
   *
   */
  nr_txn_set_path("Magento", NRPRG(txn), txn_name, NR_PATH_TYPE_ACTION,
                  NR_OK_TO_OVERWRITE);

leave:
  nr_php_zval_free(&request);
  nr_php_zval_free(&module);
  nr_php_zval_free(&controller);
  nr_php_zval_free(&action);
  nr_free(txn_name);
}
