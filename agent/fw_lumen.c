/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_user_instrument.h"
#include "php_error.h"
#include "php_execute.h"
#include "php_globals.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "php_hash.h"
#include "fw_lumen.h"
#include "fw_laravel.h"
#include "fw_laravel_queue.h"
#include "fw_support.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

#include "ext/standard/php_versioning.h"
#include "Zend/zend_exceptions.h"


void nr_lumen_enable(TSRMLS_D) {
  /*
   * We set the path to 'unknown' to prevent having to name routing errors.
   * This follows what is done in symfony2.
   */
  nr_txn_set_path("Lumen", NRPRG(txn), "unknown", NR_PATH_TYPE_ACTION,
                  NR_NOT_OK_TO_OVERWRITE);



}
