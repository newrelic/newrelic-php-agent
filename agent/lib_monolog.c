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
#include "lib_monolog_private.h"
#include "nr_datastore_instance.h"
#include "nr_segment_datastore.h"
#include "nr_txn.h"
#include "util_logging.h"
#include "util_object.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_sleep.h"

/*
 * Define name of log decorating processor function
 */
#define LOG_DECORATE_NAMESPACE "Newrelic\\Monolog"
#define LOG_DECORATE_NAMESPACE_LC "newrelic\\monolog"
#define LOG_DECORATE_PROC_FUNC_NAME \
  "newrelic_phpagent_monolog_decorating_processor"

// clang-format off
/*
 * This macro affects how instrumentation $context argument of
 * Monolog\Logger::addRecord works:
 *
 * 0 - $context argument will not be instrumented: its existance and value
 *     are ignored 
 * 1 - the message of the log record forwarded by the agent will have the value
 *     of $context appended to the value of $message.
 */
// clang-format on
#define HAVE_CONTEXT_IN_MESSAGE 0

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

#if HAVE_CONTEXT_IN_MESSAGE
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
#endif

/*
 * Purpose : Convert a zval value from context data to a nrobj_t
 *
 * Params  : zval
 *
 * Returns : nrobj_t* holding converted value
 *           NULL otherwise
 *
 * Notes   : Only scalar and string types are supported.
 *           Nested arrays are not converted and are ignored.
 *           Other zval types are also ignored.
 */
nrobj_t* nr_monolog_context_data_zval_to_attribute_obj(
    const zval* z TSRMLS_DC) {
  nrobj_t* retobj = NULL;

  if (NULL == z) {
    return NULL;
  }

  nr_php_zval_unwrap(z);

  switch (Z_TYPE_P(z)) {
    case IS_NULL:
      retobj = NULL;
      break;

    case IS_LONG:
      retobj = nro_new_long((long)Z_LVAL_P(z));
      break;

    case IS_DOUBLE:
      retobj = nro_new_double(Z_DVAL_P(z));
      break;

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
    case IS_TRUE:
      retobj = nro_new_boolean(true);
      break;

    case IS_FALSE:
      retobj = nro_new_boolean(false);
      break;
#else
    case IS_BOOL:
      retobj = nro_new_boolean(Z_BVAL_P(z));
      break;
#endif /* PHP 7.0+ */

    case IS_STRING:
      if (!nr_php_is_zval_valid_string(z)) {
        retobj = NULL;
      } else {
        retobj = nro_new_string(Z_STRVAL_P(z));
      }
      break;

    default:
      /* any other type conversion to attribute not supported */
      retobj = NULL;
      break;
  }

  return retobj;
}

/*
 * Purpose : Extract $context argument of Monolog\Logger::addRecord to
 * attributes
 *
 * Params  : # of Monolog\Logger::addRecord arguments, and
 * Monolog\Logger::addRecord argument list
 *
 * Returns : zval* for context array on success (must be freed by caller)
 *           NULL otherwise
 *
 */
static zval* nr_monolog_extract_context_data(const size_t argc,
                                             NR_EXECUTE_PROTO TSRMLS_DC) {
  zval* context_arg;

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

return_context:
  return context_arg;
}

/*
 * Purpose : Convert $context array of Monolog\Logger::addRecord to
 * attributes
 *
 * Params  : zval* for context array from Monolog
 *
 * Returns : json representation on success
 *           NULL otherwise
 *
 */
nr_attributes_t* nr_monolog_convert_context_data_to_attributes(
    zval* context_data TSRMLS_DC) {
  zend_string* key;
  zval* val;

  nr_attributes_t* attributes = NULL;
  nr_status_t st;

  if (NULL == context_data) {
    return NULL;
  }

  attributes = nr_attributes_create(NRPRG(txn)->attribute_config);

  ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARR_P(context_data), key, val) {
    (void)key;
    (void)val;

    if (NULL == key) {
      continue;
    }

    nrobj_t* obj = nr_monolog_context_data_zval_to_attribute_obj(val);

    if (NULL != obj) {
      char* buf = nr_formatf(NR_TXN_LOG_CONTEXT_DATA_ATTRIBUTE_PREFIX "%s",
                             ZSTR_VAL(key));
      st = nr_attributes_user_add(attributes, NR_ATTRIBUTE_DESTINATION_LOG, buf,
                                  obj);
      if (NR_SUCCESS != st) {
        nrl_warning(NRL_AGENT, "failed to add monolog context attribute key %s",
                    ZSTR_VAL(key));
      }
      nr_free(buf);
      nro_delete(obj);
    }
  }
  ZEND_HASH_FOREACH_END();

  return attributes;
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
#if !HAVE_CONTEXT_IN_MESSAGE
  /* Make the compiler happy - argc is not used when $context is ignored */
  (void)argc;
