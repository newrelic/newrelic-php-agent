/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file contains the error callback handler.
 * This is called to record errors for sending to RPM.
 */
#include "php_agent.h"
#include "php_call.h"
#include "php_error.h"
#include "php_globals.h"
#include "php_hooks.h"
#include "php_zval.h"
#include "fw_support.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_object.h"
#include "util_strings.h"
#include "zend_exceptions.h"

/*
 * Wrap filter functions so they can be safely stored in a zend_llist.
 * Converting a function pointer to a pointer to void is undefined in C,
 * and conforming implementations are required to issue a warning if a
 * pointer to void is converted to a function pointer. Technically, POSIX
 * does allow such conversions, but better safe than sorry.
 */
typedef struct _nr_php_exception_filter_t {
  nr_php_exception_filter_fn fn;
} nr_php_exception_filter_t;

void nr_php_exception_filters_init(zend_llist* chain) {
  if (chain) {
    zend_llist_init(chain, sizeof(nr_php_exception_filter_t), NULL, 0);
  }
}

void nr_php_exception_filters_destroy(zend_llist* chain) {
  if (chain) {
    zend_llist_clean(chain);
  }
}

static int nr_php_exception_filters_compare(void* a, void* b) {
  nr_php_exception_filter_t* fa = (nr_php_exception_filter_t*)a;
  nr_php_exception_filter_t* fb = (nr_php_exception_filter_t*)b;

  /*
   * zend_llist supports tri-valued comparisons for sorting purposes, but
   * the only well-defined comparisons for function pointers are equality
   * and inequality. Since we never sort the list, we can satisify the
   * interface so long as zero is returned for equality and non-zero for
   * inequality.
   */
  if (fa && fb && (fa->fn == fb->fn)) {
    return 0;
  }
  if ((0 == fa) && (0 == fb)) {
    return 0;
  }
  return 1;
}

nr_status_t nr_php_exception_filters_add(zend_llist* chain,
                                         nr_php_exception_filter_fn fn) {
  nr_php_exception_filter_t filter;

  if ((NULL == chain) || (NULL == fn)) {
    return NR_FAILURE;
  }

  nr_memset(&filter, 0, sizeof(filter));
  filter.fn = fn;

  zend_llist_add_element(chain, &filter);
  return NR_SUCCESS;
}

nr_status_t nr_php_exception_filters_remove(zend_llist* chain,
                                            nr_php_exception_filter_fn fn) {
  if (chain) {
    nr_php_exception_filter_t x;

    nr_memset(&x, 0, sizeof(x));
    x.fn = fn;
    zend_llist_del_element(chain, &x, nr_php_exception_filters_compare);
    return NR_SUCCESS;
  }
  return NR_FAILURE;
}

static nr_php_exception_action_t nr_php_exception_filters_apply(
    zend_llist* chain,
    zval* exception TSRMLS_DC) {
  zend_llist_position pos;
  nr_php_exception_filter_t* elt;

  elt = (nr_php_exception_filter_t*)zend_llist_get_first_ex(chain, &pos);

  /*
   * Give each filter a chance to prevent this exception from being reported.
   */
  while (pos) {
    if (elt && elt->fn) {
      if (NR_PHP_EXCEPTION_FILTER_IGNORE == elt->fn(exception TSRMLS_CC)) {
        return NR_PHP_EXCEPTION_FILTER_IGNORE;
      }
    }

    elt = (nr_php_exception_filter_t*)zend_llist_get_next_ex(chain, &pos);
  }

  return NR_PHP_EXCEPTION_FILTER_REPORT;
}

nr_php_exception_action_t nr_php_ignore_exceptions_ini_filter(
    zval* exception TSRMLS_DC) {
  nrobj_t* names;
  nr_php_exception_action_t action;

  if (0 == nr_php_is_zval_valid_object(exception)) {
    return NR_PHP_EXCEPTION_FILTER_REPORT;
  }

  action = NR_PHP_EXCEPTION_FILTER_REPORT;
  names = nr_strsplit(NRINI(ignore_exceptions), ",", 0 /* discard empty */);

  for (int i = 1, n = nro_getsize(names); i <= n; i++) {
    const char* name = nro_get_array_string(names, i, NULL);

    if (name
        && nr_php_class_entry_instanceof_class(Z_OBJCE_P(exception),
                                               name TSRMLS_CC)) {
      action = NR_PHP_EXCEPTION_FILTER_IGNORE;
      break;
    }
  }

  nro_delete(names);
  return action;
}

