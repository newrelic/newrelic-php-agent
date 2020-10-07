/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file contains PHP specific callbacks and functions for transactions.
 */
#include "php_agent.h"
#include "php_api_distributed_trace.h"
#include "php_autorum.h"
#include "php_execute.h"
#include "php_globals.h"
#include "php_hash.h"
#include "php_header.h"
#include "php_output.h"
#include "php_samplers.h"
#include "php_user_instrument.h"
#include "php_stacked_segment.h"
#include "php_txn_private.h"
#include "nr_agent.h"
#include "nr_commands.h"
#include "nr_header.h"
#include "nr_rum.h"
#include "nr_segment_children.h"
#include "nr_txn.h"
#include "nr_version.h"
#include "util_labels.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_metrics.h"
#include "util_number_converter.h"
#include "util_sleep.h"
#include "util_strings.h"

static void nr_php_collect_x_request_start(TSRMLS_D) {
  char* x_request_start;

  /*
   * If an incoming X-Request-Start request header is present, it will be
   * available within $_SERVER as HTTP_X_REQUEST_START.  However, if the
   * customer uses NGINX's fastcgi_param to set X-Request-Start, then
   * X-Request-Start will be available within $_SERVER.  Therefore we check
   * both.
   */
  x_request_start = nr_php_get_server_global("HTTP_X_REQUEST_START" TSRMLS_CC);
  if (0 == x_request_start) {
    x_request_start = nr_php_get_server_global("X-Request-Start" TSRMLS_CC);
  }

  nr_txn_set_queue_start(NRPRG(txn), x_request_start);
  nr_free(x_request_start);
}

static void nr_php_set_initial_path(nrtxn_t* txn TSRMLS_DC) {
  const zval* docroot = NULL;
  const zval* uri = NULL;
  const char* suri = NULL;
  int remove_document_root = 0;
  const zval* server;
  const char* whence = NULL;

  if (NULL == txn) {
    return;
  }

#ifdef PHP7
  server = &PG(http_globals)[TRACK_VARS_SERVER];
#else
  server = PG(http_globals)[TRACK_VARS_SERVER];
#endif /* PHP7 */

  if (nr_php_is_zval_valid_array(server)) {
    if ((NR_PHP_PROCESS_GLOBALS(special_flags).enable_path_translated)
        && (NULL
            != (uri = nr_php_zend_hash_find(Z_ARRVAL_P(server),
                                            "PATH_TRANSLATED")))) {
      whence = "WT_IS_FILENAME & PATH_TRANSLATED";
      remove_document_root = 1;
      /* uri has a zval with the translated path */
    } else if (NULL
               != (uri = nr_php_zend_hash_find(Z_ARRVAL_P(server),
                                               "SCRIPT_FILENAME"))) {
      whence = "WT_IS_FILENAME & SCRIPT_FILENAME";
      remove_document_root = 1;
      /* uri has a zval with the name of the script */
    } else if (NULL
               != (uri = nr_php_zend_hash_find(Z_ARRVAL_P(server),
                                               "SCRIPT_NAME"))) {
      whence = "WT_IS_FILENAME & SCRIPT_NAME";
/* uri has a zval with the name of the script */
#ifdef PHP7
    } else if (CG(active_op_array)) {
      whence = "WT_IS_FILENAME & op_array";
      suri = nr_php_op_array_file_name(CG(active_op_array));
/* suri has a char* to the name of the script */
#else
    } else if (EG(active_op_array)) {
      whence = "WT_IS_FILENAME & op_array";
      suri = nr_php_op_array_file_name(EG(active_op_array));
/* suri has a char* to the name of the script */
#endif /* PHP7 */
    }

    if ((NULL == uri) && (NULL == suri)) {
      const zval* argv;

      /*
       * couldn't find $_SERVER["SCRIPT_NAME"] or $_SERVER["SCRIPT_FILENAME"]
       * or $_SERVER["PATH_TRANSLATED"] so try php's $_SERVER['argv'][0]
       */
      argv = nr_php_zend_hash_find(Z_ARRVAL_P(server), "argv");
      if (nr_php_is_zval_valid_array(argv)) {
        zval* filename = nr_php_zend_hash_index_find(Z_ARRVAL_P(argv), 0);

        if (nr_php_is_zval_valid_string(filename)) {
          whence = "argv[0]";

          if (NULL
              == nr_php_zend_hash_find(Z_ARRVAL_P(server), "REQUEST_METHOD")) {
            nr_txn_set_as_background_job(txn, "argv and no REQUEST_METHOD");
          }
        }
      }
    }

    if (remove_document_root) {
      docroot = nr_php_zend_hash_find(Z_ARRVAL_P(server), "DOCUMENT_ROOT");

      if (!nr_php_is_zval_valid_string(docroot)) {
        docroot = NULL;
      }
    }
  }

  if (uri || suri) {
    nr_string_len_t len;
    char* s;
    char* q;

    if (uri) {
      len = Z_STRLEN_P(uri);
      s = nr_strndup(Z_STRVAL_P(uri), len);
    } else {
      s = nr_strdup(suri);
      len = nr_strlen(s);
    }

    q = nr_strchr(s, '?');
    if (0 != q) {
      *q = 0; /* remove the tailing query string, e.g., ?foo=34 */
    }

    if (NRINI(remove_trailing_path)) { /* remove the tailing /xyz/zy from
                                          foo/bar.php/xyz/zy */
      q = nr_strstr(s, ".php");
      if (0 != q) {
        q[4] = 0;
      }
    }

    if (remove_document_root && docroot) {
      if (0 == nr_strncmp(s, Z_STRVAL_P(docroot), Z_STRLEN_P(docroot))) {
        char* s2
            = nr_strndup(s + Z_STRLEN_P(docroot), len - Z_STRLEN_P(docroot));

        nr_free(s);
        s = s2;
      }
    }
    nr_txn_set_path(whence, txn, s, NR_PATH_TYPE_URI, NR_NOT_OK_TO_OVERWRITE);
    nr_free(s);
  } else {
    /* couldn't find _SERVER[REQUEST_URI] or argv[0] thus we'll have no path
     * information */
    nr_txn_set_path("default initial path", txn, "<unknown>",
                    NR_PATH_TYPE_UNKNOWN, NR_NOT_OK_TO_OVERWRITE);
  }
}