#endif
  char* message_and_context = nr_strdup("");

  char* message = nr_monolog_get_message(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  message_and_context = nr_str_append(message_and_context, message, "");
  nr_free(message);

#if HAVE_CONTEXT_IN_MESSAGE
  char* context = nr_monolog_get_context(argc, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_strempty(context)) {
    message_and_context = nr_str_append(message_and_context, context, " ");
  }
  nr_free(context);
#endif

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

  if (!nr_txn_log_forwarding_enabled(NRPRG(txn))
      && !nr_txn_log_metrics_enabled(NRPRG(txn))) {
    goto skip_instrumentation;
  }

  /* This code executes when at least one logging feature is enabled and
   * log level is neeeded in both features so agent will always need
   * to get the log level value */
  zval* this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  char* level_name
      = nr_monolog_get_level_name(this_var, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  int api = 0;
  size_t argc = 0;
  char* message = NULL;
  nr_attributes_t* context_attributes = NULL;
  nrtime_t timestamp = nr_get_time();

  /* Values of $message and $timestamp arguments are needed only if log
   * forwarding is enabled so agent will get them conditionally */
  if (nr_txn_log_forwarding_enabled(NRPRG(txn))) {
    argc = nr_php_get_user_func_arg_count(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
    message = nr_monolog_build_message(argc, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

    if (nr_txn_log_forwarding_context_data_enabled(NRPRG(txn))) {
      zval* context_data = nr_monolog_extract_context_data(
          argc, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
      context_attributes
          = nr_monolog_convert_context_data_to_attributes(context_data);
      nr_php_arg_release(&context_data);
    }
    api = nr_monolog_version(this_var TSRMLS_CC);
    timestamp
        = nr_monolog_get_timestamp(api, argc, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  }

  /* Record the log event */
  nr_txn_record_log_event(NRPRG(txn), level_name, message, timestamp,
                          context_attributes, NRPRG(app));

  nr_free(level_name);
  nr_free(message);

  nr_php_scope_release(&this_var);

skip_instrumentation:
  NR_PHP_WRAPPER_CALL
}
NR_PHP_WRAPPER_END

/*
 * Create processor function used for log decorating as needed
 */
static int nr_monolog_create_decorate_processor_function(TSRMLS_D) {
  int retval = SUCCESS;
  zend_function* processor_func = NULL;

  /* see if processor function exists and if not create */
  processor_func = nr_php_find_function(LOG_DECORATE_NAMESPACE_LC "\\" LOG_DECORATE_PROC_FUNC_NAME);
  if (NULL == processor_func) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "Creating Monolog decorating processor func");

    /* this function will add NR-LINKING data to the 'extra' array
     * entry in the log record.  It is careful to confirm all the
     * expected linking metadata is present as well as escaping
     * special chars in the entity.name */
    retval = zend_eval_string(
        "namespace " LOG_DECORATE_NAMESPACE
        ";"
        "function " LOG_DECORATE_PROC_FUNC_NAME
        "($record) {"
        "    $linkmeta = newrelic_get_linking_metadata();"
        "    $guid = $linkmeta['entity.guid'] ?? '';"
        "    $hostname = $linkmeta['hostname'] ?? '';"
        "    $traceid = $linkmeta['trace.id'] ?? '';"
        "    $spanid = $linkmeta['span.id'] ?? '';"
        "    $name = $linkmeta['entity.name'] ?? '';"
        "    $name = urlencode($name);"
        "    $data = 'NR-LINKING|' . $guid . '|' . $hostname . '|' ."
        "             $traceid . '|' . $spanid . '|' . $name . '|';"
        "    $record['extra']['NR-LINKING'] = $data;"
        "    return $record;"
        "}",
        NULL, "newrelic/Monolog/" LOG_DECORATE_PROC_FUNC_NAME TSRMLS_CC);

    if (SUCCESS != retval) {
      nrl_warning(NRL_FRAMEWORK,
                  "%s: error creating Monolog decorating processor function!",
                  __func__);
    }
  } else {
    nrl_verbosedebug(NRL_INSTRUMENT, "Using existing Monolog decorating processor func");
  }

  return retval;
}

NR_PHP_WRAPPER(nr_monolog_logger_pushhandler) {
  (void)wraprec;

  zval* handler = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_valid_object(handler)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: handler is not an object", __func__);
    goto end;
  }

  nrl_verbosedebug(
      NRL_INSTRUMENT, "%s : Monolog handler = %*s", __func__,
      NRSAFELEN(nr_php_class_entry_name_length(Z_OBJCE_P(handler))),
      nr_php_class_entry_name(Z_OBJCE_P(handler)));

  if (nr_txn_log_forwarding_enabled(NRPRG(txn))
      && nr_php_object_instanceof_class(
          handler, "NewRelic\\Monolog\\Enricher\\Handler" TSRMLS_CC)) {
    nrl_warning(NRL_INSTRUMENT,
                "detected NewRelic\\Monolog\\Enricher\\Handler. The "
                "application may be sending logs to New Relic twice.");
  }

  if (nr_txn_log_decorating_enabled(NRPRG(txn))) {
    zval* callback_name = NULL;
    zval* ph_retval = NULL;

    /* Verify the handler implements pushProcessor () */
    if (!nr_php_object_has_method(handler, "pushProcessor" TSRMLS_CC)) {
      nrl_warning(
          NRL_INSTRUMENT,
          "Monolog handler %*s does not implement the pushProcessor() "
          "method so log decoration will not occur!",
          NRSAFELEN(nr_php_class_entry_name_length(Z_OBJCE_P(handler))),
          nr_php_class_entry_name(Z_OBJCE_P(handler)));

      goto end;
    }

    /* Create function used to decorate Monolog log records */
    nr_monolog_create_decorate_processor_function();

    /*
     * Actually call pushProcessor
     */
    callback_name = nr_php_zval_alloc();
    nr_php_zval_str(callback_name,
                    LOG_DECORATE_NAMESPACE "\\" LOG_DECORATE_PROC_FUNC_NAME);

    ph_retval = nr_php_call(handler, "pushProcessor", callback_name TSRMLS_CC);
    if (!nr_php_is_zval_true(ph_retval)) {
      nrl_warning(
          NRL_FRAMEWORK,
          "%s: error registering Monolog decorating processor function!",
          __func__);
    } else {
      nrl_verbosedebug(NRL_INSTRUMENT,
                       "Monolog log decorating processor registered");
    }

    nr_php_zval_free(&ph_retval);
    nr_php_zval_free(&callback_name);
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