PHP_FUNCTION(newrelic_exception_handler) {
  zval* exception = NULL;

  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  if ((FAILURE
       == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET,
                                   ZEND_NUM_ARGS() TSRMLS_CC, "z", &exception))
      || (NULL == exception)) {
    /*
     * There isn't much useful that we can do here. Let's log an error
     * and return.
     */
    nrl_warning(NRL_ERROR,
                "newrelic_exception_handler: parameter is not a valid zval");
    zend_error(E_ERROR, "Uncaught exception");

    /*
     * zend_error won't return for an E_ERROR, but just in case.
     */
    return;
  }

  /*
   * Let's use this exception to generate an error. The error priority is set
   * to NR_PHP_ERROR_PRIORITY_UNCAUGHT_EXCEPTION to override anything else,
   * _including_ API noticed errors (in case the user uses newrelic_notice_error
   * as their error handler with prioritize_api_errors enabled).
   */
  nr_php_error_record_exception(
      NRPRG(txn), exception, NR_PHP_ERROR_PRIORITY_UNCAUGHT_EXCEPTION,
      "Uncaught exception ", &NRPRG(exception_filters) TSRMLS_CC);

  /*
   * Finally, we need to generate an E_ERROR to match what PHP would have done
   * if this handler wasn't installed. Happily, PHP exposes an API function
   * that we can use to do this, rather than having to replicate that logic
   * ourselves.
   */
#ifdef PHP7
  zend_exception_error(Z_OBJ_P(exception), E_ERROR TSRMLS_CC);
#else
  zend_exception_error(exception, E_ERROR TSRMLS_CC);
#endif /* PHP7 */
}

int nr_php_error_get_priority(int type) {
  switch (type) {
    case E_PARSE:
      return 50;
    case E_COMPILE_ERROR:
      return 50;
    case E_CORE_ERROR:
      return 50;
    case E_USER_ERROR:
      return 50;
    case E_ERROR:
      return 50;
    case E_COMPILE_WARNING:
      return 40;
    case E_CORE_WARNING:
      return 40;
    case E_USER_WARNING:
      return 40;
    case E_WARNING:
      return 40;
    case E_USER_NOTICE:
      return 0;
    case E_NOTICE:
      return 0;
    default:
      return 20;
  }
}

void nr_php_error_install_exception_handler(TSRMLS_D) {
  int has_user_exception_handler;

  /*
   * Not calling set_exception_handler() here is intentional: we don't want to
   * generate useless supportability metrics here, nor do we want to risk
   * errors filtering up to the user.
   *
   * Firstly, we need to check the no_exception_handler special: if that's set,
   * then we don't want to do anything anyway.
   */
  if (NR_PHP_PROCESS_GLOBALS(special_flags).no_exception_handler) {
    return;
  }

  /*
   * Although we shouldn't have a scenario in which there's an exception
   * handler installed and this function is called, we'll handle that case
   * anyway in case another extension is trying to do the same thing.
   */
#ifdef PHP7
  has_user_exception_handler = (IS_UNDEF != Z_TYPE(EG(user_exception_handler)));
#else
  has_user_exception_handler = (NULL != EG(user_exception_handler));
#endif /* PHP7 */

  if (has_user_exception_handler) {
    nrl_verbosedebug(NRL_ERROR,
                     "%s: unexpected user_exception_handler already installed, "
                     "pushing it onto the exception handler stack and "
                     "installing ours instead",
                     __func__);

    /*
     * All we have to do is push the existing handler onto the
     * user_exception_handlers stack. We don't need to copy it: ownership of
     * the pointer simply passes from executor_globals to the stack.
     */
#ifdef PHP7
    zend_stack_push(&EG(user_exception_handlers), &EG(user_exception_handler));
#else
    zend_ptr_stack_push(&EG(user_exception_handlers),
                        EG(user_exception_handler));
#endif /* PHP7 */
  }

  /*
   * Actually allocate and set the user_exception_handler zval. PHP itself
   * will destroy this at the end of the request.
   */
#ifdef PHP7
  nr_php_zval_str(&EG(user_exception_handler), "newrelic_exception_handler");
#else
  ALLOC_INIT_ZVAL(EG(user_exception_handler));
  nr_php_zval_str(EG(user_exception_handler), "newrelic_exception_handler");
#endif
}

