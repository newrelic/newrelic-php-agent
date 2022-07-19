/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_user_instrument.h"
#include "php_datastore.h"
#include "php_execute.h"
#include "php_hash.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "nr_datastore_instance.h"
#include "nr_segment_datastore.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

NR_PHP_WRAPPER(nr_monolog_logger_pushhandler) {
  (void)wraprec;

  zval* handler = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_valid_object(handler)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: handler is not an object", __func__);
    goto end;
  }

  if (/*NRINI(log_forwarding_enabled) && */ nr_php_object_instanceof_class(
      handler, "NewRelic\\Monolog\\Enricher\\Handler" TSRMLS_CC)) {
    nrl_warning(NRL_INSTRUMENT,
                "detected NewRelic\\Monolog\\Enricher\\Handler. The "
                "application may be sending logs to New Relic twice.");
  }

end:
  NR_PHP_WRAPPER_CALL
  nr_php_arg_release(&handler);
}
NR_PHP_WRAPPER_END

void nr_monolog_enable(TSRMLS_D) {
  nr_php_wrap_user_function(NR_PSTR("Monolog\\Logger::pushHandler"),
                            nr_monolog_logger_pushhandler TSRMLS_CC);
}
