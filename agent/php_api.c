/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>

#include "php_agent.h"
#include "php_api.h"
#include "php_error.h"
#include "php_hash.h"
#include "php_user_instrument.h"
#include "fw_drupal_common.h"
#include "nr_rum.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_metrics.h"
#include "util_number_converter.h"
#include "util_strings.h"

void nr_php_api_error(const char* format, ...) {
  const char* docref = NULL;
  const char* params = "";
  va_list args;
  /* Avoid using TSRMLS_DC so that we can use NRPRINTFMT */
  TSRMLS_FETCH();

  va_start(args, format);
  nrl_vlog(NRL_WARNING, NRL_API, format, args);
  va_end(args);

  /*
   * Note that if the user has set up a custom error handler and inside it
   * calls one of these API functions incorrectly, this could generate an
   * infinite loop. This is an acceptable risk, since this is possible even
   * without the agent.
   */
  va_start(args, format);
  php_verror(docref, params, E_WARNING, format, args TSRMLS_CC);
  va_end(args);
}

void nr_php_api_add_supportability_metric(const char* name TSRMLS_DC) {
  char buf[512];

  if (0 == name) {
    return;
  }
  if (0 == NRPRG(txn)) {
    return;
  }

  buf[0] = '\0';
  snprintf(buf, sizeof(buf), "Supportability/api/%s", name);

  nrm_force_add(NRTXN(unscoped_metrics), buf, 0);
}

/*
 * Purpose : (New Relic API) Pretend that there is an error at this exact spot.
 *           Useful for business logic errors. newrelic_notice_error($errstr)
 *              - newrelic_notice_error($exception)
 *              - newrelic_notice_error($errstr,$exception)
 *              - newrelic_notice_error($errno,$errstr,$fname,$line_nr,$ctx)
 */
#ifdef TAGS
void zif_newrelic_notice_error(void); /* ctags landing pad only */
void newrelic_notice_error(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_notice_error) {
  const char* errclass = "NoticedError";
  char* errormsgstr = NULL;
  nr_string_len_t errormsglen = 0;
  zend_long ignore1 = 0;
  char* ignore2 = 0;
  nr_string_len_t ignore3 = 0;
  zend_long ignore4 = 0;
  zval* exc = NULL;
  zval* ignore5 = NULL;
  int priority = 0;
  zval* ignore = NULL;

  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_FALSE;
  }

  nr_php_api_add_supportability_metric("notice_error" TSRMLS_CC);

  if (NRINI(prioritize_api_errors)) {
    priority = NR_PHP_ERROR_PRIORITY_API_PRIORITIZED;
  } else {
    priority = nr_php_error_get_priority(E_ERROR);
  }

  if (NR_SUCCESS != nr_txn_record_error_worthy(NRPRG(txn), priority)) {
    nrl_debug(NRL_API,
              "newrelic_notice_error: a higher severity error has already been "
              "noticed");
    RETURN_FALSE;
  }

  switch (ZEND_NUM_ARGS()) {
    case 1:
      /*
       * Look for an Exception object first: if we look for a string first in
       * the one argument case, the Exception will be coerced to a string and
       * we won't be able to handle it as an exception without post-processing
       * the string.
       */
      if (SUCCESS
          == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET,
                                      ZEND_NUM_ARGS() TSRMLS_CC, "o", &exc)) {
        break;
      }

      if (FAILURE
          == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET,
                                      ZEND_NUM_ARGS() TSRMLS_CC, "s",
                                      &errormsgstr, &errormsglen)) {
        nrl_debug(
            NRL_API,
            "newrelic_notice_error: invalid single argument: expected string");
        RETURN_NULL();
      }
      break;

    case 2:
      if (FAILURE
          == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET,
                                      ZEND_NUM_ARGS() TSRMLS_CC, "zo!", &ignore,
                                      &exc)) {
        nrl_debug(NRL_API,
                  "newrelic_notice_error: invalid two arguments: expected "
                  "exception as second argument");
        RETURN_NULL();
      }
      break;

    case 5:
      if (FAILURE
          == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET,
                                      ZEND_NUM_ARGS() TSRMLS_CC, "lsslz!",
                                      &ignore1, &errormsgstr, &errormsglen,
                                      &ignore2, &ignore3, &ignore4, &ignore5)) {
        nrl_debug(NRL_API, "newrelic_notice_error: invalid five arguments");
        RETURN_NULL();
      }
      break;

    default:
      nrl_debug(NRL_API, "newrelic_notice_error: invalid number of arguments");
      RETURN_NULL();
  }

  if (exc) {
    if (NR_SUCCESS
        == nr_php_error_record_exception(
            NRPRG(txn), exc, priority, "Noticed exception ", NULL TSRMLS_CC)) {
      RETURN_TRUE;
    } else {
      nrl_debug(NRL_API, "newrelic_notice_error: invalid exception argument");
      RETURN_NULL();
    }
  }

  {
    char* buf = nr_strndup(errormsgstr, errormsglen);
    char* stack_json = nr_php_backtrace_to_json(NULL TSRMLS_CC);

    nr_txn_record_error(NRPRG(txn), priority, buf, errclass, stack_json);

    nr_free(buf);
    nr_free(stack_json);

    RETURN_TRUE;
  }
}

/*
 * Purpose : (New Relic API) Completely ignore this current transaction. Useful
 *           for keeping pinger/uptime urls from polluting the average response
 *           time.
 *              - newrelic_ignore_transaction()
 */
#ifdef TAGS
void zif_newrelic_ignore_transaction(void); /* ctags landing pad only */
void newrelic_ignore_transaction(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_ignore_transaction) {
  NR_UNUSED_EXECUTE_DATA;
  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (!nr_php_recording(TSRMLS_C)) {
    return;
  }

  /* No reason to make a supportability metric here! */

  nr_txn_ignore(NRPRG(txn));
}

