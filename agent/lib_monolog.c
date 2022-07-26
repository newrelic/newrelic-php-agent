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
 * Purpose : Convert Monolog\Logger::API to integer
 *
 * Params  : 1. An instance of Monolog\Logger.
 *
 * Returns : The Monolog version number as an integer
 */
static int nr_monolog_version(const zval* logger TSRMLS_DC) {
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
 * Returns : A new string with Monolog's log level name; caller must free
 */
static char* nr_monolog_get_level_name(zval* logger,
                                       NR_EXECUTE_PROTO TSRMLS_DC) {
  zval* level_name = NULL;
  char* level_name_string = NULL;

  if (!nr_php_object_has_method(logger, "getLevelName" TSRMLS_CC)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: Logger does not have getLevelName method", __func__);
  } else {
    zval* level = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
    if (NULL == level) {
      nrl_verbosedebug(NRL_INSTRUMENT,
                       "%s: $level not defined, unable to get log level name",
                       __func__);
    } else {
      level_name = nr_php_call(logger, "getLevelName", level);
    }
    nr_php_arg_release(&level);
    if (NULL == level_name) {
      nrl_verbosedebug(NRL_INSTRUMENT, "%s: expected level_name be valid",
                       __func__);
    } else if (!nr_php_is_zval_valid_string(level_name)) {
      nrl_verbosedebug(NRL_INSTRUMENT,
                       "%s: expected level_name be a valid string, got type %d",
                       __func__, Z_TYPE_P(level_name));
    } else {
      level_name_string = nr_strdup(Z_STRVAL_P(level_name));
    }
    nr_php_zval_free(&level_name);
  }

  return level_name_string != NULL ? level_name_string : nr_strdup("UNKNOWN");
}

/*
 * Purpose : Convert $message argument of Monolog\Logger::addRecord to a string
 *
 * Params  : Monolog\Logger::addRecord argument list
 *
 * Returns : A new string with Monolog's log message; caller must free
 */

static char* nr_monolog_get_message(NR_EXECUTE_PROTO TSRMLS_DC) {
  char* message = NULL;
  zval* message_arg = NULL;

  message_arg = nr_php_arg_get(2, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (NULL == message_arg) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: $message not defined, unable to get log message",
                     __func__);
    message = nr_strdup("");
  } else if (!nr_php_is_zval_valid_string(message_arg)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: expected $message be a valid string, got type %d",
                     __func__, Z_TYPE_P(message_arg));
    message = nr_strdup("");
  } else {
    message = nr_strdup(Z_STRVAL_P(message_arg));
  }
  nr_php_arg_release(&message_arg);

  return message;
}

/*
 * Purpose : Format key of $context array's element as string
 *
 * Params  : zend_hash_key
 * *
 * Returns : A new string representing zval; caller must free
 *
 */
static char* nr_monolog_fmt_context_key(const zend_hash_key* hash_key) {
  char* key_str = NULL;
  zval* key = nr_php_zval_alloc();
  if (nr_php_zend_hash_key_is_string(hash_key)) {
    nr_php_zval_str(key, nr_php_zend_hash_key_string_value(hash_key));
    key_str = nr_formatf("%s", Z_STRVAL_P(key));
  } else if (nr_php_zend_hash_key_is_numeric(hash_key)) {
    ZVAL_LONG(key, (zend_long)nr_php_zend_hash_key_integer(hash_key));
    key_str = nr_formatf("%ld", (long)Z_LVAL_P(key));
  } else {
    /*
     * This is a warning because this really, really shouldn't ever happen.
     */
    nrl_warning(NRL_INSTRUMENT, "%s: unexpected key type", __func__);
    key_str = nr_formatf("unsupported-key-type");
  }
  nr_php_zval_free(&key);
  return key_str;
}

/*
 * Purpose : Format value of $context array's  element as string
 *
 * Params  : zval value
 * *
 * Returns : A new string representing zval; caller must free
 *
 */
static char* nr_monolog_fmt_context_value(zval* zv) {
  char* val_str = NULL;
  zval* zv_str = NULL;

  if (NULL == zv) {
    return nr_strdup("");
  }

  zv_str = nr_php_zval_alloc();
  if (NULL == zv_str) {
    return nr_strdup("");
  }

  ZVAL_DUP(zv_str, zv);
  convert_to_string(zv_str);
  val_str = nr_strdup(Z_STRVAL_P(zv_str));
  nr_php_zval_free(&zv_str);

  return val_str;
}

/*
 * Purpose : Format an element of $context array as "key => value" string
 *
 * Params  : zval value, pointer to string buffer to store formatted output
 * and hash key
 *
 * Side effect : string buffer is reallocated with each call.
 *
 * Returns : ZEND_HASH_APPLY_KEEP to keep iteration
 *
 */