static int nr_php_capture_request_parameter(zval* element,
                                            nrtxn_t* txn,
                                            zend_hash_key* key TSRMLS_DC) {
#define NR_MAX_STRLEN 512
  char keystr[NR_MAX_STRLEN];
  char datastr[NR_MAX_STRLEN];

  keystr[0] = '\0';
  datastr[0] = '\0';

  if (nr_php_zend_hash_key_is_string(key)) {
    nr_string_len_t keylen = nr_php_zend_hash_key_string_len(key);

    nr_strxcpy(keystr, nr_php_zend_hash_key_string_value(key),
               keylen < NR_MAX_STRLEN ? keylen : NR_MAX_STRLEN - 1);
  } else {
    snprintf(keystr, sizeof(keystr), NR_ULONG_FMT,
             nr_php_zend_hash_key_integer(key));
  }

  nr_php_zval_unwrap(element);

  switch (Z_TYPE_P(element)) {
    case IS_NULL:
      nr_strcpy(datastr, "(null)");
      break;

    case IS_LONG:
      snprintf(datastr, sizeof(datastr), "%ld", (long)Z_LVAL_P(element));
      break;

    case IS_DOUBLE:
      nr_double_to_str(datastr, sizeof(datastr), Z_DVAL_P(element));
      break;

#ifdef PHP7
    case IS_TRUE:
      nr_strcpy(datastr, "true");
      break;

    case IS_FALSE:
      nr_strcpy(datastr, "false");
      break;
#else
    case IS_BOOL:
      nr_strcpy(datastr, Z_BVAL_P(element) ? "true" : "false");
      break;
#endif /* PHP7 */

    case IS_STRING: {
      nr_string_len_t len;

#ifdef PHP7
      if (NULL == Z_STR_P(element)) {
        nr_strcpy(datastr, "[invalid string]");
        break;
      }
#endif /* PHP7 */

      len = Z_STRLEN_P(element) < NR_MAX_STRLEN ? Z_STRLEN_P(element)
                                                : NR_MAX_STRLEN - 1;

      nr_strxcpy(datastr, Z_STRVAL_P(element), len);
    } break;

    case IS_ARRAY:
      nr_strcpy(datastr, "[array]");
      break;

    case IS_OBJECT:
      nr_strcpy(datastr, "[object]");
      break;

    case IS_RESOURCE:
      nr_strcpy(datastr, "[resource]");
      break;

#if ZEND_MODULE_API_NO < ZEND_7_3_X_API_NO
    case IS_CONSTANT:
      nr_strcpy(datastr, "[constant]");
      break;
#endif /* PHP < 7.3 */

/*
 * PHP 5.6.0 beta 2 replaced IS_CONSTANT_ARRAY and IS_CONSTANT_INDEX with
 * IS_CONSTANT_AST. For the purposes of this function, it can be
 * considered the same thing.
 */
#if ZEND_MODULE_API_NO >= ZEND_5_6_X_API_NO
    case IS_CONSTANT_AST:
#else
    case IS_CONSTANT_ARRAY:
#endif /* PHP >= 5.6 */
      nr_strcpy(datastr, "[constants]");
      break;

    default:
      nr_strcpy(datastr, "[?]");
      break;
  }

  nr_txn_add_request_parameter(txn, keystr, datastr,
                               NRPRG(deprecated_capture_request_parameters));

  return ZEND_HASH_APPLY_KEEP;
}