/*
 * Purpose : (New Relic API) Don't generate Apdex metrics for the current
 *           transaction.
 *              - newrelic_ignore_apdex()
 */
#ifdef TAGS
void zif_newrelic_ignore_apdex(void); /* ctags landing pad only */
void newrelic_ignore_apdex(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_ignore_apdex) {
  NR_UNUSED_EXECUTE_DATA;
  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (!nr_php_recording(TSRMLS_C)) {
    return;
  }

  nr_php_api_add_supportability_metric("ignore_apdex" TSRMLS_CC);

  NRTXN(status.ignore_apdex) = 1;
  nrl_debug(NRL_API, "not generating Apdex metrics for this transaction");
}

/*
 * Purpose : (New Relic API) Consider this point to be the end of
 *           this transaction. Useful when the page starts streaming video or
 *           something: the streaming shouldn't count as "slow".
 *              - newrelic_end_of_transaction()
 */
#ifdef TAGS
void zif_newrelic_end_of_transaction(void); /* ctags landing pad only */
void newrelic_end_of_transaction(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_end_of_transaction) {
  NR_UNUSED_EXECUTE_DATA;
  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (!nr_php_recording(TSRMLS_C)) {
    return;
  }

  nr_php_api_add_supportability_metric("end_of_transaction" TSRMLS_CC);

  (void)nr_txn_end(NRPRG(txn));
  nrl_debug(NRL_API, "transaction ended prematurely");
}

/*
 * Purpose : (New Relic API) End the current transaction, sending its data off
 *           to the daemon. This differs from the function above considerably,
 *           which simply marks the end time of a transaction. This call
 *           actually properly ends the transaction and ships the data off,
 *           under the assumption that the user code will be starting a new
 *           transaction.
 *              - newrelic_end_transaction()
 *              - newrelic_end_transaction(ignore)
 */
#ifdef TAGS
void zif_newrelic_end_transaction(void); /* ctags landing pad only */
void newrelic_end_transaction(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_end_transaction) {
  nr_status_t ret;
  zend_bool ignorebool = 0;
  zend_long ignore = 0;

  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (0 == NRPRG(txn)) {
    RETURN_FALSE;
  }

  nr_php_api_add_supportability_metric("end_transaction" TSRMLS_CC);

  if (1 == ZEND_NUM_ARGS()) {
    if (SUCCESS
        == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &ignorebool)) {
      ignore = (long)ignorebool;
    } else if (FAILURE
               == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l",
                                        &ignore)) {
      RETURN_FALSE;
    }
  }

  ret = nr_php_txn_end((0 != ignore), 0 TSRMLS_CC);
  if (NR_SUCCESS == ret) {
    nrl_debug(NRL_API, "transaction completed by API");
    RETURN_TRUE;
  } else {
    /* IMPOSSIBLE path through interpreter */
    /*
     * There is no failure path through nr_php_txn_end, and if there were,
     * it would only happen if there weren't a transaction,
     * but we've already checked that, above.
     */
    nrl_debug(NRL_API, "transaction end API failed");
    RETURN_FALSE;
  }
}

#ifdef TAGS
void zif_newrelic_start_transaction(void); /* ctags landing pad only */
void newrelic_start_transaction(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_start_transaction) {
  nr_status_t ret;
  char* appnames = NULL;
  char* license = NULL;
  char* appstr = NULL;
  char* licstr = NULL;
  nr_string_len_t applen = 0;
  nr_string_len_t liclen = 0;

  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (0 != NRPRG(txn)) {
    RETURN_FALSE;
  }

  if (1 == ZEND_NUM_ARGS()) {
    if (SUCCESS
        == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &appstr,
                                 &applen)) {
      appnames = (char*)nr_alloca(applen + 1);
      nr_strxcpy(appnames, appstr, applen);
    } else {
      RETURN_FALSE;
    }
  } else if (2 == ZEND_NUM_ARGS()) {
    if (SUCCESS
        == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &appstr,
                                 &applen, &licstr, &liclen)) {
      appnames = (char*)nr_alloca(applen + 1);
      nr_strxcpy(appnames, appstr, applen);
      license = (char*)nr_alloca(liclen + 1);
      nr_strxcpy(license, licstr, liclen);
    } else {
      RETURN_FALSE;
    }
  } else {
    RETURN_FALSE;
  }

  ret = nr_php_txn_begin(appnames, license TSRMLS_CC);
  if (NR_SUCCESS == ret) {
    nr_php_api_add_supportability_metric("start_transaction" TSRMLS_CC);
    nrl_debug(NRL_API, "transaction started by API");
    RETURN_TRUE;
  } else {
    nrl_debug(NRL_API, "transaction start API failed");
    RETURN_FALSE;
  }
}

/*
 * Purpose : (New Relic API) Mark the current transaction as a background job.
 *              - newrelic_background_job([background])
 */
#ifdef TAGS
void zif_newrelic_background_job(void); /* ctags landing pad only */
void newrelic_background_job(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_background_job) {
  zend_long background = 0;
  zend_bool backgroundbool = 0;

  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (!nr_php_recording(TSRMLS_C)) {
    return;
  }

  nr_php_api_add_supportability_metric("background_job" TSRMLS_CC);

  if (ZEND_NUM_ARGS() >= 1) {
    if (FAILURE
        == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b",
                                 &backgroundbool)) {
      if (FAILURE
          == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l",
                                   &background)) {
        background = 1;
      } else {
        /* background has the value we want */
      }
    } else {
      background = backgroundbool;
    }
  } else {
    background = 1;
  }

  if (background) {
    nr_txn_set_as_background_job(NRPRG(txn),
                                 "newrelic_background_job API call");
  } else {
    nr_txn_set_as_web_transaction(NRPRG(txn),
                                  "newrelic_background_job API call");
  }
}