static int nr_monolog_fmt_context_item(zval* value,
                                       char** strbuf,
                                       zend_hash_key* hash_key TSRMLS_DC) {
  NR_UNUSED_TSRMLS;
  char* key = nr_monolog_fmt_context_key(hash_key);
  char* val = nr_monolog_fmt_context_value(value);

  char* kv_str = nr_formatf("%s => %s", key, val);
  nr_free(key);
  nr_free(val);

  char* sep = nr_strlen(*strbuf) > 1 ? ", " : "";
  *strbuf = nr_str_append(*strbuf, kv_str, sep);
  nr_free(kv_str);

  return ZEND_HASH_APPLY_KEEP;
}

/*
 * Purpose : Iterate over $context array and format each element
 *
 * Params  : string buffer to store formatted output and
 * Monolog\Logger::addRecord argument list
 *
 * Returns : A new string with Monolog's log context
 */
static char* nr_monolog_fmt_context(char* strbuf,
                                    HashTable* context TSRMLS_DC) {
  strbuf = nr_str_append(strbuf, "[", "");

  nr_php_zend_hash_zval_apply(context,
                              (nr_php_zval_apply_t)nr_monolog_fmt_context_item,
                              (void*)&strbuf TSRMLS_CC);

  return nr_str_append(strbuf, "]", "");
}

/*
 * Purpose : Convert $context argument of Monolog\Logger::addRecord to a string
 *
 * Params  : # of Monolog\Logger::addRecord arguments, and
 * Monolog\Logger::addRecord argument list
 *
 * Returns : A new string with Monolog's log context
 */
static char* nr_monolog_get_context(const size_t argc,
                                    NR_EXECUTE_PROTO TSRMLS_DC) {
  char* context = nr_strdup("");
  zval* context_arg = NULL;

  if (3 > argc) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: $context not available", __func__);
    goto return_context;
  }

  context_arg = nr_php_arg_get(3, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  if (NULL == context_arg) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: $context not defined, unable to get log context",
                     __func__);
    goto return_context;
  } else if (!nr_php_is_zval_valid_array(context_arg)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: expected $context be a valid array, got type %d",
                     __func__, Z_TYPE_P(context_arg));
    goto return_context;
  }

  if (0 == nr_php_zend_hash_num_elements(Z_ARRVAL_P(context_arg))) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: $context has no elements", __func__);
    goto return_context;
  }

  context = nr_monolog_fmt_context(context, Z_ARRVAL_P(context_arg) TSRMLS_CC);

return_context:
  nr_php_arg_release(&context_arg);
  return context;
}

/*
 * Purpose : Combine $message and $context arguments of
 * Monolog\Logger::addRecord into a single string to be used as a message
 * property of the log event.
 *
 * Params  : # of Monolog\Logger::addRecord arguments, and
 * Monolog\Logger::addRecord argument list
 *
 * Returns : A new string with a log record message; caller must free
 */
static char* nr_monolog_build_message(const size_t argc,
                                      NR_EXECUTE_PROTO TSRMLS_DC) {
  char* message_and_context = nr_strdup("");

  char* message = nr_monolog_get_message(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  message_and_context = nr_str_append(message_and_context, message, "");
  nr_free(message);

  char* context = nr_monolog_get_context(argc, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_strempty(context)) {
    message_and_context = nr_str_append(message_and_context, context, " ");
  }
  nr_free(context);

  return message_and_context;
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
static nrtime_t nr_monolog_get_timestamp(const int monolog_api,
                                         const size_t add_record_argc,
                                         NR_EXECUTE_PROTO TSRMLS_DC) {
  nrtime_t timestamp = nr_get_time();

  zval* datetime = NULL;
  /* $datetime is only available since API level 2 */
  if (2 <= monolog_api && 4 <= add_record_argc) {
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

  size_t argc = nr_php_get_user_func_arg_count(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  char* message
      = nr_monolog_build_message(argc, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  nrtime_t timestamp
      = nr_monolog_get_timestamp(api, argc, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  nrl_verbosedebug(NRL_INSTRUMENT,
                   "%s: #args = %zu, Monolog API: [%d], level=[%s], "
                   "message=[%s], timestamp=[%" PRIu64 "]",
                   __func__, argc, api, level_name, message, timestamp);

  /* Record the log event */
  nr_txn_record_log_event(NRPRG(txn), level_name, message, timestamp, NRPRG(app));

  nr_free(level_name);
  nr_free(message);

  NR_PHP_WRAPPER_CALL

  nr_php_scope_release(&this_var);
}

NR_PHP_WRAPPER(nr_monolog_logger_pushhandler) {
  (void)wraprec;

  zval* handler = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_valid_object(handler)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: handler is not an object", __func__);
    goto end;
  }

  if (nr_txn_log_forwarding_enabled(NRPRG(txn))
      && nr_php_object_instanceof_class(
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