static void nr_php_capture_request_parameters(nrtxn_t* txn TSRMLS_DC) {
  zval* data = 0;

  if (0 == txn) {
    return;
  }

  data = nr_php_zend_hash_find(&EG(symbol_table), "_REQUEST");
  if (!nr_php_is_zval_valid_array(data)) {
    return;
  }

  nr_php_zend_hash_zval_apply(
      Z_ARRVAL_P(data), (nr_php_zval_apply_t)nr_php_capture_request_parameter,
      txn TSRMLS_CC);
}

static void nr_php_attribute_helper(nrtxn_t* txn,
                                    const char* name,
                                    const nr_txn_attribute_t* attribute
                                        TSRMLS_DC) {
  char* value = nr_php_get_server_global(name TSRMLS_CC);

  nr_txn_set_string_attribute(txn, attribute, value);
  nr_free(value);
}

static void nr_php_gather_global_params(nrtxn_t* txn TSRMLS_DC) {
  char* request_referer;
  char* content_length;

  if (0 == txn) {
    return;
  }
  request_referer = nr_php_get_server_global("HTTP_REFERER" TSRMLS_CC);
  content_length = nr_php_get_server_global("CONTENT_LENGTH" TSRMLS_CC);

  nr_txn_set_request_referer(txn, request_referer);
  nr_txn_set_request_content_length(txn, content_length);

  nr_php_attribute_helper(txn, "HTTP_ACCEPT",
                          nr_txn_request_accept_header TSRMLS_CC);
  nr_php_attribute_helper(txn, "CONTENT_TYPE",
                          nr_txn_request_content_type TSRMLS_CC);
  nr_php_attribute_helper(txn, "HTTP_HOST", nr_txn_request_host TSRMLS_CC);
  nr_php_attribute_helper(txn, "REQUEST_METHOD",
                          nr_txn_request_method TSRMLS_CC);
  nr_php_attribute_helper(txn, "HTTP_USER_AGENT",
                          nr_txn_request_user_agent_deprecated TSRMLS_CC);
  nr_php_attribute_helper(txn, "HTTP_USER_AGENT",
                          nr_txn_request_user_agent TSRMLS_CC);
  nr_php_attribute_helper(txn, "SERVER_NAME", nr_txn_server_name TSRMLS_CC);

  nr_free(request_referer);
  nr_free(content_length);
}

static void nr_php_modify_attribute_destinations(nr_attribute_config_t* config,
                                                 const char* prefix,
                                                 const char* str,
                                                 uint32_t include,
                                                 uint32_t exclude) {
  int i;
  nrobj_t* rs;
  int ns = 0;

  if ((0 == str) || (0 == str[0])) {
    return;
  }

  rs = nr_strsplit(str, ",", 0);
  ns = nro_getsize(rs);

  if (rs && (ns > 0)) {
    for (i = 0; i < ns; i++) {
      char* match;
      const char* s = nro_get_array_string(rs, i + 1, NULL);

      match = nr_formatf("%s%s", NRBLANKSTR(prefix), NRBLANKSTR(s));
      nr_attribute_config_modify_destinations(config, match, include, exclude);
      nr_free(match);
    }
  }

  nro_delete(rs);
}

/*
 * Purpose : Determine whether an attribute destination should be disabled.
 *
 * Returns : The destination if it should be disabled, and 0 otherwise.
 */
static uint32_t nr_php_attribute_disable_destination_helper(
    const char* new_setting_name,
    int new_setting_value,
    int old_setting_value,
    uint32_t destination) {
  if (nr_php_ini_setting_is_set_by_user(new_setting_name)) {
    /*
     * If the new setting has been explictly set by the user, then use its
     * value.
     */
    if (0 == new_setting_value) {
      return destination;
    } else {
      return 0;
    }
  }

  /*
   * If the new setting has not been explicitly set by the user, then use
   * the replaced setting's value.  Note that this works since the new setting
   * and the replaced setting have the same default value.
   */
  if (0 == old_setting_value) {
    return destination;
  } else {
    return 0;
  }
}

/*
 * Purpose : Translate the PHP ini settings into axiom's attribute configuration
 *           format.  This is messy because of the many old deprecated ini
 *           settings that still need to be supported.
 */