static void nr_php_api_capture_params_internal(const char* function_name,
                                               INTERNAL_FUNCTION_PARAMETERS) {
  zend_long enable = 0;
  zend_bool enablebool = 0;

  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (!nr_php_recording(TSRMLS_C)) {
    return;
  }

  nr_php_api_add_supportability_metric(function_name TSRMLS_CC);

  if (ZEND_NUM_ARGS() >= 1) {
    if (FAILURE
        == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &enablebool)) {
      if (FAILURE
          == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &enable)) {
        enable = 1;
      } else {
        /* enable has the value we want */
      }
    } else {
      enable = enablebool;
    }
  } else {
    enable = 1;
  }

  NRPRG(deprecated_capture_request_parameters) = enable ? 1 : 0;

  nrl_debug(NRL_API, "capture params enabled='%.10s'",
            NRPRG(deprecated_capture_request_parameters) ? "true" : "false");
}

/*
 * Purpose : (New Relic API) Turn the capture params on or off
 *              - newrelic_enable_params([enable])
 *
 *           Deprecated in favor of newrelic_capture_params.
 */
#ifdef TAGS
void zif_newrelic_enable_params(void); /* ctags landing pad only */
void newrelic_enable_params(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_enable_params) {
  nr_php_api_capture_params_internal("enable_params",
                                     INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

/*
 * Purpose : (New Relic API) Turn the capture params on or off
 *              - newrelic_capture_params([enable])
 */
#ifdef TAGS
void zif_newrelic_capture_params(void); /* ctags landing pad only */
void newrelic_capture_params(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_capture_params) {
  nr_php_api_capture_params_internal("capture_params",
                                     INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

/*
 * Purpose : (New Relic API) Add this custom metric
 *              - newrelic_custom_metric(metric, value)
 */
#ifdef TAGS
void zif_newrelic_custom_metric(void); /* ctags landing pad only */
void newrelic_custom_metric(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_custom_metric) {
  char* metricstr = NULL;
  nr_string_len_t metriclen = 0;
  double value_ms = 0;
  char* key = NULL;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_TRUE;
  }

  nr_php_api_add_supportability_metric("custom_metric" TSRMLS_CC);

  if (ZEND_NUM_ARGS() < 2) {
    RETURN_FALSE;
  }

  if (FAILURE
      == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sd", &metricstr,
                               &metriclen, &value_ms)) {
    RETURN_FALSE;
  }
  key = (char*)nr_alloca(metriclen + 1);

  nr_strxcpy(key, metricstr, metriclen);

  if (NR_SUCCESS == nr_txn_add_custom_metric(NRPRG(txn), key, value_ms)) {
    RETURN_TRUE;
  } else {
    RETURN_FALSE;
  }
}

#define NR_PHP_API_INVALID_ATTRIBUTE_FMT \
  "%s: expects parameter to be scalar, %s given"

static nrobj_t* nr_php_api_zval_to_attribute_obj(const zval* z TSRMLS_DC) {
  char* str = NULL;
  nrobj_t* obj = NULL;

  if (NULL == z) {
    return NULL;
  }

  nr_php_zval_unwrap(z);

  switch (Z_TYPE_P(z)) {
    case IS_NULL:
      return nro_new_none();

    case IS_LONG:
      return nro_new_long(Z_LVAL_P(z));

    case IS_DOUBLE:
      return nro_new_double(Z_DVAL_P(z));

#ifdef PHP7
    case IS_TRUE:
      return nro_new_boolean(1);

    case IS_FALSE:
      return nro_new_boolean(0);
#else
    case IS_BOOL:
      return nro_new_boolean(Z_BVAL_P(z));
#endif /* PHP7 */

    case IS_STRING:
      if (!nr_php_is_zval_valid_string(z)) {
        nr_php_api_error(NR_PHP_API_INVALID_ATTRIBUTE_FMT,
                         get_active_function_name(TSRMLS_C), "invalid string");
        return NULL;
      }

      /* Copy the string to ensure that it is NUL-terminated */
      str = nr_strndup(Z_STRVAL_P(z), Z_STRLEN_P(z));
      obj = nro_new_string(str);
      nr_free(str);
      return obj;

    case IS_ARRAY:
      nr_php_api_error(NR_PHP_API_INVALID_ATTRIBUTE_FMT,
                       get_active_function_name(TSRMLS_C), "array");
      return NULL;

    case IS_OBJECT:
      nr_php_api_error(NR_PHP_API_INVALID_ATTRIBUTE_FMT,
                       get_active_function_name(TSRMLS_C), "object");
      return NULL;

    case IS_RESOURCE:
      nr_php_api_error(NR_PHP_API_INVALID_ATTRIBUTE_FMT,
                       get_active_function_name(TSRMLS_C), "resource");
      return NULL;

#if ZEND_MODULE_API_NO < ZEND_7_3_X_API_NO
    case IS_CONSTANT:
      nr_php_api_error(NR_PHP_API_INVALID_ATTRIBUTE_FMT,
                       get_active_function_name(TSRMLS_C), "constant");
      return NULL;
#endif /* PHP < 7.3 */

#if ZEND_MODULE_API_NO >= ZEND_5_6_X_API_NO
    case IS_CONSTANT_AST:
      nr_php_api_error(NR_PHP_API_INVALID_ATTRIBUTE_FMT,
                       get_active_function_name(TSRMLS_C), "constant AST");
      return NULL;
#else
    case IS_CONSTANT_ARRAY:
      nr_php_api_error(NR_PHP_API_INVALID_ATTRIBUTE_FMT,
                       get_active_function_name(TSRMLS_C), "constant array");
      return NULL;
#endif /* PHP >= 5.6 */

    default:
      nr_php_api_error(NR_PHP_API_INVALID_ATTRIBUTE_FMT,
                       get_active_function_name(TSRMLS_C), "unknown");
      return NULL;
  }
}

/*
 * Purpose : (New Relic API) Add this custom parameter to the current
 *           transaction
 *              - newrelic_add_custom_parameter(key, value)
 */
#ifdef TAGS
void zif_newrelic_add_custom_parameter(void); /* ctags landing pad only */
void newrelic_add_custom_parameter(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_add_custom_parameter) {
  zval* zzkey = NULL;
  zval* zzvalue = NULL;
  char* key = NULL;
  char tmp[64];
  nr_status_t rv = NR_SUCCESS;
  nrobj_t* obj = NULL;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_TRUE;
  }

  nr_php_api_add_supportability_metric("add_custom_parameter" TSRMLS_CC);

  if (ZEND_NUM_ARGS() < 2) {
    RETURN_FALSE;
  }

  if (FAILURE
      == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &zzkey,
                               &zzvalue)) {
    RETURN_FALSE;
  }

  nr_php_zval_unwrap(zzkey);

  switch (Z_TYPE_P(zzkey)) {
    case IS_NULL:
      key = nr_strdup("(null)");
      break;

    case IS_LONG:
      snprintf(tmp, sizeof(tmp), "%ld", (long)Z_LVAL_P(zzkey));
      key = nr_strdup(tmp);
      break;

    case IS_DOUBLE:
      nr_double_to_str(tmp, sizeof(tmp), Z_DVAL_P(zzkey));
      key = nr_strdup(tmp);
      break;

#ifdef PHP7
    case IS_TRUE:
      key = nr_strdup("True");
      break;

    case IS_FALSE:
      key = nr_strdup("False");
      break;
#else
    case IS_BOOL:
      key = nr_strdup(Z_BVAL_P(zzkey) ? "True" : "False");
      break;
#endif /* PHP7 */

    case IS_ARRAY:
      key = nr_strdup("(Array)");
      break;

    case IS_OBJECT:
      key = nr_strdup("(Object)");
      break;

    case IS_STRING:
      if (!nr_php_is_zval_valid_string(zzkey)) {
        key = nr_strdup("(Invalid String)");
      } else {
        key = (char*)nr_malloc(Z_STRLEN_P(zzkey) + 1);
        nr_strxcpy(key, Z_STRVAL_P(zzkey), Z_STRLEN_P(zzkey));
      }

      break;

    case IS_RESOURCE:
      key = nr_strdup("(Resource)");
      break;

#if ZEND_MODULE_API_NO < ZEND_7_3_X_API_NO
    case IS_CONSTANT:
      key = nr_strdup("(Constant)"); /* NOTTESTED */
      break;
#endif /* PHP < 7.3 */

#if ZEND_MODULE_API_NO >= ZEND_5_6_X_API_NO
    case IS_CONSTANT_AST:
      key = nr_strdup("(Constant AST)"); /* NOTTESTED */
      break;
#else
    case IS_CONSTANT_ARRAY:
      key = nr_strdup("(Constant array)"); /* NOTTESTED */
      break;
#endif /* PHP >= 5.6 */

    default:
      key = nr_strdup("(?)"); /* NOTTESTED */
      break;
  }

  obj = nr_php_api_zval_to_attribute_obj(zzvalue TSRMLS_CC);

  if (obj) {
    rv = nr_txn_add_user_custom_parameter(NRPRG(txn), key, obj);
  }

  nro_delete(obj);
  nr_free(key);

  if (NR_SUCCESS == rv) {
    RETURN_TRUE;
  } else {
    RETURN_FALSE;
  }
}

