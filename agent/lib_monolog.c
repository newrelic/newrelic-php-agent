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
#include "util_sleep.h"

/*
 * Purpose : Return a copy of Monolog\Logger::API.
 *
 * Params  : 1. An instance of Monolog\Logger.
 *
 * Returns : The Monolog version number as an integer
 */
static int nr_monolog_version(zval* logger TSRMLS_DC) {
  int retval = 0;
  zval* api = NULL;
  zend_class_entry* ce = NULL;

  if (0 == nr_php_is_zval_valid_object(logger)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: Logger object is invalid", __func__);
    return 0;
  }

  ce = Z_OBJCE_P(logger);
  if (NULL == ce) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: Logger has NULL class entry",
                     __func__);
    return 0;
  }

  api = nr_php_get_class_constant(ce, "API");
  if (NULL == api) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: Logger does not have API", __func__);
    return 0;
  }

  if (nr_php_is_zval_valid_integer(api)) {
    retval = Z_LVAL_P(api);
  } else {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: expected API be a valid int, got type %d", __func__,
                     Z_TYPE_P(api));
  }

  nr_php_zval_free(&api);
  return retval;
}

/*
 * Purpose : Convert $level argument of Monolog\Logger::addRecord to a string
 * representation of Monolog's log level.
 *
 * Params  : Logger instance and Monolog\Logger::addRecord argument list
 *
 * Returns : A new string with Monolog's log level name
 */
static char* nr_monolog_get_level_name(zval* logger,
                                       NR_EXECUTE_PROTO TSRMLS_DC) {
  zval* level_name = NULL;
  char* level_name_string = nr_strdup("UNKNOWN");

  if (!nr_php_object_has_method(logger, "getLevelName" TSRMLS_CC)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: Logger does not have getLevelName method", __func__);
    return level_name_string;
  }

  zval* level = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  level_name = nr_php_call(logger, "getLevelName", level);
  if (nr_php_is_zval_valid_string(level_name)) {
    level_name_string = nr_strdup(Z_STRVAL_P(level_name));
  } else {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: expected level_name be a valid string, got type %d",
                     __func__, Z_TYPE_P(level_name));
  }

  nr_php_zval_free(&level_name);
  nr_php_arg_release(&level);

  return level_name_string;
}

/*
 * Purpose : Create timestamp for the log event by inspecting $datetime argument
 * of Monolog\Logger::addRecord
 *
 * Params  : Logger API, # of Monolog\Logger::addRecord arguments, and
 * Monolog\Logger::addRecord argument list
 *
 * Returns : timestamp in milliseconds calculated from $datetime if available,
 * current time otherwise.
 */
static nrtime_t nr_monolog_get_timestamp(int api,
                                         size_t argc,
                                         NR_EXECUTE_PROTO TSRMLS_DC) {
  nrtime_t timestamp = nr_get_time();

  zval* datetime = NULL;
  /* $datetime is only available since API level 2 */
  if (2 <= api && 4 <= argc) {
    datetime = nr_php_arg_get(4, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
    if (nr_php_is_zval_valid_object(datetime)
        && nr_php_object_has_method(datetime, "format" TSRMLS_CC)) {
      /* call $datetime->format("Uv"); to get $datetime in milliseconds */
      zval* fmt_datetime_milliseconds = nr_php_zval_alloc();
      nr_php_zval_str(fmt_datetime_milliseconds, "Uv");
      zval* datetime_milliseconds
          = nr_php_call(datetime, "format", fmt_datetime_milliseconds);
      nr_php_zval_free(&fmt_datetime_milliseconds);

      if (nr_php_is_zval_valid_string(datetime_milliseconds)) {
        /* convert string to nrtime_t */
        timestamp = nr_parse_unix_time(Z_STRVAL_P(datetime_milliseconds));
      }
      nr_php_zval_free(&datetime_milliseconds);
    }
  }
  if (datetime)
    nr_php_arg_release(&datetime);

  return timestamp;
}

NR_PHP_WRAPPER(nr_monolog_logger_addrecord) {
  (void)wraprec;

  /* Get Monolog API level */
  zval* this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  int api = nr_monolog_version(this_var TSRMLS_CC);

  /* Get values of $level and $message arguments */

  char* level_name
      = nr_monolog_get_level_name(this_var, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  zval* message = nr_php_arg_get(2, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  size_t argc = nr_php_get_user_func_arg_count(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  /* Get values of optional arguments: $context and $datetime */
  zval* context = NULL;

  if (3 <= argc) {
    context = nr_php_arg_get(3, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
    nrl_verbosedebug(
        NRL_INSTRUMENT, "%s: typeof(context)=[%d], len(context)=%zu", __func__,
        Z_TYPE_P(context), nr_php_zend_hash_num_elements(Z_ARRVAL_P(context)));
  }

  nrtime_t timestamp
      = nr_monolog_get_timestamp(api, argc, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  nrl_verbosedebug(NRL_INSTRUMENT,
                   "%s: #args = %zu, Monolog API: [%d], level=[%s], "
                   "message=[%s], timestamp=[%" PRIu64 "]",
                   __func__, argc, api, level_name, Z_STRVAL_P(message),
                   timestamp);

  /* construct the log_event from level, message, context and datetime */

  nr_free(level_name);

  NR_PHP_WRAPPER_CALL
  if (context)
    nr_php_arg_release(&context);
  nr_php_arg_release(&message);
  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

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
  nr_php_wrap_user_function(NR_PSTR("Monolog\\Logger::addRecord"),
                            nr_monolog_logger_addrecord TSRMLS_CC);
}