static nr_attribute_config_t* nr_php_create_attribute_config(TSRMLS_D) {
  nr_attribute_config_t* config;
  uint32_t disabled_destinations = 0;

  config = nr_attribute_config_create();

  /*
   * Handle the disabled destinations.
   */
  disabled_destinations |= nr_php_attribute_disable_destination_helper(
      "newrelic.transaction_tracer.attributes.enabled",
      NRINI(transaction_tracer_attributes.enabled),
      NRINI(transaction_tracer_capture_attributes),
      NR_ATTRIBUTE_DESTINATION_TXN_TRACE);

  disabled_destinations |= nr_php_attribute_disable_destination_helper(
      "newrelic.error_collector.attributes.enabled",
      NRINI(error_collector_attributes.enabled),
      NRINI(error_collector_capture_attributes),
      NR_ATTRIBUTE_DESTINATION_ERROR);

  disabled_destinations |= nr_php_attribute_disable_destination_helper(
      "newrelic.transaction_events.attributes.enabled",
      NRINI(transaction_events_attributes.enabled),
      NRINI(analytics_events_capture_attributes),
      NR_ATTRIBUTE_DESTINATION_TXN_EVENT);

  disabled_destinations |= nr_php_attribute_disable_destination_helper(
      "newrelic.span_events.attributes.enabled",
      NRINI(span_events_attributes.enabled),
      NRINI(span_events_attributes.enabled), NR_ATTRIBUTE_DESTINATION_SPAN);

  disabled_destinations |= nr_php_attribute_disable_destination_helper(
      "newrelic.browser_monitoring.attributes.enabled",
      NRINI(browser_monitoring_attributes.enabled),
      NRINI(browser_monitoring_capture_attributes),
      NR_ATTRIBUTE_DESTINATION_BROWSER);

  if (0 == NRINI(attributes.enabled)) {
    disabled_destinations |= NR_ATTRIBUTE_DESTINATION_ALL;
  }

  nr_attribute_config_disable_destinations(config, disabled_destinations);

  /*
   * Handle the destination modifiers.
   */
  nr_php_modify_attribute_destinations(
      config, 0, NRINI(transaction_tracer_attributes.include),
      NR_ATTRIBUTE_DESTINATION_TXN_TRACE, 0);
  nr_php_modify_attribute_destinations(
      config, 0, NRINI(transaction_tracer_attributes.exclude), 0,
      NR_ATTRIBUTE_DESTINATION_TXN_TRACE);

  nr_php_modify_attribute_destinations(
      config, 0, NRINI(error_collector_attributes.include),
      NR_ATTRIBUTE_DESTINATION_ERROR, 0);
  nr_php_modify_attribute_destinations(
      config, 0, NRINI(error_collector_attributes.exclude), 0,
      NR_ATTRIBUTE_DESTINATION_ERROR);

  nr_php_modify_attribute_destinations(
      config, 0, NRINI(transaction_events_attributes.include),
      NR_ATTRIBUTE_DESTINATION_TXN_EVENT, 0);
  nr_php_modify_attribute_destinations(
      config, 0, NRINI(transaction_events_attributes.exclude), 0,
      NR_ATTRIBUTE_DESTINATION_TXN_EVENT);

  nr_php_modify_attribute_destinations(config, 0,
                                       NRINI(span_events_attributes.include),
                                       NR_ATTRIBUTE_DESTINATION_SPAN, 0);
  nr_php_modify_attribute_destinations(config, 0,
                                       NRINI(span_events_attributes.exclude), 0,
                                       NR_ATTRIBUTE_DESTINATION_SPAN);

  nr_php_modify_attribute_destinations(
      config, 0, NRINI(browser_monitoring_attributes.include),
      NR_ATTRIBUTE_DESTINATION_BROWSER, 0);
  nr_php_modify_attribute_destinations(
      config, 0, NRINI(browser_monitoring_attributes.exclude), 0,
      NR_ATTRIBUTE_DESTINATION_BROWSER);

  nr_php_modify_attribute_destinations(config, 0, NRINI(attributes.include),
                                       NR_ATTRIBUTE_DESTINATION_ALL, 0);
  nr_php_modify_attribute_destinations(config, 0, NRINI(attributes.exclude), 0,
                                       NR_ATTRIBUTE_DESTINATION_ALL);

  /*
   * Handle deprecated ignored request parameters.
   */
  nr_php_modify_attribute_destinations(
      config, NR_TXN_REQUEST_PARAMETER_ATTRIBUTE_PREFIX, NRINI(ignored_params),
      0, NR_ATTRIBUTE_DESTINATION_ALL);

  /*
   * The deprecated capture_params is handled when request parameters are
   * captured.
   */

  return config;
}

static nrobj_t* nr_php_txn_get_labels(TSRMLS_D) {
  const char* input;

  /*
   * Specifying labels via an environment variable is not supported.
   */
  input = NRINI(labels);

  return nr_labels_parse(input);
}

static void nr_php_txn_prepared_statement_destroy(void* sql) {
  nr_free(sql);
}

bool nr_php_txn_is_policy_secure(const char* policy_name,
                                 const nrtxnopt_t* opts) {
  if (NULL == policy_name) {
    return false;
  }

  if (NULL == opts) {
    return false;
  }

  if (0 == strcmp("record_sql", policy_name)) {
    /*
     * record_sql is considered more secure only when there's no
     * sql reporting.  NR_SQL_RAW and NR_SQL_OBFUSCATED are
     * considered the less secure values.
     */
    return NR_SQL_NONE == opts->tt_recordsql;
  }

  if (0 == strcmp("allow_raw_exception_messages", policy_name)) {
    /* 
     * allow_raw_exception_messages is considered insecure when
     * the private newrelic.allow_raw_exception_messages ini value is 1.
     */
    return 0 == opts->allow_raw_exception_messages;
  }

  if (0 == strcmp("custom_events", policy_name)) {
    /* 
     * custom_events is considered insecure when the
     * newrelic.custom_events_enabled ini value is 1.
     */
    return 0 == opts->custom_events_enabled;
  }

  if (0 == strcmp("custom_parameters", policy_name)) {
    /* 
     * custom_parameters is considered insecure when the
     * newrelic.custom_parameters_enabled ini value is 1.
     */
    return 0 == opts->custom_parameters_enabled;
  }

  /*
   * We reach this point when the policy name is unknown
   */
  nrl_debug(NRL_INIT, "Request unknown security policy: %s", policy_name);
  return false;
}