/*
 * Purpose : (New Relic API) Specify the name of the current transaction
 *              - newrelic_name_transaction(string)
 */
#ifdef TAGS
void zif_newrelic_name_transaction(void); /* ctags landing pad only */
void newrelic_name_transaction(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_name_transaction) {
  int zend_rv = 0;
  nr_status_t rv;
  char* namestr = NULL;
  nr_string_len_t namestrlen = 0;
  char* s = NULL;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_TRUE;
  }

  nr_php_api_add_supportability_metric("name_transaction" TSRMLS_CC);

  if (1 != ZEND_NUM_ARGS()) {
    nrl_warning(
        NRL_API,
        "newrelic_name_transaction failure: improper number of parameters");
    RETURN_FALSE;
  }

  zend_rv = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &namestr,
                                  &namestrlen);
  if ((SUCCESS != zend_rv) || (0 == namestr) || (namestrlen <= 0)) {
    nrl_warning(
        NRL_API,
        "newrelic_name_transaction failure: unable to parse string parameter");
    RETURN_FALSE;
  }

  s = (char*)nr_alloca(namestrlen + 1);
  nr_strxcpy(s, namestr, namestrlen);

  rv = nr_txn_set_path("API", NRPRG(txn), s, NR_PATH_TYPE_CUSTOM,
                       NR_OK_TO_OVERWRITE);
  if (NR_SUCCESS != rv) {
    nrl_warning(
        NRL_API,
        "newrelic_name_transaction failure: unable to change name to " NRP_FMT,
        NRP_TXNNAME(s));
  } else {
    nrl_debug(NRL_API, "newrelic_name_transaction: API naming is " NRP_FMT,
              NRP_TXNNAME(s));
  }

  RETURN_TRUE;
}

/*
 * Purpose : (New Relic API) Add this function to the transaction tracer
 *              - newrelic_add_custom_tracer(function_name)
 *              - newrelic_add_custom_tracer(classname::function_name)
 */
#ifdef TAGS
void zif_newrelic_add_custom_tracer(void); /* ctags landing pad only */
void newrelic_add_custom_tracer(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_add_custom_tracer) {
  char* namestr = NULL;
  nr_string_len_t namestrlen = 0;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_TRUE;
  }

  nr_php_api_add_supportability_metric("add_custom_tracer" TSRMLS_CC);

  if (1 == ZEND_NUM_ARGS()) {
    if (FAILURE
        == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &namestr,
                                 &namestrlen)) {
      RETURN_FALSE;
    }
  } else {
    RETURN_FALSE;
  }

  nr_php_add_custom_tracer(namestr, namestrlen TSRMLS_CC);

  RETURN_TRUE;
}