/*
 * Purpose : Get the stack trace for an exception.
 *
 * Params  : 1. The exception to get the stack trace from. This argument is not
 *              checked in any way, and is assumed to be a valid Exception
 *              object.
 *
 * Returns : A zval for the stack trace, which the caller will need to destroy,
 *           or NULL if no trace is available.
 */
static zval* nr_php_error_exception_stack_trace(zval* exception TSRMLS_DC) {
  zval* trace;

  trace = nr_php_call(exception, "getTrace");
  if (!nr_php_is_zval_valid_array(trace)) {
    nr_php_zval_free(&trace);
    return NULL;
  }

  return trace;
}

/*
 * Purpose : Wrapper for Exception::getFile().
 *
 * Params  : 1. The exception to get the file name from. This argument is not
 *              checked in any way, and is assumed to be a valid Exception
 *              object.
 *
 * Returns : A NULL terminated string containing the file name, which the caller
 *           will need to free, or NULL if no file name is available.
 */
static char* nr_php_error_exception_file(zval* exception TSRMLS_DC) {
  zval* file_zv = nr_php_call(exception, "getFile");
  char* file = NULL;

  if (nr_php_is_zval_valid_string(file_zv)) {
    file = nr_strndup(Z_STRVAL_P(file_zv), Z_STRLEN_P(file_zv));
  }

  nr_php_zval_free(&file_zv);
  return file;
}

/*
 * Purpose : Wrapper for Exception::getLine().
 *
 * Params  : 1. The exception to get the line number from. This argument is not
 *              checked in any way, and is assumed to be a valid Exception
 *              object.
 *
 * Returns : The 1-indexed line number, or 0 on error.
 */
static long nr_php_error_exception_line(zval* exception TSRMLS_DC) {
  long line = 0;
  zval* line_zv = nr_php_call(exception, "getLine");

  /*
   * All scalar types can be coerced to IS_LONG.
   */
  if (nr_php_is_zval_valid_scalar(line_zv)) {
    convert_to_long(line_zv);
    line = Z_LVAL_P(line_zv);
  }

  nr_php_zval_free(&line_zv);
  return line;
}

/*
 * Purpose : Extract a useful message from an exception object.
 *
 * Params  : 1. The exception to get a message from. This argument is not
 *              checked in any way, and is assumed to be a valid Exception
 *              object.
 *
 * Returns : A NULL terminated string containing the message, which the caller
 *           will need to free, or NULL if no message could be extracted.
 */
static char* nr_php_error_exception_message(zval* exception TSRMLS_DC) {
  /*
   * This intentionally prefers getMessage(): __toString() can include stack
   * dumps generated by PHP, which can include user data that we don't want to
   * send up and for which it isn't obvious that it would be sent.
   */
  zval* message_zv = nr_php_call(exception, "getMessage");
  char* message = NULL;

  if (nr_php_is_zval_valid_string(message_zv)) {
    message = nr_strndup(Z_STRVAL_P(message_zv), Z_STRLEN_P(message_zv));
  }

  nr_php_zval_free(&message_zv);
  return message;
}

static const char* get_error_type_string(int type) {
  switch (type) {
    case E_ERROR:
      return "E_ERROR";
    case E_WARNING:
      return "E_WARNING";
    case E_PARSE:
      return "E_PARSE";
    case E_NOTICE:
      return "E_NOTICE";
    case E_CORE_ERROR:
      return "E_CORE_ERROR";
    case E_CORE_WARNING:
      return "E_CORE_WARNING";
    case E_COMPILE_ERROR:
      return "E_COMPILE_ERROR";
    case E_COMPILE_WARNING:
      return "E_COMPILE_WARNING";
    case E_USER_ERROR:
      return "E_USER_ERROR";
    case E_USER_WARNING:
      return "E_USER_WARNING";
    case E_USER_NOTICE:
      return "E_USER_NOTICE";
    default:
      return "Error";
  }
}