nrobj_t* nr_php_txn_get_supported_security_policy_settings(nrtxnopt_t* opts) {
  nrobj_t* supported_policy_settings = nro_new(NR_OBJECT_HASH);
  int i;
  int count_supported_policy_names;

  /* 
   * The policies we support.  Non supported policies are omitted to save
   * space on the wire (vs. sending them with support/enabled of 0.
   */
  const char* supported_policy_names[]
      = {"record_sql", "allow_raw_exception_messages", "custom_events",
         "custom_parameters"};

  count_supported_policy_names = sizeof(supported_policy_names) / sizeof(char*);
  /* setup default values and object structure for supported policies */
  for (i = 0; i < count_supported_policy_names; i++) {
    nrobj_t* tmp;
    tmp = nro_new(NR_OBJECT_HASH);
    nro_set_hash_boolean(
        tmp, "enabled",
        !nr_php_txn_is_policy_secure(supported_policy_names[i], opts));
    nro_set_hash_boolean(tmp, "supported", 1);
    nro_set_hash(supported_policy_settings, supported_policy_names[i], tmp);
    nro_delete(tmp);
  }

  return supported_policy_settings;
}

nr_status_t nr_php_txn_begin(const char* appnames,
                             const char* license TSRMLS_DC) {
  nrtxnopt_t opts;
  const char* lic_to_use;
  int pfd;
  nr_attribute_config_t* attribute_config;
  nr_app_info_t info;
  bool is_cli = (0 != NR_PHP_PROCESS_GLOBALS(cli));

  if ((0 == NR_PHP_PROCESS_GLOBALS(enabled)) || (0 == NRINI(enabled))) {
    return NR_FAILURE;
  }

  /*
   * Transaction globals must be zeroed out, whether the transaction can be
   * started or not.
   */
  memset(&NRPRG(txn_globals), 0, sizeof(NRPRG(txn_globals)));

  /*
   * This call will attempt to ensure we are connected to the daemon. It is
   * non-blocking so it is pretty quick. If we had no connection and the daemon
   * has since been brought back up, this will start the process of connecting
   * to it.
   */
  pfd = nr_get_daemon_fd();

  nr_php_reset_user_instrumentation();

  if (pfd < 0) {
    nrl_debug(NRL_INIT, "unable to begin transaction: no daemon connection");
    return NR_FAILURE;
  }

  lic_to_use = nr_php_use_license(license TSRMLS_CC);
  if (0 == lic_to_use) {
    /*
     * Log a debug message if there is not proper-looking license key.
     * Note that this message is debug and not warning:  It may be that the
     * user does not have a default license, and expects to provide one
     * using the API (with newrelic_start_transaction).  In that case, we
     * do not want to log a warning when this function is called by every
     * RINIT.
     */
    nrl_debug(NRL_INIT,
              "unable to begin transaction: license key is missing "
              "or has an invalid format");
    return NR_FAILURE;
  }

  opts.custom_events_enabled = (int)NRINI(custom_events_enabled);
  opts.synthetics_enabled = (int)NRINI(synthetics_enabled);
  opts.instance_reporting_enabled = (int)NRINI(instance_reporting_enabled);
  opts.database_name_reporting_enabled
      = (int)NRINI(database_name_reporting_enabled);
  opts.err_enabled = (int)NRINI(errors_enabled);
  opts.request_params_enabled = (int)NRINI(capture_params);
  opts.autorum_enabled = (int)NRINI(browser_monitoring_auto_instrument);
  opts.analytics_events_enabled = (int)NRINI(analytics_events_enabled)
                                  && (int)NRINI(transaction_events_enabled);
  opts.error_events_enabled = (int)NRINI(error_events_enabled);
  opts.tt_enabled = (int)NRINI(tt_enabled);
  opts.ep_enabled = (int)NRINI(ep_enabled);
  opts.tt_recordsql = (nr_tt_recordsql_t)NRINI(tt_recordsql);
  opts.tt_slowsql = (int)NRINI(tt_slowsql);
  opts.apdex_t = 0; /* Set by application */
  opts.tt_threshold = NRINI(tt_threshold);
  opts.ep_threshold = NRINI(ep_threshold);
  opts.ss_threshold = NRINI(ss_threshold);
  opts.cross_process_enabled = (int)NRINI(cross_process_enabled);
  opts.tt_is_apdex_f = NRPRG(tt_threshold_is_apdex_f);
  opts.allow_raw_exception_messages = NRINI(allow_raw_exception_messages);
  opts.custom_parameters_enabled = NRINI(custom_parameters_enabled);
  opts.distributed_tracing_enabled = NRINI(distributed_tracing_enabled);
  opts.distributed_tracing_exclude_newrelic_header
      = NRINI(distributed_tracing_exclude_newrelic_header);
  opts.span_events_enabled = NRINI(span_events_enabled);
  opts.max_span_events = NRINI(max_span_events);
  opts.max_segments
      = is_cli ? NRINI(tt_max_segments_cli) : NRINI(tt_max_segments_web);
  opts.span_queue_batch_size = NRINI(agent_span_queue_size);
  opts.span_queue_batch_timeout = NRINI(agent_span_queue_timeout);

  /*
   * Enable the behaviour whereby asynchronous time is discounted from the total
   * time. This matches the actual behaviour of PHP when Predis and Guzzle are
   * used, which are the only methods by which the PHP agent can create
   * asynchronous segments.
   *
   * In the future, when the PHP agent has support for threaded or evented PHP
   * frameworks, we may want to make this toggleable.
   */
  opts.discount_main_context_blocking = true;

  if ((0 == appnames) || (0 == appnames[0])) {
    appnames = NRINI(appnames);
  }

  nr_memset(&info, 0, sizeof(info));
  info.high_security = NR_PHP_PROCESS_GLOBALS(high_security);
  info.license = nr_strdup(lic_to_use);
  info.settings = NULL; /* Populated through callback. */
  info.environment = nro_copy(NR_PHP_PROCESS_GLOBALS(appenv));
  info.labels = nr_php_txn_get_labels(TSRMLS_C);
  info.host_display_name = nr_strdup(NRINI(process_host_display_name));
  info.lang = nr_strdup("php");
  info.version = nr_strdup(nr_version());
  info.appname = nr_strdup(appnames);
  info.redirect_collector = nr_strdup(NR_PHP_PROCESS_GLOBALS(collector));
  info.security_policies_token = nr_strdup(NRINI(security_policies_token));
  info.supported_security_policies
      = nr_php_txn_get_supported_security_policy_settings(&opts);
  info.trace_observer_host = nr_strdup(NRINI(trace_observer_host));
  info.trace_observer_port = NRINI(trace_observer_port);
  info.span_queue_size = NRINI(span_queue_size);

  NRPRG(app) = nr_agent_find_or_add_app(
      nr_agent_applist, &info,
      /*
       * Settings are provided through a callback:
       * They cannot be calculated once per process,
       * since they depend on per request settings.
       * They are not calculated each transaction to
       * reduce overhead.
       */
      &nr_php_app_settings, NR_PHP_PROCESS_GLOBALS(daemon_app_connect_timeout));
  nr_app_info_destroy_fields(&info);

  if (0 == NRPRG(app)) {
    nrl_debug(NRL_INIT, "unable to begin transaction: app '%.128s' is unknown",
              appnames ? appnames : "");
    return NR_FAILURE;
  }

  attribute_config = nr_php_create_attribute_config(TSRMLS_C);
  NRPRG(txn) = nr_txn_begin(NRPRG(app), &opts, attribute_config);
  nrt_mutex_unlock(&(NRPRG(app)->app_lock));

  nr_attribute_config_destroy(&attribute_config);

  if (0 == NRPRG(txn)) {
    nrl_debug(NRL_INIT, "no Axiom transaction this time around");
    return NR_FAILURE;
  }

  /*
   * Disable automated parenting for the default parent context. See
   * php_stacked_segment.h for further details.
   */
  nr_txn_force_current_segment(NRPRG(txn), NRTXN(segment_root));

  nr_php_collect_x_request_start(TSRMLS_C);
  nr_php_set_initial_path(NRPRG(txn) TSRMLS_CC);

  nr_txn_add_match_files(NRPRG(txn), NRINI(file_name_list));

  if (is_cli) {
    nr_txn_set_as_background_job(NRPRG(txn), "CLI SAPI");
  }

  NRTXNGLOBAL(mysqli_links) = nr_mysqli_metadata_create();

  nr_php_add_user_instrumentation(TSRMLS_C);
  nr_php_resource_usage_sampler_start(TSRMLS_C);
  nr_php_gather_global_params(NRPRG(txn) TSRMLS_CC);

  NRTXN(special_flags.no_sql_parsing)
      = NR_PHP_PROCESS_GLOBALS(special_flags).no_sql_parsing;
  NRTXN(special_flags.show_sql_parsing)
      = NR_PHP_PROCESS_GLOBALS(special_flags).show_sql_parsing;
  NRTXN(special_flags.debug_cat)
      = NR_PHP_PROCESS_GLOBALS(special_flags).debug_cat;

  NRTXNGLOBAL(prepared_statements)
      = nr_hashmap_create(nr_php_txn_prepared_statement_destroy);

  /*
   * Install the cross process buffer handler:  See the documentation of
   * nr_php_header_output_handler for explanation of its purpose and the
   * the conditionals.
   *
   * Output handlers are technically request globals. However, one can only
   * sensibly check whether to install them once one has an initialized
   * transaction.
   *
   * Already installed handlers are not overwritten.
   */
  if (nr_rum_do_autorum(NRPRG(txn))) {
    nr_php_output_install_handler("New Relic auto-RUM",
                                  nr_php_rum_output_handler TSRMLS_CC);
  }
  if ((NR_STATUS_CROSS_PROCESS_START == NRTXN(status.cross_process))
      && nr_php_has_request_header("HTTP_X_NEWRELIC_ID" TSRMLS_CC)) {
    nr_php_output_install_handler("New Relic header",
                                  nr_php_header_output_handler TSRMLS_CC);
  }

  if (NRPRG(txn)->options.distributed_tracing_enabled) {
    char* nr_payload = nr_php_get_request_header("HTTP_NEWRELIC" TSRMLS_CC);
    char* traceparent = nr_php_get_request_header("HTTP_TRACEPARENT" TSRMLS_CC);
    char* tracestate = nr_php_get_request_header("HTTP_TRACESTATE" TSRMLS_CC);

    nr_hashmap_t* header_map = nr_header_create_distributed_trace_map(
        nr_payload, traceparent, tracestate);
    nr_php_api_accept_distributed_trace_payload_httpsafe(NRPRG(txn), header_map,
                                                         "HTTP");

    nr_hashmap_destroy(&header_map);
    nr_free(nr_payload);
    nr_free(traceparent);
    nr_free(tracestate);
  } else if (NRPRG(txn)->options.cross_process_enabled) {
    char* x_newrelic_id = NULL;
    char* x_newrelic_transaction = NULL;

    x_newrelic_id = nr_php_get_request_header("HTTP_X_NEWRELIC_ID" TSRMLS_CC);
    x_newrelic_transaction
        = nr_php_get_request_header("HTTP_X_NEWRELIC_TRANSACTION" TSRMLS_CC);

    (void)nr_header_set_cat_txn(NRPRG(txn), x_newrelic_id,
                                x_newrelic_transaction);

    nr_free(x_newrelic_id);
    nr_free(x_newrelic_transaction);
  }

  if (NRPRG(txn)->options.synthetics_enabled) {
    char* encoded = NULL;

    encoded = nr_php_get_request_header("HTTP_X_NEWRELIC_SYNTHETICS" TSRMLS_CC);
    (void)nr_header_set_synthetics_txn(NRPRG(txn), encoded);

    nr_free(encoded);
  }

  if (NRPRG(txn)->options.distributed_tracing_enabled
      && !NRPRG(txn)->options.tt_enabled) {
    nrl_error(NRL_INIT,
              "newrelic.transaction_tracer.enabled must be enabled in order "
              "to use distributed tracing");
  }

  if (nr_php_ini_setting_is_set_by_user("opcache.preload")
      && NR_PHP_PROCESS_GLOBALS(preload_framework_library_detection)) {
    nr_php_user_instrumentation_from_opcache(TSRMLS_C);
  }

  return NR_SUCCESS;
}