/*
 * Purpose : (New Relic API) Support Real User Monitoring(tm)
 *              - newrelic_get_browser_timing_header(bool)
 *              - newrelic_get_browser_timing_footer(bool)
 *
 *           Optional boolean (defaults to true) tells us whether or not to
 *           return the enclosing script tags.
 */
#ifdef TAGS
void zif_newrelic_get_browser_timing_header(void); /* ctags landing pad only */
void newrelic_get_browser_timing_header(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_get_browser_timing_header) {
  char* timingScript = NULL;
  zend_long usetags = 1;
  zend_bool tagsbool = 0;

  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_EMPTY_STRING();
  }

  nr_php_api_add_supportability_metric("get_browser_timing_header" TSRMLS_CC);

  if (ZEND_NUM_ARGS() >= 1) {
    if (FAILURE
        == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &tagsbool)) {
      if (FAILURE
          == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &usetags)) {
        usetags = 1;
      } else {
        /* usetags has the value we want */
      }
    } else {
      usetags = tagsbool;
    }
  }

  timingScript = nr_rum_produce_header(NRPRG(txn), usetags == 1, 0);
  if (0 == timingScript) {
    RETURN_EMPTY_STRING();
  }

  /*
   * Required to silence warnings about PHP's prototypes.
   */
#ifdef PHP7
  RETVAL_STRING(timingScript);
#else
#if defined(__clang__) || (__GNUC__ > 4) \
    || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 5))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
  RETVAL_STRING(timingScript, 1);
#pragma GCC diagnostic pop
#else
  RETVAL_STRING(timingScript, 1);
#endif
#endif /* PHP7 */

  nr_free(timingScript);
}

#ifdef TAGS
void zif_newrelic_get_browser_timing_footer(void); /* ctags landing pad only */
void newrelic_get_browser_timing_footer(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_get_browser_timing_footer) {
  zend_long usetags = 1;
  zend_bool tagsbool = 0;
  char* buf = NULL;

  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_EMPTY_STRING();
  }

  nr_php_api_add_supportability_metric("get_browser_timing_footer" TSRMLS_CC);

  if (ZEND_NUM_ARGS() >= 1) {
    if (FAILURE
        == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &tagsbool)) {
      if (FAILURE
          == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &usetags)) {
        usetags = 1;
      } else {
        /* usetags has the value we want */
      }
    } else {
      usetags = tagsbool;
    }
  }

  buf = nr_rum_produce_footer(NRPRG(txn), usetags == 1, 0);
  if (0 == buf) {
    RETURN_EMPTY_STRING();
  }

  /*
   * Required to silence warnings about PHP's prototypes.
   */
#ifdef PHP7
  RETVAL_STRING(buf);
#else
#if defined(__clang__) || (__GNUC__ > 4) \
    || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 5))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
  RETVAL_STRING(buf, 1);
#pragma GCC diagnostic pop
#else
  RETVAL_STRING(buf, 1);
#endif
#endif /* PHP7 */

  nr_free(buf);
}

/*
 * Purpose : (New Relic API) If auto-RUM not already sent, disable it.
 *              - newrelic_disable_autorum()
 */
#ifdef TAGS
void zif_newrelic_disable_autorum(void); /* ctags landing pad only */
void newrelic_disable_autorum(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_disable_autorum) {
  NR_UNUSED_EXECUTE_DATA;
  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (!nr_php_recording(TSRMLS_C)) {
    return;
  }

  nr_php_api_add_supportability_metric("disable_autorum" TSRMLS_CC);

  NRTXN(options.autorum_enabled) = 0;
  RETURN_TRUE;
}

/* Set up a bitmask to track the state of a call to newrelic_set_appname(). */
typedef uint8_t nr_php_set_appname_state_t;
const nr_php_set_appname_state_t NR_PHP_APPNAME_LICENSE_CHANGED = (1 << 0);
const nr_php_set_appname_state_t NR_PHP_APPNAME_LICENSE_PROVIDED = (1 << 1);
const nr_php_set_appname_state_t NR_PHP_APPNAME_LASP_ENABLED = (1 << 2);

/*
 * LASP will prevent an application switch using the newrelic_set_appname() API
 * below if LASP is enabled (by setting newrelic.security_policies_token to a
 * non-empty string) _and_ a different licence key has been provided. This
 * constant encodes what that state looks like in an nr_php_set_appname_state_t
 * bitmask, as defined above.
 */
#define NR_PHP_APPNAME_LASP_DENIED \
  (NR_PHP_APPNAME_LASP_ENABLED | NR_PHP_APPNAME_LICENSE_CHANGED)

/*
 * Purpose : (New Relic API) Switch to a different application mid-flight. Will
 *           not work if the RUM footer has already been sent.
 *              - newrelic_set_appname(name)
 *              - newrelic_set_appname(name, license)
 *              - newrelic_set_appname(name, license, xmit)
 */