static int nr_php_should_record_error(int type, const char* format TSRMLS_DC) {
  int errprio;

  if (0 == (EG(error_reporting) & type)) {
    return 0;
  }

  /*
   * Note: The sense of this check is reversed compared to the error_reporting
   * setting.
   */
  if (0 != (NRINI(ignore_errors) & type)) {
    return 0;
  }

  /*
   * Exceptions should only be recorded through our exception handler so that we
   * don't get stack traces with parameters.  Since our exception handler
   * creates an error (to mimic not having an exception handler), this
   * conditional will help us prevent double capture once we start recording
   * more than one error per transaction.
   *
   * Note:  The format string comparison is a fragile check:  This "Uncaught"
   * string is not guaranteed in the PHP runtime.  If there was a better way to
   * detect uncaught exceptions we would do so.  To reduce the chance that this
   * early exit triggers erroneously, we check for the exception error type.
   *
   * In PHP 8.0+, this is no longer a format string so we now omit the %s in the
   * check.
   */
  if ((E_ERROR == type) && (0 == nr_strnicmp(format, NR_PSTR("Uncaught")))) {
    return 0;
  }

  errprio = nr_php_error_get_priority(type);

  if (0 == errprio) {
    return 0;
  }

  if (NR_SUCCESS != nr_txn_record_error_worthy(NRPRG(txn), errprio)) {
    return 0;
  }

  return 1;
}
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
void nr_php_error_cb(int type,
                     const char* error_filename,
                     uint error_lineno,
                     zend_string* message) {
#else
void nr_php_error_cb(int type,
                     const char* error_filename,
                     uint error_lineno,
                     const char* format,
                     va_list args) {
#endif /* PHP >= 8.0 */
  TSRMLS_FETCH();
  char* stack_json = NULL;
  const char* errclass = NULL;
  char* msg = NULL;

#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO
  if (nr_php_should_record_error(type, format TSRMLS_CC)) {
    va_list args2;
    int len;

    va_copy(args2, args);
    len = vasprintf(&msg, format, args2);
    msg[len] = '\0';

#else
  /*
   * In PHP 8.0+, the type is OR'ed with the new E_DONT_BAIL error value. None
   * of our existing routines can handle this addition so we remove it before
   * proceeding.
   */
  type = type & ~E_DONT_BAIL;
  if (nr_php_should_record_error(type, ZSTR_VAL(message))) {
    msg = nr_strdup(ZSTR_VAL(message));

#endif /* PHP < 8.0 */

    stack_json = nr_php_backtrace_to_json(0 TSRMLS_CC);
    errclass = get_error_type_string(type);

    nr_txn_record_error(NRPRG(txn), nr_php_error_get_priority(type), msg,
                        errclass, stack_json);

    nr_free(msg);
    nr_free(stack_json);
  }

  /*
   * Call through to the actual error handler.
   */
  if (0 != NR_PHP_PROCESS_GLOBALS(orig_error_cb)) {
#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO
    NR_PHP_PROCESS_GLOBALS(orig_error_cb)
    (type, error_filename, error_lineno, format, args);
#else
    NR_PHP_PROCESS_GLOBALS(orig_error_cb)
    (type, error_filename, error_lineno, message);
#endif /* PHP < 8.0 */
  }
}

nr_status_t nr_php_error_record_exception(nrtxn_t* txn,
                                          zval* exception,
                                          int priority,
                                          const char* prefix,
                                          zend_llist* filters TSRMLS_DC) {
  zend_class_entry* ce;
  char* error_message = NULL;
  char* file;
  char* klass = NULL;
  long line;
  char* message;
  char* stack_json;
  zval* stack_trace;

  if ((NULL == txn)
      || (0 == nr_php_error_zval_is_exception(exception TSRMLS_CC))) {
    return NR_FAILURE;
  }

  if (filters) {
    nr_php_exception_action_t action;

    action = nr_php_exception_filters_apply(filters, exception TSRMLS_CC);
    if (NR_PHP_EXCEPTION_FILTER_IGNORE == action) {
      return NR_SUCCESS;
    }
  }

  if (NULL == prefix) {
    prefix = "Exception ";
  }

  ce = Z_OBJCE_P(exception);
  file = nr_php_error_exception_file(exception TSRMLS_CC);
  klass = nr_strndup(ZEND_STRING_VALUE(ce->name), ZEND_STRING_LEN(ce->name));
  line = nr_php_error_exception_line(exception TSRMLS_CC);
  message = nr_php_error_exception_message(exception TSRMLS_CC);
  stack_trace = nr_php_error_exception_stack_trace(exception TSRMLS_CC);
  stack_json = nr_php_backtrace_to_json(stack_trace TSRMLS_CC);

  /*
   * We could do a single malloc and build the string up from its constituent
   * parts below, but that requires us to calculate the maximum possible length
   * of the error message. I'd prefer to just do one allocation and be done
   * with it via a tree of asprintfs, as ugly as it is.
   *
   * The formats below originally came from newrelic_notice_error, except that
   * the prefix there was hardcoded to "Exception ". (The prefix is settable so
   * that we can distinguish in APM between uncaught and noticed exceptions,
   * which we want to display differently.)
   */
  if (file && line) {
    if (message) {
      error_message = nr_formatf("%s'%s' with message '%s' in %s:%ld", prefix,
                                 klass, message, file, line);
    } else {
      error_message = nr_formatf("%s'%s' in %s:%ld", prefix, klass, file, line);
    }
  } else if (message) {
    error_message
        = nr_formatf("%s'%s' with message '%s'", prefix, klass, message);
  } else {
    error_message = nr_formatf("%s'%s'", prefix, klass);
  }

  nr_txn_record_error(NRPRG(txn), priority, error_message, klass, stack_json);

  nr_free(error_message);
  nr_free(file);
  nr_free(klass);
  nr_free(message);
  nr_free(stack_json);
  nr_php_zval_free(&stack_trace);

  return NR_SUCCESS;
}

nr_status_t nr_php_error_record_exception_segment(nrtxn_t* txn,
                                                  zval* exception,
                                                  zend_llist* filters
                                                      TSRMLS_DC) {
  char* klass = NULL;
  char* error_message = NULL;
  char* file = NULL;
  char* message = NULL;
  char* prefix = "Uncaught exception ";
  long line = 0;
  zend_class_entry* ce;
  zval* zend_err_message = NULL;
  zval* zend_err_file = NULL;
  zval* zend_err_line;

  if ((NULL == txn)
      || (0 == nr_php_error_zval_is_exception(exception TSRMLS_CC))) {
    return NR_FAILURE;
  }

  if (filters) {
    nr_php_exception_action_t action;

    action = nr_php_exception_filters_apply(filters, exception TSRMLS_CC);
    if (NR_PHP_EXCEPTION_FILTER_IGNORE == action) {
      return NR_SUCCESS;
    }
  }

  ce = Z_OBJCE_P(exception);
  klass = nr_strndup(ZEND_STRING_VALUE(ce->name), ZEND_STRING_LEN(ce->name));
  zend_err_file = nr_php_get_zval_object_property(exception, "file" TSRMLS_CC);
  file = nr_strndup(Z_STRVAL_P(zend_err_file), Z_STRLEN_P(zend_err_file));
  zend_err_message
      = nr_php_get_zval_object_property(exception, "message" TSRMLS_CC);
  message
      = nr_strndup(Z_STRVAL_P(zend_err_message), Z_STRLEN_P(zend_err_message));
  zend_err_line = nr_php_get_zval_object_property(exception, "line" TSRMLS_CC);
  line = (long)Z_LVAL_P(zend_err_line);

  if (file && line) {
    if (message) {
      error_message = nr_formatf("%s'%s' with message '%s' in %s:%ld", prefix,
                                 klass, message, file, line);
    } else {
      error_message = nr_formatf("%s'%s' in %s:%ld", prefix, klass, file, line);
    }
  } else if (message) {
    error_message
        = nr_formatf("%s'%s' with message '%s'", prefix, klass, message);
  } else {
    error_message = nr_formatf("%s'%s'", prefix, klass);
  }

  nr_segment_record_exception(nr_txn_get_current_segment(NRPRG(txn), NULL),
                              error_message, klass);

  nr_free(error_message);
  nr_free(klass);
  nr_free(message);

  return NR_SUCCESS;
}

int nr_php_error_zval_is_exception(zval* zv TSRMLS_DC) {
#ifdef PHP7
  return nr_php_object_instanceof_class(zv, "Throwable" TSRMLS_CC);
#else
  return nr_php_object_instanceof_class(zv, "Exception" TSRMLS_CC);
#endif /* PHP7 */
}