static int nr_php_txn_should_ignore(int ignoretxn TSRMLS_DC) {
  if (NRTXN(status.ignore)) {
    ignoretxn = 1;
  }

  if ((NRTXN(status.background))
      && (NR_PHP_PROCESS_GLOBALS(special_flags).no_background_jobs)) {
    ignoretxn = 1;
  }

  return ignoretxn;
}

/* 
 * Actually shutdown, without checking if we should ignore or if the txn is
 * NULL. 
 */
static void nr_php_txn_do_shutdown(nrtxn_t* txn TSRMLS_DC) {
  char* request_uri;

  request_uri = nr_php_get_server_global("REQUEST_URI" TSRMLS_CC);
  nr_txn_set_request_uri(txn, request_uri);
  nr_free(request_uri);

  /*
   * Request parameters are captured here at the end of the request
   * in case the user has used newrelic_enable_params or
   * newrelic_capture_params.  Note, therefore, that request parameters
   * cannot be configured into the browser client config.
   */
  nr_php_capture_request_parameters(txn TSRMLS_CC);

  nr_hashmap_destroy(&NRTXNGLOBAL(mysqli_queries));
  nr_hashmap_destroy(&NRTXNGLOBAL(pdo_link_options));
}

void nr_php_txn_shutdown(TSRMLS_D) {
  nrtxn_t* txn = NRPRG(txn);

  if (NULL != txn) {
    nr_php_txn_do_shutdown(txn TSRMLS_CC);
  }
}