#ifdef TAGS
void zif_newrelic_set_appname(void); /* ctags landing pad only */
void newrelic_set_appname(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_set_appname) {
  nr_status_t ret;
  char* appnames = NULL;
  char* current_license = NULL;
  char* license = NULL;
  char* appstr = NULL;
  char* licstr = NULL;
  nr_string_len_t applen = 0;
  nr_string_len_t liclen = 0;
  nr_php_set_appname_state_t state = 0;
  zend_bool xmitbool = 0;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  nr_php_api_add_supportability_metric("set_appname/before" TSRMLS_CC);

  /*
   * If there is an active transaction, get a copy of the license.
   * This will help to determine whether this call is being used
   * to switch from one license to another.
   */
  if (0 != NRPRG(txn)) {
    /*
     * If the current license is at least a valid length, make a copy
     * for the scope of this function. If it's longer, it will be
     * truncated, and New Relic will throw "invalid license key" on
     * the connection attempt.
     */
    if (NR_LICENSE_SIZE == nr_strnlen(NRTXN(license), NR_LICENSE_SIZE)) {
      current_license = (char*)nr_alloca(NR_LICENSE_SIZE + 1);
      nr_strxcpy(current_license, NRTXN(license), NR_LICENSE_SIZE + 1);
    }
  }

  if (SUCCESS
      == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|sb", &appstr,
                               &applen, &licstr, &liclen, &xmitbool)) {
    appnames = (char*)nr_alloca(applen + 1);
    nr_strxcpy(appnames, appstr, applen);

    if (0 != licstr) {
      license = (char*)nr_alloca(liclen + 1);
      nr_strxcpy(license, licstr, liclen);
    }
  } else {
    RETURN_FALSE;
  }

  /*
   * Figure out if we're about to change licenses. We need this both for the
   * supportability metrics we'll create within the new application and to
   * prevent a license change if the current application has LASP enabled.
   */
  if (!nr_strempty(license)) {
    state |= NR_PHP_APPNAME_LICENSE_PROVIDED;
    if (0 != current_license
        && 0 != nr_strncmp(current_license, license, NR_LICENSE_SIZE)) {
      state |= NR_PHP_APPNAME_LICENSE_CHANGED;
    }
  }

  /*
   * Determine if LASP is enabled. Since there may or may not be a transaction
   * active, we can't rely on the transaction options as the source of truth
   * here, so we'll go to the raw INI setting instead.
   *
   * If the user has set newrelic.security_policies_token to a non-empty string,
   * then we know that LASP is enabled, and don't care about the details of
   * what's actually set or not.
   */
  if (!nr_strempty(NRINI(security_policies_token))) {
    state |= NR_PHP_APPNAME_LASP_ENABLED;
  }

  /*
   * If LASP is going to deny the new application, we'll add a supportability
   * metric for Angler to pick up, although in practice most users don't
   * transmit the previous transaction.
   *
   * Note that we don't want to return from here, since the previous transaction
   * hasn't yet ended.
   */
  if (NR_PHP_APPNAME_LASP_DENIED == (state & NR_PHP_APPNAME_LASP_DENIED)) {
    nr_php_api_add_supportability_metric("set_appname/lasp_denied" TSRMLS_CC);
  }

  ret = nr_php_txn_end((0 == xmitbool), 0 TSRMLS_CC);
  if (NR_SUCCESS != ret) {
    nrl_verbose(NRL_API,
                "newrelic_set_appname: failed to end current transaction in "
                "changing app to " NRP_FMT " [" NRP_FMT "]",
                NRP_APPNAME(appnames), NRP_LICNAME(license));
  }

  /*
   * OK, now the transaction has ended, we should return if LASP is denying the
   * new transaction.
   */
  if (NR_PHP_APPNAME_LASP_DENIED == (state & NR_PHP_APPNAME_LASP_DENIED)) {
    nr_php_api_error(
        "newrelic_set_appname: when a security_policies_token is present in "
        "the newrelic.ini file, it is not permitted to call "
        "newrelic_set_appname() with a non-empty license key. LASP does not "
        "permit changing accounts during runtime. Consider using \"\" for the "
        "second parameter");
    RETURN_FALSE;
  }

  ret = nr_php_txn_begin(appnames, license TSRMLS_CC);
  if (NR_SUCCESS != ret) {
    nrl_verbose(NRL_API,
                "newrelic_set_appname: unable to start new transaction with "
                "app " NRP_FMT " [" NRP_FMT "]",
                NRP_APPNAME(appnames), NRP_LICNAME(license));
    RETURN_FALSE;
  }

  /*
   * If this function was called with a non-empty license, send up a
   * supportability metric. Moreover, if there's a current license that we are
   * about to switch away from, send up a supportability metric.
   */
  if (state & NR_PHP_APPNAME_LICENSE_PROVIDED) {
    nr_php_api_add_supportability_metric("set_appname/with_license" TSRMLS_CC);
    if (state & NR_PHP_APPNAME_LICENSE_CHANGED) {
      nrl_debug(NRL_API,
                "newrelic_set_appname: application changed away from " NRP_FMT,
                NRP_LICNAME(current_license));
      nr_php_api_add_supportability_metric(
          "set_appname/switched_license" TSRMLS_CC);
    }
  }

  nr_php_api_add_supportability_metric("set_appname/after" TSRMLS_CC);
  nrl_debug(NRL_API,
            "newrelic_set_appname: application changed to " NRP_FMT " [" NRP_FMT
            "]",
            NRP_APPNAME(appnames), NRP_LICNAME(license));

  RETURN_TRUE;
}

static nr_status_t nr_php_api_add_custom_parameter_string(nrtxn_t* txn,
                                                          const char* key,
                                                          const char* val,
                                                          int val_len) {
  nr_status_t rv;
  nrobj_t* obj;
  char* val_nul_terminated = 0;

  if (0 == val) {
    return NR_SUCCESS;
  }
  if (val_len <= 0) {
    return NR_SUCCESS;
  }
  val_nul_terminated = nr_strndup(val, val_len);
  obj = nro_new_string(val_nul_terminated);
  rv = nr_txn_add_user_custom_parameter(txn, key, obj);
  nro_delete(obj);
  nr_free(val_nul_terminated);

  return rv;
}

/*
 * Purpose : (New Relic API) Sets user attributes
 *              - newrelic_set_user_attributes(user, account, product)
 */
#ifdef TAGS
void zif_newrelic_set_user_attributes(void); /* ctags landing pad only */
void newrelic_set_user_attributes(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_set_user_attributes) {
  char* userstr = NULL;
  char* accstr = NULL;
  char* prodstr = NULL;
  nr_string_len_t userlen = 0;
  nr_string_len_t acclen = 0;
  nr_string_len_t prodlen = 0;
  nr_status_t rv;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_TRUE;
  }

  nr_php_api_add_supportability_metric("set_user_attributes" TSRMLS_CC);

  if (3 != ZEND_NUM_ARGS()) {
    RETURN_FALSE;
  }

  if (FAILURE
      == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss", &userstr,
                               &userlen, &accstr, &acclen, &prodstr,
                               &prodlen)) {
    RETURN_FALSE;
  }

  rv = nr_php_api_add_custom_parameter_string(NRPRG(txn), "user", userstr,
                                              userlen);
  if (NR_FAILURE == rv) {
    RETURN_FALSE;
  }
  rv = nr_php_api_add_custom_parameter_string(NRPRG(txn), "account", accstr,
                                              acclen);
  if (NR_FAILURE == rv) {
    RETURN_FALSE;
  }
  rv = nr_php_api_add_custom_parameter_string(NRPRG(txn), "product", prodstr,
                                              prodlen);
  if (NR_FAILURE == rv) {
    RETURN_FALSE;
  }

  RETURN_TRUE;
}

static nr_status_t nr_php_api_add_custom_span_attribute(const char* keystr,
                                                        nr_string_len_t keylen,
                                                        nrobj_t* value
                                                            TSRMLS_DC) {
  bool rv;
  char* key = NULL;
  nr_segment_t* current;

  current = nr_txn_get_current_segment(NRPRG(txn), NULL);
  if (!current) {
    return NR_FAILURE;
  }

  key = nr_strndup(keystr, keylen);
  rv = nr_segment_attributes_user_add(current, NR_ATTRIBUTE_DESTINATION_SPAN,
                                      key, value);
  nr_free(key);
  if (!rv) {
    return NR_FAILURE;
  }

  return NR_SUCCESS;
}

/*
 * Purpose : (New Relic API) Adds a custom span parameter
 *              - newrelic_add_custom_span_parameter(key, value)
 */
#ifdef TAGS
void zif_newrelic_add_custom_span_parameter(void); /* ctags landing pad only */
void newrelic_add_custom_span_parameter(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_add_custom_span_parameter) {
  char* key = NULL;
  nr_string_len_t keylen = 0;
  zval* zvalue = NULL;
  nr_status_t rv;
  nrobj_t* value = NULL;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_TRUE;
  }

  nr_php_api_add_supportability_metric("add_custom_span_parameter" TSRMLS_CC);

  if (2 != ZEND_NUM_ARGS()) {
    RETURN_FALSE;
  }

  if (FAILURE
      == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &key, &keylen,
                               &zvalue)) {
    RETURN_FALSE;
  }

  value = nr_php_api_zval_to_attribute_obj(zvalue TSRMLS_CC);

  rv = nr_php_api_add_custom_span_attribute(key, keylen, value TSRMLS_CC);

  nro_delete(value);

  if (NR_FAILURE == rv) {
    RETURN_FALSE;
  }

  RETURN_TRUE;
}

/*
 * Purpose : Transform the zval array of attributes into nrobj_t format
 *           expected by axiom.
 */
static nrobj_t* nr_php_api_transform_custom_events_attributes(
    zval* params_zval TSRMLS_DC) {
  nrobj_t* obj = NULL;
  zval* element = NULL;
  nr_php_string_hash_key_t* string_key;
  zend_ulong num_key = 0;

  /*
   * GCC correctly detects num_key as being unused, since we only ever assign
   * to it; we ignore any assigned value in the loop below because we're only
   * interested in string keys in this function.
   *
   * In the future when PHP 5 compatibility is removed, we can replace the
   * loop below with ZEND_HASH_FOREACH_STR_KEY_VAL, but rather than
   * implementing every variation in php_compat.h for now, let's just quiet the
   * warning.
   */
  (void)num_key;

  obj = nro_new_hash();

  ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(params_zval), num_key, string_key,
                            element) {
    nrobj_t* o;
    char* terminated_key = NULL;

    if (NULL == element) {
      continue;
    }

    if (string_key) {
      terminated_key = nr_strndup(ZEND_STRING_VALUE(string_key),
                                  ZEND_STRING_LEN(string_key));
    } else {
      nr_php_api_error(
          "newrelic_record_custom_event: ignoring non-string array key");
      continue;
    }

    o = nr_php_api_zval_to_attribute_obj(element TSRMLS_CC);
    nro_set_hash(obj, terminated_key, o);
    nro_delete(o);
    nr_free(terminated_key);
  }
  ZEND_HASH_FOREACH_END();

  return obj;
}

#ifdef TAGS
void zif_newrelic_record_custom_event(void); /* ctags landing pad only */
void newrelic_record_custom_event(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_record_custom_event) {
  char* event_type = NULL;
  nr_string_len_t event_type_len = 0;
  char* event_type_terminated = NULL;
  zval* params_zval = NULL;
  nrobj_t* obj = NULL;

  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (0 == nr_php_recording(TSRMLS_C)) {
    return;
  }

  if (0 == NRPRG(txn)->options.custom_events_enabled) {
    return;
  }

  nr_php_api_add_supportability_metric("record_custom_event" TSRMLS_CC);

  if (FAILURE
      == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa", &event_type,
                               &event_type_len, &params_zval)) {
    nrl_warning(NRL_API,
                "unable to parse parameters to newrelic_record_custom_event. "
                "%d parameters received",
                ZEND_NUM_ARGS());
    return;
  }

  if ((NULL == event_type) || (event_type_len <= 0)) {
    nr_php_api_error(
        "improper parameter to newrelic_record_custom_event: event_type must "
        "be a nonempty string");
    return;
  }

  if (0 == nr_php_is_zval_valid_array(params_zval)) {
    nr_php_api_error(
        "improper parameter to newrelic_record_custom_event: parameters must "
        "be an array");
    return;
  }

  event_type_terminated = nr_strndup(event_type, event_type_len);
  obj = nr_php_api_transform_custom_events_attributes(params_zval TSRMLS_CC);

  nr_txn_record_custom_event(NRPRG(txn), event_type_terminated, obj);

  nro_delete(obj);
  nr_free(event_type_terminated);
}