void nr_php_txn_handle_fpm_error(nrtxn_t* txn TSRMLS_DC) {
  if (nrunlikely(NULL == txn)) {
    return;
  }

  /*
   * PHP-FPM starts and stops a transaction even if the script it's trying to
   * load doesn't exist or can't be loaded. To avoid a potential MGI on the URI
   * naming, we'll detect that case using a combination of the SAPI name,
   * response code, and whether we ever saw a PHP function or file frame, and
   * if so, use a status code transaction name.
   *
   * Technically, the call count and path type checks are redundant in normal
   * use, but are here just in case anyone is doing crazy things where their
   * entire request is handled in an extension and no PHP frame ever occurs.
   */
  if (NR_PATH_TYPE_URI == txn->status.path_type
      && nr_streq(sapi_module.name, "fpm-fcgi")
      && 0 == NRTXNGLOBAL(execute_count)) {
    char* response_code = nr_formatf("%d", nr_php_http_response_code(TSRMLS_C));

    nr_txn_set_string_attribute(txn, nr_txn_request_uri, txn->path);

    nr_txn_set_path("FPM status code", txn, response_code,
                    NR_PATH_TYPE_STATUS_CODE, NR_NOT_OK_TO_OVERWRITE);

    nr_free(response_code);
  }
}