/*
 * Purpose : (New Relic API) Returns a boolean indicating if current txn is
 *           marked sampled
 *              - newrelic_is_sampled()
 */
#ifdef TAGS
void zif_newrelic_is_sampled(void); /* ctags landing pad only */
void newrelic_is_sampled(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_is_sampled) {
  NR_UNUSED_HT;
  NR_UNUSED_EXECUTE_DATA;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_FALSE;
  }

  if (FAILURE == zend_parse_parameters_none()) {
    nrl_warning(NRL_API,
                "unable to parse parameters to "
                "newrelic_is_sampled; %d parameters received, expected none",
                ZEND_NUM_ARGS());
  }

  nr_php_api_add_supportability_metric("is_sampled" TSRMLS_CC);

  if (nr_txn_is_sampled(NRPRG(txn))) {
    RETURN_TRUE;
  } else {
    RETURN_FALSE;
  }
}

static void nr_php_add_assoc_string_const(zval* arr,
                                          const char* key,
                                          const char* value) {
  char* val = NULL;

  if (NULL == arr || NULL == key || NULL == value) {
    return;
  }

  val = nr_strdup(value);
  nr_php_add_assoc_string(arr, key, val);
  nr_free(val);
}

#ifdef TAGS
void zif_newrelic_get_linking_metadata(void); /* ctags landing pad only */
void newrelic_get_linking_metadata(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_get_linking_metadata) {
  char* trace_id = NULL;
  char* span_id = NULL;

  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  nr_php_api_add_supportability_metric("get_linking_metadata" TSRMLS_CC);

  array_init(return_value);

  if (FAILURE == zend_parse_parameters_none()) {
    nrl_warning(NRL_API,
                "unable to parse parameters to "
                "newrelic_get_linking_metadata; %d parameters received",
                ZEND_NUM_ARGS());
    return;
  }

  if (nrlikely(NRPRG(app))) {
    nr_php_add_assoc_string_const(return_value, "entity.name",
                                  nr_app_get_entity_name(NRPRG(app)));
    nr_php_add_assoc_string_const(return_value, "entity.type",
                                  nr_app_get_entity_type(NRPRG(app)));
    nr_php_add_assoc_string_const(return_value, "entity.guid",
                                  nr_app_get_entity_guid(NRPRG(app)));
    nr_php_add_assoc_string_const(return_value, "hostname",
                                  nr_app_get_host_name(NRPRG(app)));
  }

  if (nrlikely(NRPRG(txn))) {
    trace_id = nr_txn_get_current_trace_id(NRPRG(txn));
    span_id = nr_txn_get_current_span_id(NRPRG(txn));

    if (trace_id) {
      nr_php_add_assoc_string(return_value, "trace.id", trace_id);
    }

    if (span_id) {
      nr_php_add_assoc_string(return_value, "span.id", span_id);
    }

    nr_free(trace_id);
    nr_free(span_id);
  }
}

#ifdef TAGS
void zif_newrelic_get_trace_metadata(void); /* ctags landing pad only */
void newrelic_get_trace_metadata(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_get_trace_metadata) {
  char* trace_id;
  char* span_id;

  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  nr_php_api_add_supportability_metric("get_trace_metadata" TSRMLS_CC);

  array_init(return_value);

  if (FAILURE == zend_parse_parameters_none()) {
    nrl_warning(NRL_API,
                "unable to parse parameters to newrelic_get_trace_metadata; "
                "%d parameters received",
                ZEND_NUM_ARGS());
    return;
  }

  if (nrlikely(NRPRG(txn))) {
    trace_id = nr_txn_get_current_trace_id(NRPRG(txn));
    if (trace_id) {
      nr_php_add_assoc_string(return_value, "trace_id", trace_id);
    }
    nr_free(trace_id);

    span_id = nr_txn_get_current_span_id(NRPRG(txn));
    if (span_id) {
      nr_php_add_assoc_string(return_value, "span_id", span_id);
    }
    nr_free(span_id);
  }
}

#ifdef TAGS
void zif_newrelic_set_error_group_callback(void); /* ctags landing pad only */
void newrelic_set_error_group_callback(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_set_error_group_callback) {
  zend_fcall_info fci;
  zend_fcall_info_cache fcc;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  nr_php_api_add_supportability_metric("set_error_group_callback" TSRMLS_CC);

  // Verify that only one argument has been provided to the API (the callback)
  if (1 != ZEND_NUM_ARGS()) {
    nrl_warning(NRL_API,
                "newrelic_set_error_group_callback failure: invalid number of "
                "parameters");
    RETURN_FALSE;
  }

  // Verify that the argument passed to the API is a function
  // Populate function data
  if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "f", &fci, &fcc)) {
    nrl_warning(
        NRL_API,
        "newrelic_set_error_group_callback failure: invalid argument passed");
    RETURN_FALSE;
  }

  // Verify the user callback accepts 2 arguments
  if (2 != fcc.function_handler->common.num_args) {
    nrl_warning(NRL_API,
                "newrelic_set_error_group_callback failure: invalid number of "
                "callback parameters: %d",
                fcc.function_handler->common.num_args);
    RETURN_FALSE;
  }

  // Log info message if the user is overwriting an existing callback
  if (NULL != NRPRG(error_group_user_callback)) {
    nrl_info(
        NRL_API,
        "newrelic_set_error_group_callback: overwriting previous callback");
  } else {
    // Allocate memory for global callback reference
    // This is freed (if set) in RSHUTDOWN
    NRPRG(error_group_user_callback) = nr_malloc(sizeof(nrcallbackfn_t));
  }

  // Set global values
  NRPRG(error_group_user_callback)->fci = fci;
  NRPRG(error_group_user_callback)->fcc = fcc;

  nrl_info(
      NRL_API,
      "newrelic_set_error_group_callback success: error group callback set");

  RETURN_TRUE;
}