static void nr_php_txn_set_response_header_attributes(nrtxn_t* txn TSRMLS_DC) {
  char* content_type;
  int content_length;

  if (NULL == txn) {
    return;
  }

  if (txn->status.background) {
    return;
  }

  content_type = nr_php_get_response_content_type(TSRMLS_C);
  nr_txn_set_string_attribute(txn, nr_txn_response_content_type, content_type);
  nr_free(content_type);

  content_length = nr_php_get_response_content_length(TSRMLS_C);
  if (content_length < 0) {
    return;
  }

  nr_txn_set_long_attribute(txn, nr_txn_response_content_length,
                            content_length);
}

nr_status_t nr_php_txn_end(int ignoretxn, int in_post_deactivate TSRMLS_DC) {
  nr_status_t ret;

  if (NULL == NRPRG(txn)) {
    return NR_SUCCESS;
  }

  /* Stop all recording although we shouldn't be getting anything */
  NRTXN(status.recording) = 0;

  /*
   * If a transaction is ended while stacked segments are active (e. g.
   * by calling newrelic_end_transaction inside nested function scopes)
   * the stack of stacked segments has to be cleaned up.
   */
  nr_php_stacked_segment_unwind(TSRMLS_C);

  ignoretxn = nr_php_txn_should_ignore(ignoretxn TSRMLS_CC);

  /* Add the remaining metrics that aren't added on shutdown. */
  if (0 == ignoretxn) {
    nrtxn_t* txn = NRPRG(txn);

    /*
     * We can't access server superglobals if we're in the post-deactivate
     * handler, as they've been freed. The Zend Engine usually sets the type to
     * IS_NULL from PHP 5.3 onwards, but it appears there are
     * not-completely-deterministic cases where this doesn't happen, so instead
     * we just won't ever call nr_php_txn_do_shutdown in this case.
     *
     * Unfortunately, the Zend Engine doesn't actually track the state it's in,
     * so it's effectively impossible to tell in nr_php_get_server_global
     * whether the access is actually valid.
     */
    if (0 == in_post_deactivate) {
      nr_php_txn_do_shutdown(txn TSRMLS_CC);
    }

    nrm_force_add(txn->unscoped_metrics,
                  "Supportability/execute/user/call_count",
                  NRTXNGLOBAL(execute_count));

    nrm_force_add(txn->unscoped_metrics,
                  "Supportability/execute/allocated_segment_count",
                  nr_txn_allocated_segment_count(txn));

    /* Add CPU and memory metrics */
    nr_php_resource_usage_sampler_end(TSRMLS_C);

    nr_txn_set_http_status(txn, nr_php_http_response_code(TSRMLS_C));

    nr_framework_create_metric(TSRMLS_C);

    nr_php_txn_set_response_header_attributes(txn TSRMLS_CC);

    nr_php_txn_handle_fpm_error(txn TSRMLS_CC);

    nr_txn_finalize_parent_stacks(txn);

    nr_txn_end(txn);

    if (0 == txn->status.ignore) {
      /*
       * Check status.ignore again in case it has changed during nr_txn_end.
       */
      ret = nr_cmd_txndata_tx(nr_get_daemon_fd(), txn);
      if (NR_FAILURE == ret) {
        nrl_debug(NRL_TXN, "failed to send txn");
      }
    }
  }

  nr_txn_destroy(&NRPRG(txn));

  nr_hashmap_destroy(&NRTXNGLOBAL(guzzle_objs));

  nr_hashmap_destroy(&NRTXNGLOBAL(prepared_statements));
  nr_hashmap_destroy(&NRTXNGLOBAL(curl_metadata));
  nr_hashmap_destroy(&NRTXNGLOBAL(curl_multi_metadata));

  nr_mysqli_metadata_destroy(&NRTXNGLOBAL(mysqli_links));

  return NR_SUCCESS;
}
