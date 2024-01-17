/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "php_agent.h"
#include "php_execute.h"
#include "php_globals.h"
#include "php_hash.h"
#include "php_internal_instrument.h"
#include "php_user_instrument.h"

#include "nr_commands.h"
#include "nr_configstrings.h"
#include "nr_limits.h"
#include "nr_version.h"
#include "nr_log_level.h"
#include "util_buffer.h"
#include "util_json.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_url.h"

/*
 * This file deals with INI variables.
 *
 * This file deals with the relatively complex task of initializing, modifying
 * and tracking INI variables. To the greatest extent possible this file uses
 * standard PHP macros for doing all its work, in order to reduce porting
 * complexities for future versions of PHP.
 *
 * This file is complex because:
 *
 * - It acts as a cross bar switch, moving data from PHPs realm into the agent's
 *   realm in various ways, although not necessarily the other direction.
 *
 * - It implements visibility and modifiability semantics for various ini
 *   settings, depending on the minit/rinit cycle.
 *
 * - It implements display semantics, as needed for php -i or phpinfo(),
 *   generating straight text or html, as appropriate.
 */

/*
 * The cross bar and visibility semantics are implemented by calls to PHP
 * standard macros. PHP_INI_ENTRY_EX has no mechanisms to route data structure
 * pointers into modify handlers, STD_PHP_INI_ENTRY_EX has mechanisms to
 * route data structure pointers into modify handlers. Use of these macros are
 * wrapped between PHP_INI_BEGIN and PHP_INI_END.
 *
 * At some point in the cycle, the static const array data structure of type
 * zend_ini_entry[] which is implicit between PHP_INI_BEGIN and PHP_INI_END is
 * copied into a hash table indexed by the ini entry name, such as
 * "newrelic.license". This hash table contains one entry for all ini entries,
 * across all module names.
 *
 * When the PHP engine processes an ini setting, it is working entirely
 * with strings. The engine first modifies the string value held
 * in the zend_ini_entry data structure, and arranges to call the
 * modify handler to disseminate the value. It's up to the modify
 * handler to convert the string value held by PHP into an appropriate
 * implementation type (such as int), and disseminate the value as it
 * sees fit. The modify handler can call whatever it wants, to change
 * whatever far and wide data structure it chooses.
 *
 * When the PHP engine regurgitates an ini setting, as for example from
 * a call to PHP ini_get() function, or in the course of evaluating
 * phpinfo(), it consults the string value held in the hash table
 * wrapping over the zend_ini_data structures. For the case of ini_get,
 * the string value is just returned back through ini_get. For the case
 * of phpinfo() (or php -i), the value is given to a display handler.
 * The default display handler merely prints the string in one of
 * several formats, although we use several custom display handlers to
 * slightly customize the formatting, as for example to obscure the
 * middle of the license key string.
 *
 * IMPORTANT:
 *
 * Recall that our discipline of using the modify handler is very lax:
 * it can call anything it wants, to modify other data structures. These
 * other data structures contain the agent's version of truth. These
 * data structures can be modified by calls to the Agent API (such as
 * a call to newrelic_set_appname), or in the course of the agent's
 * execution. NONE of the display handlers (or ini_get, for that matter)
 * chase down these "other" data structures, so what's regurgitated this
 * way may not reflect what's really happening.
 *
 * Fortunately, most of the ini settings are bound into the
 * newrelic_globals data structure, which represents the agent's version
 * of truth.
 *
 * Copies are problematic, but that's what we have: PHP's string copy of the
 * value at one time, and the agent's actual value, which depending on
 * the particular init setting, can live "anywhere".
 *
 * Note that the API also provides a limited number of getter functions.
 */

/*
 * For Zend/PHP5, the various sources of ini settings' values are
 * processed in the order shown in the following case analysis table.
 * This table was made by tracking the calls to nr_license_mh with
 * specific values originating in specific places.
 *
 * Case 1: php invoked with -dvar=value
 *   1: -dvar=value from the command line (stage == ZEND_INI_STAGE_STARTUP)
 *   2: the value comes from the table between PHP_INI_BEGIN and PHP_INI_END
 *      (stage == ZEND_INI_STAGE_STARTUP)
 *
 * Case 2: php not invoked with -dvar=value
 *   1: the value comes from the newrelic.ini file (stage ==
 *      ZEND_INI_STAGE_STARTUP) (minit time)
 *
 * Case 3: newrelic.ini file present, but the value is commented out
 *   1: the value comes from the table between PHP_INI_BEGIN and PHP_INI_END
 *      (stage == ZEND_INI_STAGE_STARTUP)
 *
 * Then, the value given in the .htaccess file, if any, would be acted on
 * through the sapi. An example .htaccess file might contain: php_value
 * newrelic.appname "special app name"
 *
 * Then, calls to PHP ini_set() would be acted on, assuming that the
 * individual ini setting has allowed this PHP_INI_USER semantics.
 * Since this is disallowed for all of our ini settings, this situation
 * doesn't come up.
 *
 * Note that the modify handler would be called again with the original value
 * to restore the previous settings.
 */

/*
 * This is what happens when running as a threaded PHP as a worker MPM
 * with ZTS. This mode is not officially supported by New Relic.
 *
 * 0a: zm_startup_newrelic is called for module initialization
 *
 * 1. The modify handler is called* from zm_startup_newrelic (module
 *    init time), to consume the value held in the newrelic.ini file
 *
 * 2. The modify handler is called* again from
 *    zend_new_thread_end_handler called* from allocate_new_resource.
 *
 * Important: This is acting on work done by end_copy_ini_directives,
 * which copies ini entries from the static hashtable
 * registered_zend_ini_directives into a freshly allocated per-thread
 * hash table EG(ini_directives).
 *
 * 2a: zm_activate_newrelic is called for request initialization.
 *
 * 3: The modify handler is called* from a call to PHP ini_set(),
 *    assuming that PHP_INI_USER semantics are in force.
 *    By now, the ini_entry has a copy of the original value.
 *
 * 4: The modify handler is called* from php_request_shutdown calls*
 *    zend_ini_deactivate calls* zend_restore_ini_entry_cb
 */

/*
 * The names of the custom modify functions (aka update handlers) all
 * end with the sub string "_mh". These are called when a given value is
 * modified.
 *
 * The modify handlers must do sanity checks on the values they are given,
 * and do whatever translation from a string that is necessary.
 *
 * The modify handlers may disseminate their values as they see fit.
 * There are several common patterns for dissemination:
 *
 * Pattern A:
 *  The modify handler may call some agent- or axiom-specific function
 *  to receive the value immediately.  An example is the modifiy handler
 *  nr_logfile_mh which is called when the ini setting "newrelic.logfile" is
 *  changed. tHe handler disseminates its value immediately by calling the
 *  function nrl_set_log_file.
 *
 * Pattern B:
 *  The modify handler assigns to per-process globals through the macro
 *  NR_PHP_PROCESS_GLOBALS
 *
 * Pattern C:
 *  The modify handle assigns to per request globals using the macro NRPRG
 *  An example of this is the modify handler nr_tt_threshold_mh.
 *
 * Pattern D:
 *  The modify handler assign through a pointer to a data structure
 *  that PHP presents in arguments to the modify handler. Our use of
 *  the macro STD_PHP_INI_ENTRY_EX has arranged to have this pointer
 *  argument to the modify handler point to the like-named slot in the
 *  "newrelic_globals" variable, which is of type zend_newrelic_globals.
 *  This variable is either process global or thread global, depending
 *  on the zts semantics. Like pattern C, above, this variable is normally
 *  accessed through NRPRG.
 */

/*
 * The names of the handful of custom display handlers all
 * end with the sub string "_dh". These are called with the string value
 * of the ini setting, as maintained by PHP.
 */

/*
 * Here's one additional thing to note when reading or extending this code:
 *
 * The PHP INI parser will turn things like:
 *   option = no
 *   option = off
 *   option = false
 * into an empty(!) string.  It will turn things like:
 *   option = on
 *   option = yes
 *   option = true
 * into a string with the contents "1".
 * Thus any code which expects to take a boolean
 * argument must interpret an empty string (for new_value) as boolean false.
 */

#define NR_PHP_INI_DEFAULT_DAEMON_LOCATION "/usr/bin/newrelic-daemon"

#define NR_PHP_INI_DEFAULT_LOG_FILE "/var/log/newrelic/php_agent.log"

#define NR_PHP_INI_DEFAULT_LOG_LEVEL "info"

ZEND_DECLARE_MODULE_GLOBALS(newrelic)

typedef void (*foreach_fn_t)(const char* name, int namelen TSRMLS_DC);

static void foreach_list(const char* str, foreach_fn_t f_eachname TSRMLS_DC) {
  nrobj_t* rs;
  int ns = 0;
  int i;

  if ((0 == str) || (0 == str[0])) {
    return;
  }

  rs = nr_strsplit(str, ",", 0);
  ns = nro_getsize(rs);
  for (i = 0; i < ns; i++) {
    const char* s = nro_get_array_string(rs, i + 1, NULL);

    f_eachname(s, nr_strlen(s) TSRMLS_CC);
  }

  nro_delete(rs);
}

static nrtime_t nr_parse_time_from_config(const char* str) {
  return nr_parse_time(str);
}

static void check_ini_int_min_max(int* var, int minval, int maxval) {
  if (*var < minval) {
    *var = minval;
  } else if ((*var > maxval) && (maxval > 0)) {
    *var = maxval;
  }
}

/*
 * @brief error handling for strtoimax() functionality
 *
 * @param val_p   pointer to store the parsed value; remains undefined when
 *                function returns NR_FAILURE
 * @param str     string to parse
 * @param base    base to be used for number conversion
 * @return nr_status_t NR_SUCCESS || NR_FAILURE
 */
static nr_status_t nr_strtoi(int* val_p, const char* str, int base) {
  int val;
  char* endptr_p;
  errno = 0;

  val = strtoimax(str, &endptr_p, base);
  if (0 != errno || '\0' != *endptr_p) {
    return NR_FAILURE;
  }

  *val_p = val;
  return NR_SUCCESS;
}

#ifdef PHP7
#define PHP_INI_ENTRY_NAME(ie) (ie)->name->val
#define PHP_INI_ENTRY_NAME_LEN(ie) (ie)->name->len + 1
#define PHP_INI_ENTRY_ORIG_VALUE(ie) (ie)->orig_value->val
#define PHP_INI_ENTRY_ORIG_VALUE_LEN(ie) (ie)->orig_value->len
#define PHP_INI_ENTRY_VALUE(ie) (ie)->value->val
#define PHP_INI_ENTRY_VALUE_LEN(ie) (ie)->value->len
#else
#define PHP_INI_ENTRY_NAME(ie) (ie)->name
#define PHP_INI_ENTRY_NAME_LEN(ie) (ie)->name_length
#define PHP_INI_ENTRY_ORIG_VALUE(ie) (ie)->orig_value
#define PHP_INI_ENTRY_ORIG_VALUE_LEN(ie) (ie)->orig_value_length
#define PHP_INI_ENTRY_VALUE(ie) (ie)->value
#define PHP_INI_ENTRY_VALUE_LEN(ie) (ie)->value_length
#endif

/*
 * Next we declare some custom display functions for producing more neatly
 * formatted phpinfo() output. Most are pretty simple, a few slightly less so.
 */
static int nr_bool_val(php_ini_entry* ini_entry, int type) {
  const char* value = PHP_INI_ENTRY_VALUE(ini_entry);
  int ret;

  if ((PHP_INI_DISPLAY_ORIG == type) && (0 != ini_entry->modified)) {
    value = PHP_INI_ENTRY_ORIG_VALUE(ini_entry);
  }

  ret = nr_bool_from_str(value);
  return (1 == ret);
}

/*
 * This displayer produces the word "enabled" or "disabled".
 */
static PHP_INI_DISP(nr_enabled_disabled_dh) {
  int val = nr_bool_val(ini_entry, type);

  if (val) {
    php_printf("%s", "enabled");
  } else {
    php_printf("%s", "disabled");
  }
}

/*
 * This displayer produces the word "on" or "off".
 */
static PHP_INI_DISP(nr_on_off_dh) {
  int val = nr_bool_val(ini_entry, type);

  if (val) {
    php_printf("%s", "on");
  } else {
    php_printf("%s", "off");
  }
}

/*
 * This displayer produces the word "yes" or "no".
 */
static PHP_INI_DISP(nr_yes_no_dh) {
  int val = nr_bool_val(ini_entry, type);

  if (val) {
    php_printf("%s", "yes");
  } else {
    php_printf("%s", "no");
  }
}

static PHP_INI_DISP(nr_daemon_proxy_dh) {
  const char* value = PHP_INI_ENTRY_VALUE(ini_entry);
  char* printable_proxy = NULL;

  if ((PHP_INI_DISPLAY_ORIG == type) && (0 != ini_entry->modified)) {
    value = PHP_INI_ENTRY_ORIG_VALUE(ini_entry);
  }

  printable_proxy = nr_url_proxy_clean(value);

  if (printable_proxy) {
    php_printf("%s", printable_proxy);
  } else if (sapi_module.phpinfo_as_text) {
    php_printf("%s", "no value");
  } else {
    php_printf("<i>no value</i>");
  }

  nr_free(printable_proxy);
}

/*
 * This displayer is used to display the New Relic license. For obvious
 * reasons we do not want to display the full license. Therefore, we trim the
 * display of the license to include only the first and last few characters of
 * the license. We make a very weak attempt to ensure the license is valid,
 * solely by checking its length.
 */
static PHP_INI_DISP(nr_license_dh) {
  const char* value = PHP_INI_ENTRY_VALUE(ini_entry);
  char* printable_license = NULL;

  if ((PHP_INI_DISPLAY_ORIG == type) && (0 != ini_entry->modified)) {
    value = PHP_INI_ENTRY_ORIG_VALUE(ini_entry);
  }

  printable_license = nr_app_create_printable_license(value);

  if (printable_license) {
    php_printf("%s", printable_license);
  } else if (sapi_module.phpinfo_as_text) {
    php_printf("%s", "***INVALID FORMAT***");
  } else {
    php_printf("<b>%s</b>", "***INVALID FORMAT***");
  }

  nr_free(printable_license);
}

static PHP_INI_DISP(nr_framework_dh) {
  const char* value = PHP_INI_ENTRY_VALUE(ini_entry);

  if ((PHP_INI_DISPLAY_ORIG == type) && (0 != ini_entry->modified)) {
    value = PHP_INI_ENTRY_ORIG_VALUE(ini_entry);
  }

  if (value && value[0]) {
    php_printf("%s", value);
  } else {
    php_printf("%s", "auto-detect");
  }
}

/*
 * Now begin the modify handlers. Firstly, we shall define some compatibility
 * macros.
 */
#ifdef PHP7
#define NEW_VALUE new_value->val
#define NEW_VALUE_LEN new_value->len
#else
#define NEW_VALUE new_value
#define NEW_VALUE_LEN new_value_length
#endif /* PHP7 */

/*
 * On PHP 5, the arguments to the modify handlers are:
 *  zend_ini_entry *entry
 *  char *new_value
 *  uint new_value_length
 *  void *mh_arg1          the offset into the blob of data holding all ini
 *                         values
 *  void *mh_arg2          the blob of data holding all ini values
 *  void *mh_arg3          unused for our purposes int stage taken from the set
 *                         shown below
 *  TSRMLS_DC
 *
 * On PHP 7, the arguments are similar, but with PHP 7 appropriate changes for
 * the string and thread safety:
 *  zend_ini_entry *entry
 *  zend_string *new_value
 *  void *mh_arg1          the offset into the blob of data holding all ini
 *                         values
 *  void *mh_arg2          the blob of data holding all ini values
 *  void *mh_arg3          unused for our purposes int stage taken from the set
 *                         shown below
 *
 * The "stage" argument is a bitset formed from these symbols
 * (it is likely that the given set is a singleton):
 *   ZEND_INI_STAGE_STARTUP
 *   ZEND_INI_STAGE_SHUTDOWN
 *   ZEND_INI_STAGE_ACTIVATE
 *   ZEND_INI_STAGE_DEACTIVATE
 *   ZEND_INI_STAGE_RUNTIME
 *   ZEND_INI_STAGE_HTACCESS
 */

static PHP_INI_MH(nr_logfile_mh) {
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  if (0 != NEW_VALUE_LEN) {
    nrl_set_log_file(NEW_VALUE);
  } else {
    nrl_set_log_file(NR_PHP_INI_DEFAULT_LOG_FILE);
  }
  return SUCCESS;
}

static PHP_INI_MH(nr_daemon_auditlog_mh) {
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  nr_free(NR_PHP_PROCESS_GLOBALS(daemon_auditlog));

  if (0 != NEW_VALUE_LEN) {
    NR_PHP_PROCESS_GLOBALS(daemon_auditlog) = nr_strdup(NEW_VALUE);
  }
  return SUCCESS;
}

static PHP_INI_MH(nr_high_security_mh) {
  int val;

  (void)entry;
  (void)NEW_VALUE_LEN;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  val = nr_bool_from_str(NEW_VALUE);

  if (-1 == val) {
    return FAILURE;
  }

  if (val) {
    NR_PHP_PROCESS_GLOBALS(high_security) = 1;
  } else {
    NR_PHP_PROCESS_GLOBALS(high_security) = 0;
  }

  return SUCCESS;
}

static PHP_INI_MH(nr_preload_framework_library_detection_mh) {
  int val;

  (void)entry;
  (void)NEW_VALUE_LEN;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  val = nr_bool_from_str(NEW_VALUE);

  if (-1 == val) {
    return FAILURE;
  }

  NR_PHP_PROCESS_GLOBALS(preload_framework_library_detection) = val ? 1 : 0;

  return SUCCESS;
}

static PHP_INI_MH(nr_loglevel_mh) {
  nr_status_t rv;

  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  if (0 != NEW_VALUE_LEN) {
    rv = nrl_set_log_level(NEW_VALUE);
    if (NR_FAILURE == rv) {
      /*
       * There's a bit of a chicken and egg problem here.
       * A bogus loglevel will have the effect of "info",
       * so calling nrl_debug will successfully log the fault.
       */
      nrl_warning(NRL_INIT, "unknown loglevel \"%.8s\"; using \"info\" instead",
                  NEW_VALUE);
    }
  } else {
    rv = nrl_set_log_level(NR_PHP_INI_DEFAULT_LOG_LEVEL);
  }
  if (NR_SUCCESS == rv) {
    return SUCCESS;
  } else {
    return FAILURE;
  }
}

static PHP_INI_MH(nr_daemon_logfile_mh) {
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  nr_free(NR_PHP_PROCESS_GLOBALS(daemon_logfile));

  if (0 != NEW_VALUE_LEN) {
    NR_PHP_PROCESS_GLOBALS(daemon_logfile) = nr_strdup(NEW_VALUE);
  }
  return SUCCESS;
}

static PHP_INI_MH(nr_daemon_loglevel_mh) {
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  nr_free(NR_PHP_PROCESS_GLOBALS(daemon_loglevel));

  if (0 != NEW_VALUE_LEN) {
    NR_PHP_PROCESS_GLOBALS(daemon_loglevel) = nr_strdup(NEW_VALUE);
  }
  return SUCCESS;
}

static PHP_INI_MH(nr_daemon_port_mh) {
  const char* local_new_value = NULL;
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  nr_free(NR_PHP_PROCESS_GLOBALS(udspath));

  if (NEW_VALUE_LEN > 0) {
    local_new_value = NEW_VALUE;
  }

  NR_PHP_PROCESS_GLOBALS(udspath) = nr_strdup(local_new_value);
  return SUCCESS;
}

static PHP_INI_MH(nr_daemon_address_mh) {
  const char* local_new_value = NULL;
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  nr_free(NR_PHP_PROCESS_GLOBALS(address_path));

  if (NEW_VALUE_LEN > 0) {
    local_new_value = NEW_VALUE;
  }

  NR_PHP_PROCESS_GLOBALS(address_path) = nr_strdup(local_new_value);
  return SUCCESS;
}

static PHP_INI_MH(nr_daemon_ssl_cafile_mh) {
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  nr_free(NR_PHP_PROCESS_GLOBALS(ssl_cafile));

  if (NEW_VALUE_LEN > 0) {
    NR_PHP_PROCESS_GLOBALS(ssl_cafile) = nr_strdup(NEW_VALUE);
  }

  return SUCCESS;
}

static PHP_INI_MH(nr_daemon_ssl_capath_mh) {
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  nr_free(NR_PHP_PROCESS_GLOBALS(ssl_capath));

  if (NEW_VALUE_LEN > 0) {
    NR_PHP_PROCESS_GLOBALS(ssl_capath) = nr_strdup(NEW_VALUE);
  }

  return SUCCESS;
}

static PHP_INI_MH(nr_daemon_collector_host_mh) {
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  nr_free(NR_PHP_PROCESS_GLOBALS(collector));

  if (NEW_VALUE_LEN > 0) {
    NR_PHP_PROCESS_GLOBALS(collector) = nr_strdup(NEW_VALUE);
  }

  return SUCCESS;
}

static PHP_INI_MH(nr_daemon_proxy_mh) {
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  nr_free(NR_PHP_PROCESS_GLOBALS(proxy));

  if (NEW_VALUE_LEN > 0) {
    NR_PHP_PROCESS_GLOBALS(proxy) = nr_strdup(NEW_VALUE);
  }
  return SUCCESS;
}

static PHP_INI_MH(nr_daemon_location_mh) {
  const char* local_new_value = NULL;
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  nr_free(NR_PHP_PROCESS_GLOBALS(daemon));

  if (NEW_VALUE_LEN > 0) {
    local_new_value = NEW_VALUE;
  }

  NR_PHP_PROCESS_GLOBALS(daemon) = nr_strdup(local_new_value);
  return SUCCESS;
}

static PHP_INI_MH(nr_daemon_pidfile_mh) {
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  nr_free(NR_PHP_PROCESS_GLOBALS(pidfile));

  if (NEW_VALUE_LEN > 0) {
    NR_PHP_PROCESS_GLOBALS(pidfile) = nr_strdup(NEW_VALUE);
  }
  return SUCCESS;
}

static PHP_INI_MH(nr_daemon_app_timeout_mh) {
  const char* local_new_value = NULL;
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  nr_free(NR_PHP_PROCESS_GLOBALS(daemon_app_timeout));

  if (NEW_VALUE_LEN > 0) {
    local_new_value = NEW_VALUE;
  }

  NR_PHP_PROCESS_GLOBALS(daemon_app_timeout) = nr_strdup(local_new_value);
  return SUCCESS;
}

static PHP_INI_MH(nr_daemon_app_connect_timeout_mh) {
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  if (0 != NEW_VALUE_LEN) {
    NR_PHP_PROCESS_GLOBALS(daemon_app_connect_timeout)
        = nr_parse_time_from_config(NEW_VALUE);
  } else {
    NR_PHP_PROCESS_GLOBALS(daemon_app_connect_timeout) = 0;
  }

  return SUCCESS;
}

static PHP_INI_MH(nr_daemon_start_timeout_mh) {
  const char* local_new_value = NULL;
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  nr_free(NR_PHP_PROCESS_GLOBALS(daemon_start_timeout));

  if (NEW_VALUE_LEN > 0) {
    local_new_value = NEW_VALUE;
  }

  NR_PHP_PROCESS_GLOBALS(daemon_start_timeout) = nr_strdup(local_new_value);
  return SUCCESS;
}

static PHP_INI_MH(nr_daemon_dont_launch_mh) {
  int val;

  (void)entry;
  (void)NEW_VALUE_LEN;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  if (NEW_VALUE_LEN > 0) {
    val = (int)strtol(NEW_VALUE, 0, 10);
    if (val < 0) {
      val = 0;
    } else if (val > 3) {
      val = 3;
    }
    NR_PHP_PROCESS_GLOBALS(no_daemon_launch) = val;
  }
  return SUCCESS;
}

#define NR_PHP_UTILIZATION_MH_NAME(name) nr_daemon_utilization_##name##_mh

#define NR_PHP_UTILIZATION_MH(name)                     \
  static PHP_INI_MH(NR_PHP_UTILIZATION_MH_NAME(name)) { \
    int val;                                            \
                                                        \
    (void)entry;                                        \
    (void)NEW_VALUE_LEN;                                \
    (void)mh_arg1;                                      \
    (void)mh_arg2;                                      \
    (void)mh_arg3;                                      \
    (void)stage;                                        \
    NR_UNUSED_TSRMLS;                                   \
                                                        \
    val = nr_bool_from_str(NEW_VALUE);                  \
    if (-1 == val) {                                    \
      return FAILURE;                                   \
    }                                                   \
                                                        \
    NR_PHP_PROCESS_GLOBALS(utilization).name = val;     \
                                                        \
    return SUCCESS;                                     \
  }

NR_PHP_UTILIZATION_MH(aws)
NR_PHP_UTILIZATION_MH(azure)
NR_PHP_UTILIZATION_MH(gcp)
NR_PHP_UTILIZATION_MH(pcf)
NR_PHP_UTILIZATION_MH(docker)
NR_PHP_UTILIZATION_MH(kubernetes)

static PHP_INI_MH(nr_daemon_special_curl_verbose_mh) {
  int val;

  (void)entry;
  (void)NEW_VALUE_LEN;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  if (NEW_VALUE_LEN > 0) {
    val = (int)strtol(NEW_VALUE, 0, 10);
    NR_PHP_PROCESS_GLOBALS(daemon_special_curl_verbose) = val;
  }
  return SUCCESS;
}

static PHP_INI_MH(nr_daemon_special_integration_mh) {
  int val;

  (void)entry;
  (void)NEW_VALUE_LEN;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  if (NEW_VALUE_LEN > 0) {
    val = (int)strtol(NEW_VALUE, 0, 10);
    NR_PHP_PROCESS_GLOBALS(daemon_special_integration) = val;
  }
  return SUCCESS;
}

static void foreach_special_control_flag(const char* str,
                                         int str_len TSRMLS_DC) {
  NR_UNUSED_TSRMLS;

  if (str_len <= 0) {
    return;
  }

  if (0 == nr_strcmp(str, "no_sql_parsing")) {
    NR_PHP_PROCESS_GLOBALS(special_flags).no_sql_parsing = 1;
    return;
  }
  if (0 == nr_strcmp(str, "show_sql_parsing")) {
    NR_PHP_PROCESS_GLOBALS(special_flags).show_sql_parsing = 1;
    return;
  }
  if (0 == nr_strcmp(str, "enable_path_translated")) {
    NR_PHP_PROCESS_GLOBALS(special_flags).enable_path_translated = 1;
    return;
  }
  if (0 == nr_strcmp(str, "no_background_jobs")) {
    NR_PHP_PROCESS_GLOBALS(special_flags).no_background_jobs = 1;
    return;
  }
  if (0 == nr_strcmp(str, "show_executes")) {
    NR_PHP_PROCESS_GLOBALS(special_flags).show_executes = 1;
    return;
  }
  if (0 == nr_strcmp(str, "show_execute_params")) {
    NR_PHP_PROCESS_GLOBALS(special_flags).show_execute_params = 1;
    return;
  }
  if (0 == nr_strcmp(str, "show_execute_stack")) {
    NR_PHP_PROCESS_GLOBALS(special_flags).show_execute_stack = 1;
    return;
  }
  if (0 == nr_strcmp(str, "show_execute_returns")) {
    NR_PHP_PROCESS_GLOBALS(special_flags).show_execute_returns = 1;
    return;
  }
  if (0 == nr_strcmp(str, "show_executes_untrimmed")) {
    NR_PHP_PROCESS_GLOBALS(special_flags).show_executes_untrimmed = 1;
    return;
  }
  if (0 == nr_strcmp(str, "no_exception_handler")) {
    NR_PHP_PROCESS_GLOBALS(special_flags).no_exception_handler = 1;
    return;
  }
  if (0 == nr_strcmp(str, "no_signal_handler")) {
    NR_PHP_PROCESS_GLOBALS(special_flags).no_signal_handler = 1;
    return;
  }
  if (0 == nr_strcmp(str, "debug_autorum")) {
    NR_PHP_PROCESS_GLOBALS(special_flags).debug_autorum = 1;
    return;
  }
  if (0 == nr_strcmp(str, "show_loaded_files")) {
    NR_PHP_PROCESS_GLOBALS(special_flags).show_loaded_files = 1;
    return;
  }
  if (0 == nr_strcmp(str, "debug_cat")) {
    NR_PHP_PROCESS_GLOBALS(special_flags).debug_cat = 1;
    return;
  }
  if (0 == nr_strcmp(str, "disable_laravel_queue")) {
    NR_PHP_PROCESS_GLOBALS(special_flags).disable_laravel_queue = 1;
    return;
  }
}

static PHP_INI_MH(nr_special_mh) {
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;

  NR_PHP_PROCESS_GLOBALS(special_flags).no_sql_parsing = 0;
  NR_PHP_PROCESS_GLOBALS(special_flags).show_sql_parsing = 0;
  NR_PHP_PROCESS_GLOBALS(special_flags).enable_path_translated = 0;
  NR_PHP_PROCESS_GLOBALS(special_flags).no_background_jobs = 0;
  NR_PHP_PROCESS_GLOBALS(special_flags).show_executes = 0;
  NR_PHP_PROCESS_GLOBALS(special_flags).show_execute_params = 0;
  NR_PHP_PROCESS_GLOBALS(special_flags).show_execute_stack = 0;
  NR_PHP_PROCESS_GLOBALS(special_flags).show_execute_returns = 0;
  NR_PHP_PROCESS_GLOBALS(special_flags).show_executes_untrimmed = 0;
  NR_PHP_PROCESS_GLOBALS(special_flags).no_exception_handler = 0;
  NR_PHP_PROCESS_GLOBALS(special_flags).no_signal_handler = 0;
  NR_PHP_PROCESS_GLOBALS(special_flags).debug_autorum = 0;
  NR_PHP_PROCESS_GLOBALS(special_flags).show_loaded_files = 0;
  NR_PHP_PROCESS_GLOBALS(special_flags).debug_cat = 0;
  NR_PHP_PROCESS_GLOBALS(special_flags).disable_laravel_queue = 0;

  if (0 != NEW_VALUE_LEN) {
    foreach_list(NEW_VALUE, foreach_special_control_flag TSRMLS_CC);
  }

  return SUCCESS;
}

#define CHECK_FEATURE_FLAG(flag)                    \
  if (0 == nr_strcmp(str, #flag)) {                 \
    NR_PHP_PROCESS_GLOBALS(feature_flags).flag = 1; \
    return;                                         \
  }

static void foreach_feature_flag(const char* str NRUNUSED,
                                 int str_len TSRMLS_DC) {
  NR_UNUSED_TSRMLS;

  if (str_len <= 0) {
    return;
  }

  /*
   * Check each feature flag in turn. For example, for a feature flag called
   * "foo":
   *
   * CHECK_FEATURE_FLAG (foo);
   */
}

static PHP_INI_MH(nr_feature_flag_mh) {
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;

  /*
   * Set default feature flag values. For example, for a feature flag called
   * "foo":
   *
   * NR_PHP_PROCESS_GLOBALS (feature_flags).foo = 0;
   */

  if (0 != NEW_VALUE_LEN) {
    foreach_list(NEW_VALUE, foreach_feature_flag TSRMLS_CC);
  }

  return SUCCESS;
}

static PHP_INI_MH(nr_special_appinfo_timeout_mh) {
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  if (0 != NEW_VALUE_LEN) {
    nrtime_t us = nr_parse_time_from_config(NEW_VALUE);

    /*
     * 0 means "use the current/default value".
     */
    if (us > 0) {
      nr_cmd_appinfo_timeout_us = us;
    }
  }

  return SUCCESS;
}

static void foreach_disable_instrumentation(const char* str,
                                            int str_len TSRMLS_DC) {
  nrinternalfn_t* w;

  NR_UNUSED_TSRMLS;

  for (w = nr_wrapped_internal_functions; 0 != w; w = w->next) {
    if (0 == nr_strncmp(str, w->full_name, str_len)) {
      w->is_disabled = 1;
    }
  }
}

static PHP_INI_MH(nr_special_disable_instrumentation_mh) {
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;

  if (0 != NEW_VALUE_LEN) {
    foreach_list(NEW_VALUE, foreach_disable_instrumentation TSRMLS_CC);
  }

  return SUCCESS;
}

static PHP_INI_MH(nr_special_expensive_node_min_mh) {
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  if (0 != NEW_VALUE_LEN) {
    NR_PHP_PROCESS_GLOBALS(expensive_min)
        = nr_parse_time_from_config(NEW_VALUE);
  } else {
    NR_PHP_PROCESS_GLOBALS(expensive_min) = 2 * NR_TIME_DIVISOR_MS;
  }
  return SUCCESS;
}

static PHP_INI_MH(nr_special_enable_extension_instrumentation_mh) {
  int val = 0;

  (void)entry;
  (void)NEW_VALUE_LEN;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  val = nr_bool_from_str(NEW_VALUE);

  if (-1 == val) {
    return FAILURE;
  }

  NR_PHP_PROCESS_GLOBALS(instrument_extensions) = val ? 1 : 0;
  return SUCCESS;
}

static PHP_INI_MH(nr_enabled_mh) {
  nrinibool_t* p;
  int val = 0;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nrinibool_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  (void)NEW_VALUE_LEN;
  NR_UNUSED_TSRMLS;

  p->where = 0;

  val = nr_bool_from_str(NEW_VALUE);

  if (-1 == val) {
    return FAILURE;
  }

  if (PHP_INI_STAGE_STARTUP == stage) {
    /*
     * This behaviour is different depending on whether we are doing MINIT
     * or RINIT. In the MINIT case (this one) if we were disabled then this
     * is a global disabling of the entire agent.
     */
    NR_PHP_PROCESS_GLOBALS(enabled) = val;
  }
  p->value = (zend_bool)val;
  p->where = stage;

  return SUCCESS;
}

static PHP_INI_MH(nr_license_mh) {
  nrinistr_t* p;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nrinistr_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  NR_UNUSED_TSRMLS;

  p->where = 0;

  if (NR_LICENSE_SIZE == NEW_VALUE_LEN) {
    p->value = NEW_VALUE;
    p->where = stage;
    return SUCCESS;
  }

  return FAILURE;
}

static PHP_INI_MH(nr_string_mh) {
  nrinistr_t* p;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nrinistr_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  NR_UNUSED_TSRMLS;

  p->where = 0;

  if (NEW_VALUE_LEN > 0) {
    p->value = NEW_VALUE;
    p->where = stage;
    return SUCCESS;
  }

  return FAILURE;
}

static PHP_INI_MH(nr_boolean_mh) {
  nrinibool_t* p;
  int val = 0;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nrinibool_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  (void)NEW_VALUE_LEN;
  NR_UNUSED_TSRMLS;

  p->where = 0;

  val = nr_bool_from_str(NEW_VALUE);

  if (-1 == val) {
    return FAILURE;
  }

  p->value = (zend_bool)val;
  p->where = stage;

  return SUCCESS;
}

static PHP_INI_MH(nr_cat_enabled_mh) {
  nrinibool_t* p;
  int val = 0;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nrinibool_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  (void)NEW_VALUE_LEN;
  NR_UNUSED_TSRMLS;

  p->where = 0;

  val = nr_bool_from_str(NEW_VALUE);

  if (-1 == val) {
    return FAILURE;
  }

  if (0 != val) {
    nrl_warning(NRL_INIT,
                "Cross Application Training (CAT) has been enabled.  "
                "Note that CAT has been deprecated and will be removed "
                "in a future release.");
  }

  p->value = (zend_bool)val;
  p->where = stage;

  return SUCCESS;
}

static PHP_INI_MH(nr_tt_detail_mh) {
  nriniuint_t* p;
  int val = 1;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nriniuint_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  NR_UNUSED_TSRMLS;

  p->where = 0;

  if (0 == NEW_VALUE_LEN) {
    val = 0;
  } else {
    val = (int)strtol(NEW_VALUE, 0, 0);
    check_ini_int_min_max(&val, 0, 2);
  }

  p->value = (zend_uint)val;
  p->where = stage;

  return SUCCESS;
}

static PHP_INI_MH(nr_tt_max_segments_cli_mh) {
  nriniuint_t* p;
  int val = 1;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nriniuint_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  NR_UNUSED_TSRMLS;

  p->where = 0;

  if (0 != NEW_VALUE_LEN) {
    val = (int)strtol(NEW_VALUE, 0, 0);
    check_ini_int_min_max(&val, 0, INT_MAX);
    p->value = (zend_uint)val;
    p->where = stage;
  }

  return SUCCESS;
}

static PHP_INI_MH(nr_tt_max_segments_web_mh) {
  nriniuint_t* p;
  int val = 1;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nriniuint_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  NR_UNUSED_TSRMLS;

  p->where = 0;

  /*
   * Attempts to set the value with a 0-length string will do nothing.
   */
  if (0 != NEW_VALUE_LEN) {
    val = (int)strtol(NEW_VALUE, 0, 0);
    check_ini_int_min_max(&val, 0, INT_MAX);
    p->value = (zend_uint)val;
    p->where = stage;
  }

  return SUCCESS;
}

static PHP_INI_MH(nr_span_events_max_samples_stored_mh) {
  nriniuint_t* p;
  int val = NR_DEFAULT_SPAN_EVENTS_MAX_SAMPLES_STORED;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nriniuint_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  NR_UNUSED_TSRMLS;

  /* Anything other than a valid value will result in the
   * default value of NR_DEFAULT_SPAN_EVENTS_MAX_SAMPLES_STORED.
   */

  p->where = 0;

  if (0 != NEW_VALUE_LEN) {
    val = (int)strtol(NEW_VALUE, 0, 0);
    if ((0 >= val) || (NR_MAX_SPAN_EVENTS_MAX_SAMPLES_STORED < val)) {
      val = NR_DEFAULT_SPAN_EVENTS_MAX_SAMPLES_STORED;
      nrl_debug(NRL_INIT,
                "Invalid span_event.max_samples_stored value \"%.8s\"; using "
                "%d instead",
                NEW_VALUE, val);
    }
  }
  p->value = (zend_uint)val;
  p->where = stage;

  return SUCCESS;
}

static PHP_INI_MH(nr_span_queue_size_mh) {
  nriniuint_t* p;
  int val = 1;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nriniuint_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  NR_UNUSED_TSRMLS;

  p->where = 0;

  /*
   * This value cannot be lower than the span batch size (otherwise the
   * span queue couldn't even hold a single span batch).
   */
  if (0 != NEW_VALUE_LEN) {
    val = (int)strtol(NEW_VALUE, 0, 0);
    check_ini_int_min_max(&val, NR_MAX_8T_SPAN_BATCH_SIZE, INT_MAX);
    p->value = (zend_uint)val;
    p->where = stage;
  }

  return SUCCESS;
}

static PHP_INI_MH(nr_max_nesting_level_mh) {
  nriniuint_t* p;
  int val = 1;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nriniuint_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  NR_UNUSED_TSRMLS;

  p->where = 0;

  /*
   * Attempts to set the value with a 0-length string will do nothing.
   */
  if (0 != NEW_VALUE_LEN) {
    val = (int)strtol(NEW_VALUE, 0, 0);
    check_ini_int_min_max(
        &val, -1, 100000); /* some ludicrous high value, documented in
                              agent/scripts/newrelic.ini.private.template */
    p->value = (zend_uint)val;
    p->where = stage;
  }

  return SUCCESS;
}

static PHP_INI_MH(nr_tt_threshold_mh) {
  nrinitime_t* p;
  nrtime_t val;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nrinitime_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;

  if (0 == NEW_VALUE_LEN) {
    val = 0;
    NRPRG(tt_threshold_is_apdex_f) = 1;
  } else {
    if (0 == nr_strcmp(NEW_VALUE, "apdex_f")) {
      val = 0;
      NRPRG(tt_threshold_is_apdex_f) = 1;
    } else {
      val = nr_parse_time_from_config(NEW_VALUE);
    }
  }

  p->value = val;
  p->where = stage;

  return SUCCESS;
}

static PHP_INI_MH(nr_time_mh) {
  nrinitime_t* p;
  nrtime_t val;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nrinitime_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  NR_UNUSED_TSRMLS;

  if (0 == NEW_VALUE_LEN) {
    val = 0;
  } else {
    val = nr_parse_time_from_config(NEW_VALUE);
  }

  p->value = val;
  p->where = stage;

  return SUCCESS;
}

static PHP_INI_MH(nr_recordsql_mh) {
  nriniuint_t* p;
  int val;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nriniuint_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  NR_UNUSED_TSRMLS;

  if (0 == NEW_VALUE_LEN) {
    val = NR_PHP_RECORDSQL_OFF;
  } else {
    if (0 == nr_stricmp(NEW_VALUE, "off")) {
      val = NR_PHP_RECORDSQL_OFF;
    } else if (0 == nr_stricmp(NEW_VALUE, "raw")) {
      val = NR_PHP_RECORDSQL_RAW;
    } else if (0 == nr_stricmp(NEW_VALUE, "obfuscated")) {
      val = NR_PHP_RECORDSQL_OBFUSCATED;
    } else {
      p->where = 0;
      return FAILURE;
    }
  }

  p->value = (zend_uint)val;
  p->where = stage;

  return SUCCESS;
}

static PHP_INI_MH(nr_tt_internal_mh) {
  int val;

  (void)entry;
  (void)NEW_VALUE_LEN;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;
  (void)stage;
  NR_UNUSED_TSRMLS;

  val = nr_bool_from_str(NEW_VALUE);

  if (-1 == val) {
    return FAILURE;
  }

  if (val) {
    NR_PHP_PROCESS_GLOBALS(instrument_internal) = 1;
  } else {
    NR_PHP_PROCESS_GLOBALS(instrument_internal) = 0;
  }

  return SUCCESS;
}

static PHP_INI_MH(nr_framework_mh) {
  nrinifw_t* p;
  nrframework_t val = NR_FW_UNSET;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nrinifw_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  NR_UNUSED_TSRMLS;

  if (0 == NEW_VALUE_LEN) {
    val = NR_FW_UNSET;
    p->value = val;
    p->where = stage;
    return SUCCESS;
  } else {
    val = nr_php_framework_from_config(NEW_VALUE);
    if (NR_FW_UNSET != val) {
      p->value = val;
      p->where = stage;
      return SUCCESS;
    }
  }

  p->value = val;
  p->where = 0;
  return FAILURE;
}

static PHP_INI_MH(nr_wtfuncs_mh) {
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;

  if (NEW_VALUE_LEN > 0) {
    foreach_list(NEW_VALUE, nr_php_add_transaction_naming_function TSRMLS_CC);
  }

  NRPRG(wtfuncs_where) = stage;
  return SUCCESS;
}

static PHP_INI_MH(nr_ttcustom_mh) {
  (void)entry;
  (void)mh_arg1;
  (void)mh_arg2;
  (void)mh_arg3;

  if (0 != NEW_VALUE_LEN) {
    foreach_list(NEW_VALUE, nr_php_add_custom_tracer TSRMLS_CC);
  }

  NRPRG(ttcustom_where) = stage;
  return SUCCESS;
}

static PHP_INI_MH(nr_rum_loader_mh) {
  nrinistr_t* p;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nrinistr_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  NR_UNUSED_TSRMLS;

  p->where = 0;

  if (NEW_VALUE_LEN > 0) {
    p->value = NEW_VALUE;
    p->where = stage;
    return SUCCESS;
  }

  return FAILURE;
}

static PHP_INI_MH(nr_int_mh) {
  nriniint_t* p;
  long val = 0;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nriniint_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  NR_UNUSED_TSRMLS;

  p->where = 0;

  if (0 == NEW_VALUE_LEN) {
    val = 0;
  } else {
    val = strtol(NEW_VALUE, 0, 0);
    if (val > INT_MAX) {
      val = INT_MAX;
    } else if (val < INT_MIN) {
      val = INT_MIN;
    }
  }

  p->value = (int)val;
  p->where = stage;

  return SUCCESS;
}

static PHP_INI_MH(nr_unsigned_int_mh) {
  nriniuint_t* p;
  unsigned long val = 0;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nriniuint_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  NR_UNUSED_TSRMLS;

  p->where = 0;

  /*
   * For negative given values, fall back to the default of 0.
   * For values larger than UINT_MAX, use UINT_MAX.
   */
  if (0 == NEW_VALUE_LEN || NEW_VALUE[0] == '-') {
    val = 0;
  } else {
    val = strtoul(NEW_VALUE, 0, 0);
    if (val > UINT_MAX) {
      val = UINT_MAX;
    }
  }

  p->value = (zend_uint)val;
  p->where = stage;

  return SUCCESS;
}

static PHP_INI_MH(nr_log_events_max_samples_stored_mh) {
  nriniuint_t* p;
  int val = NR_DEFAULT_LOG_EVENTS_MAX_SAMPLES_STORED;
  bool err = false;
  nr_status_t parse_status = NR_SUCCESS;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nriniuint_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  NR_UNUSED_TSRMLS;

  /*
   * -- An invalid value will result in the default value.
   * -- A value < 0 will result in the default value
   * -- A value > MAX will result in MAX value
   */

  p->where = 0;

  if (0 != NEW_VALUE_LEN) {
    parse_status = nr_strtoi(&val, NEW_VALUE, 0);
    if (0 > val || NR_FAILURE == parse_status) {
      val = NR_DEFAULT_LOG_EVENTS_MAX_SAMPLES_STORED;
      err = true;
    } else if (NR_MAX_LOG_EVENTS_MAX_SAMPLES_STORED < val) {
      val = NR_MAX_LOG_EVENTS_MAX_SAMPLES_STORED;
      err = true;
    }
    if (err) {
      nrl_warning(NRL_INIT,
                  "Invalid application_logging.forwarding.max_samples_stored "
                  "value \"%.8s\"; using "
                  "%d instead",
                  NEW_VALUE, val);
    }
  }
  p->value = (zend_uint)val;
  p->where = stage;

  return SUCCESS;
}

static PHP_INI_MH(nr_log_forwarding_log_level_mh) {
  nriniuint_t* p;
  int log_level = LOG_LEVEL_DEFAULT;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nriniuint_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  NR_UNUSED_TSRMLS;

  p->where = 0;

  if (NEW_VALUE_LEN > 0) {
    nrl_debug(NRL_INIT, "Log Level (PSR-3): %s", NEW_VALUE);

    log_level = nr_log_level_str_to_int(NEW_VALUE);
    if (LOG_LEVEL_UNKNOWN == log_level) {
      log_level = LOG_LEVEL_DEFAULT;
      nrl_warning(NRL_INIT,
                  "Unknown log forwarding level %s, using %s instead.",
                  NEW_VALUE, nr_log_level_rfc_to_psr(log_level));
    }
    p->value = log_level;
    p->where = stage;

    nrl_debug(NRL_INIT, "Log Forwarding Log Level (RFC5424) set to: %d (%s)",
              p->value, nr_log_level_rfc_to_psr(p->value));
    return SUCCESS;
  }

  return FAILURE;
}

static PHP_INI_MH(nr_custom_events_max_samples_stored_mh) {
  nriniuint_t* p;
  int val = NR_DEFAULT_CUSTOM_EVENTS_MAX_SAMPLES_STORED;
  bool err = false;
  nr_status_t parse_status = NR_SUCCESS;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nriniuint_t*)(base + (size_t)mh_arg1);

  (void)entry;
  (void)mh_arg3;
  NR_UNUSED_TSRMLS;

  /*
   * -- An invalid value will result in the default value.
   * -- A value < 0 will result in the default value
   * -- A value > MAX will result in MAX value
   */

  p->where = 0;

  if (0 != NEW_VALUE_LEN) {
    parse_status = nr_strtoi(&val, NEW_VALUE, 0);
    if (0 > val || NR_FAILURE == parse_status) {
      val = NR_DEFAULT_CUSTOM_EVENTS_MAX_SAMPLES_STORED;
      err = true;
    } else if (NR_MAX_CUSTOM_EVENTS_MAX_SAMPLES_STORED < val) {
      val = NR_MAX_CUSTOM_EVENTS_MAX_SAMPLES_STORED;
      err = true;
    }
    if (err) {
      nrl_warning(NRL_INIT,
                  "Invalid custom_events.max_samples_stored "
                  "value \"%.8s\"; using "
                  "%d instead",
                  NEW_VALUE, val);
    }
  }
  p->value = (zend_uint)val;
  p->where = stage;

  return SUCCESS;
}

#define DEFAULT_WORDPRESS_HOOKS_OPTIONS "all_callbacks"
static PHP_INI_MH(nr_wordpress_hooks_options_mh) {
  nrinistr_t* p;

#ifndef ZTS
  char* base = (char*)mh_arg2;
#else
  char* base = (char*)ts_resource(*((int*)mh_arg2));
#endif

  p = (nrinistr_t*)(base + (size_t)mh_arg1);

  (void)mh_arg3;
  NR_UNUSED_TSRMLS;

  if (NEW_VALUE_LEN > 0) {
    p->value = NEW_VALUE;
    p->where = stage;
  }

  /* Default when value is all_callbacks, empty, or invalid */
  NRPRG(wordpress_plugins) = true;
  NRPRG(wordpress_core) = true;

  if (0 == nr_strcmp(NEW_VALUE, "plugin_callbacks")) {
    NRPRG(wordpress_plugins) = true;
    NRPRG(wordpress_core) = false;
  } else if (0 == nr_strcmp(NEW_VALUE, "threshold")) {
    NRPRG(wordpress_plugins) = false;
    NRPRG(wordpress_core) = false;
  } else if (NEW_VALUE_LEN > 0
             && 0 != nr_strcmp(NEW_VALUE, DEFAULT_WORDPRESS_HOOKS_OPTIONS)) {
    nrl_warning(NRL_INIT, "Invalid %s value \"%s\"; using \"%s\" instead.",
                ZEND_STRING_VALUE(entry->name), NEW_VALUE,
                DEFAULT_WORDPRESS_HOOKS_OPTIONS);
  }

  return SUCCESS;
}

/*
 * Now for the actual INI entry table. Please note there are two types of INI
 * entry specification used.
 *
 * The PHP_INI_ENTRY definitions are for values which are processed for
 * their side-effects only. That is, the value is not stored in the
 * globals structure and all the "work" for the INI value in question is
 * handled by the corresponding modification handler above.
 *
 * The STD_PHP_INI_ENTRY* definitions are for INI entries that are bound
 * to global variables inside the global variables structure defined in
 * php_newrelic.h.
 *
 * While these functions can have other side-effects if the handler
 * attached to them needs to, the primary purpose of the modification
 * handler is to set a parsed, validated version of the setting in the
 * global structure.
 *
 * Please always remember that the term "global structure" refers to the
 * ZEND_BEGIN_MODULE_GLOBALS() structure, which is set up per-request. Only
 * a few very special cases set actual real global variables; those
 * entries that are PHP_INI_SYSTEM entries.
 *
 * Please also note that PHP_INI_ALL (the set of PHP_INI_SYSTEM,
 * PHP_INI_REQUEST and PHP_INI_USER) is very explicitly NOT used.
 *
 * Here's why: The way we interact between PHP and Axiom, we get the
 * initial values during RINIT and populate the Axiom settings structure
 * based on those values.
 *
 * Almost all options are actually queried through Axiom and not directly
 * through PHP (because most of the "juicy bits" are implemented in Axiom and
 * are not specific to PHP). Therefore, allowing the user to use ini_set() gives
 * the false impression that doing so will have any effect. It won't.
 *
 * For those things that a user may be able to tweak at runtime, we
 * provide API calls that set both the PHP and Axiom view of things.
 *
 * Examples of such API calls:
 *   newrelic_set_appname
 */
#define NR_PHP_SYSTEM (PHP_INI_SYSTEM)
#define NR_PHP_REQUEST (PHP_INI_SYSTEM | PHP_INI_PERDIR)

#ifdef TAGS
static const zend_ini_entry ini_entries; /* ctags landing pad only */
#endif
PHP_INI_BEGIN() /* { */
/*
 * This first set are system settings. That is, they can only ever have the
 * default value or a value set in a master INI file. They can not be
 * changed on a per-directory basis, via .htaccess or via ini_set().
 *
 * Each of these has their own modify handler,
 * and is NOT wired through the PHP macros to any data structure
 *
 * The signature for PHP_INI_ENTRY_EX is:
 *   PHP_INI_ENTRY_EX(name, default_value, modifiable, on_modify, displayer)
 */
PHP_INI_ENTRY_EX("newrelic.logfile",
                 NR_PHP_INI_DEFAULT_LOG_FILE,
                 NR_PHP_SYSTEM,
                 nr_logfile_mh,
                 0)
PHP_INI_ENTRY_EX("newrelic.loglevel",
                 NR_PHP_INI_DEFAULT_LOG_LEVEL,
                 NR_PHP_SYSTEM,
                 nr_loglevel_mh,
                 0)

/*
 * High security mode is a system setting since it affects daemon spawn.
 */
PHP_INI_ENTRY_EX("newrelic.high_security",
                 "0",
                 NR_PHP_SYSTEM,
                 nr_high_security_mh,
                 0)

/*
 * Feature flag handling.
 */
PHP_INI_ENTRY_EX("newrelic.feature_flag",
                 "",
                 NR_PHP_SYSTEM,
                 nr_feature_flag_mh,
                 0)

/*
 * Enables framework and library detection when preloading (added in PHP 7.4) is
 * enabled.
 */
PHP_INI_ENTRY_EX("newrelic.preload_framework_library_detection",
                 "1",
                 NR_PHP_SYSTEM,
                 nr_preload_framework_library_detection_mh,
                 0)

/*
 * Daemon
 */
PHP_INI_ENTRY_EX("newrelic.daemon.auditlog",
                 "",
                 NR_PHP_SYSTEM,
                 nr_daemon_auditlog_mh,
                 0)
PHP_INI_ENTRY_EX("newrelic.daemon.logfile",
                 "",
                 NR_PHP_SYSTEM,
                 nr_daemon_logfile_mh,
                 0)
PHP_INI_ENTRY_EX("newrelic.daemon.loglevel",
                 "",
                 NR_PHP_SYSTEM,
                 nr_daemon_loglevel_mh,
                 0)
PHP_INI_ENTRY_EX(
    "newrelic.daemon.port",
    NR_PHP_INI_DEFAULT_PORT, /* port and address share the same default */
    NR_PHP_SYSTEM,
    nr_daemon_port_mh,
    0)
PHP_INI_ENTRY_EX(
    "newrelic.daemon.address",
    NR_PHP_INI_DEFAULT_PORT, /* port and address share the same default */
    NR_PHP_SYSTEM,
    nr_daemon_address_mh,
    0)
PHP_INI_ENTRY_EX("newrelic.daemon.ssl_ca_bundle",
                 "",
                 NR_PHP_SYSTEM,
                 nr_daemon_ssl_cafile_mh,
                 0)
PHP_INI_ENTRY_EX("newrelic.daemon.ssl_ca_path",
                 "",
                 NR_PHP_SYSTEM,
                 nr_daemon_ssl_capath_mh,
                 0)
PHP_INI_ENTRY_EX("newrelic.daemon.collector_host",
                 "",
                 NR_PHP_SYSTEM,
                 nr_daemon_collector_host_mh,
                 0)
PHP_INI_ENTRY_EX("newrelic.daemon.proxy",
                 "",
                 NR_PHP_SYSTEM,
                 nr_daemon_proxy_mh,
                 nr_daemon_proxy_dh)
PHP_INI_ENTRY_EX("newrelic.daemon.location",
                 NR_PHP_INI_DEFAULT_DAEMON_LOCATION,
                 NR_PHP_SYSTEM,
                 nr_daemon_location_mh,
                 0)
PHP_INI_ENTRY_EX("newrelic.daemon.pidfile",
                 "",
                 NR_PHP_SYSTEM,
                 nr_daemon_pidfile_mh,
                 0)
PHP_INI_ENTRY_EX("newrelic.daemon.dont_launch",
                 "",
                 NR_PHP_SYSTEM,
                 nr_daemon_dont_launch_mh,
                 0)
PHP_INI_ENTRY_EX("newrelic.daemon.app_timeout",
                 "",
                 NR_PHP_SYSTEM,
                 nr_daemon_app_timeout_mh,
                 0)
PHP_INI_ENTRY_EX("newrelic.daemon.app_connect_timeout",
                 "",
                 NR_PHP_SYSTEM,
                 nr_daemon_app_connect_timeout_mh,
                 0)
PHP_INI_ENTRY_EX("newrelic.daemon.start_timeout",
                 "",
                 NR_PHP_SYSTEM,
                 nr_daemon_start_timeout_mh,
                 0)

/*
 * Utilization
 */
PHP_INI_ENTRY_EX("newrelic.daemon.utilization.detect_aws",
                 "1",
                 NR_PHP_SYSTEM,
                 NR_PHP_UTILIZATION_MH_NAME(aws),
                 nr_enabled_disabled_dh)
PHP_INI_ENTRY_EX("newrelic.daemon.utilization.detect_azure",
                 "1",
                 NR_PHP_SYSTEM,
                 NR_PHP_UTILIZATION_MH_NAME(azure),
                 nr_enabled_disabled_dh)
PHP_INI_ENTRY_EX("newrelic.daemon.utilization.detect_gcp",
                 "1",
                 NR_PHP_SYSTEM,
                 NR_PHP_UTILIZATION_MH_NAME(gcp),
                 nr_enabled_disabled_dh)
PHP_INI_ENTRY_EX("newrelic.daemon.utilization.detect_pcf",
                 "1",
                 NR_PHP_SYSTEM,
                 NR_PHP_UTILIZATION_MH_NAME(pcf),
                 nr_enabled_disabled_dh)
PHP_INI_ENTRY_EX("newrelic.daemon.utilization.detect_docker",
                 "1",
                 NR_PHP_SYSTEM,
                 NR_PHP_UTILIZATION_MH_NAME(docker),
                 nr_enabled_disabled_dh)
PHP_INI_ENTRY_EX("newrelic.daemon.utilization.detect_kubernetes",
                 "1",
                 NR_PHP_SYSTEM,
                 NR_PHP_UTILIZATION_MH_NAME(kubernetes),
                 nr_enabled_disabled_dh)
/*
 * This daemon flag is for internal development use only.  It should not be
 * documented to customers.
 */
PHP_INI_ENTRY_EX("newrelic.daemon.special.integration",
                 "",
                 NR_PHP_SYSTEM,
                 nr_daemon_special_integration_mh,
                 0)

/*
 * These entries are NOT documented anywhere, but primarily used for
 * development or debugging.
 *
 * The defaults for these settings _must_ be "", otherwise phpinfo() will
 * show them. This behaviour cannot be disabled by a display handler.
 */
PHP_INI_ENTRY_EX("newrelic.special", "", NR_PHP_SYSTEM, nr_special_mh, 0)
PHP_INI_ENTRY_EX("newrelic.special.appinfo_timeout",
                 "",
                 NR_PHP_SYSTEM,
                 nr_special_appinfo_timeout_mh,
                 0)
PHP_INI_ENTRY_EX("newrelic.special.disable_instrumentation",
                 "",
                 NR_PHP_SYSTEM,
                 nr_special_disable_instrumentation_mh,
                 0)
PHP_INI_ENTRY_EX("newrelic.special.expensive_node_min",
                 "",
                 NR_PHP_SYSTEM,
                 nr_special_expensive_node_min_mh,
                 0)
PHP_INI_ENTRY_EX("newrelic.special.enable_extension_instrumentation",
                 "",
                 NR_PHP_SYSTEM,
                 nr_special_enable_extension_instrumentation_mh,
                 0)
PHP_INI_ENTRY_EX("newrelic.daemon.special.curl_verbose",
                 "",
                 NR_PHP_SYSTEM,
                 nr_daemon_special_curl_verbose_mh,
                 0)

/*
 * The remaining entries are all per-directory settable, or settable via
 * scripts. Unlike the global entries above, these should only ever set
 * variables in the per-request globals. There are a few cases, such as the
 * "newrelic.enabled" setting, that have special meaning at the global
 * scope. These are well documented in the corresponding modify handler
 * functions.
 *
 * STD_ZEND_INI_ENTRY_EX(name, default_value, modifiable, on_modify,
 * property_name, struct_type, struct_ptr, displayer)
 */
STD_PHP_INI_ENTRY_EX("newrelic.enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_enabled_mh,
                     enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_yes_no_dh)
STD_PHP_INI_ENTRY_EX("newrelic.license",
                     "",
                     NR_PHP_REQUEST,
                     nr_license_mh,
                     license,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_license_dh)
STD_PHP_INI_ENTRY_EX("newrelic.appname",
                     NR_PHP_APP_NAME_DEFAULT,
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     appnames,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.webtransaction.name.remove_trailing_path",
                     "0",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     remove_trailing_path,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_yes_no_dh)
STD_PHP_INI_ENTRY_EX("newrelic.framework.drupal.modules",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     drupal_modules,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_on_off_dh)
STD_PHP_INI_ENTRY_EX("newrelic.framework.wordpress.hooks",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     wordpress_hooks,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_on_off_dh)
STD_PHP_INI_ENTRY_EX("newrelic.framework.wordpress.hooks.options",
                     DEFAULT_WORDPRESS_HOOKS_OPTIONS,
                     NR_PHP_REQUEST,
                     nr_wordpress_hooks_options_mh,
                     wordpress_hooks_options,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.framework.wordpress.hooks.threshold",
                     "1ms",
                     NR_PHP_REQUEST,
                     nr_time_mh,
                     wordpress_hooks_threshold,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.framework.wordpress.hooks_skip_filename",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     wordpress_hooks_skip_filename,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.framework",
                     "",
                     NR_PHP_REQUEST,
                     nr_framework_mh,
                     force_framework,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_framework_dh)

/* DEPRECATED */
STD_PHP_INI_ENTRY_EX("newrelic.cross_application_tracer.enabled",
                     "0",
                     NR_PHP_REQUEST,
                     nr_cat_enabled_mh,
                     cross_process_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.special.max_nesting_level",
                     "-1",
                     NR_PHP_REQUEST,
                     nr_max_nesting_level_mh,
                     max_nesting_level,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.labels",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     labels,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.process_host.display_name",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     process_host_display_name,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.webtransaction.name.files",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     file_name_list,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.guzzle.enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     guzzle_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)

/*
 * Attributes
 */
STD_PHP_INI_ENTRY_EX("newrelic.attributes.enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     attributes.enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.attributes.include",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     attributes.include,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.attributes.exclude",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     attributes.exclude,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
/* DEPRECATED */
STD_PHP_INI_ENTRY_EX("newrelic.capture_params",
                     "0",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     capture_params,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_on_off_dh)
/* DEPRECATED */
STD_PHP_INI_ENTRY_EX("newrelic.ignored_params",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     ignored_params,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)

/*
 * Transaction Tracer
 */
/* DEPRECATED */
STD_PHP_INI_ENTRY_EX("newrelic.transaction_tracer.capture_attributes",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     transaction_tracer_capture_attributes,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.transaction_tracer.attributes.enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     transaction_tracer_attributes.enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.transaction_tracer.attributes.include",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     transaction_tracer_attributes.include,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.transaction_tracer.attributes.exclude",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     transaction_tracer_attributes.exclude,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.transaction_tracer.enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     tt_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.transaction_tracer.explain_enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     ep_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.transaction_tracer.detail",
                     "1",
                     NR_PHP_REQUEST,
                     nr_tt_detail_mh,
                     tt_detail,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.transaction_tracer.max_segments_cli",
                     "100000",
                     NR_PHP_REQUEST,
                     nr_tt_max_segments_cli_mh,
                     tt_max_segments_cli,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.transaction_tracer.max_segments_web",
                     "0",
                     NR_PHP_REQUEST,
                     nr_tt_max_segments_web_mh,
                     tt_max_segments_web,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.transaction_tracer.slow_sql",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     tt_slowsql,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.transaction_tracer.threshold",
                     "apdex_f",
                     NR_PHP_REQUEST,
                     nr_tt_threshold_mh,
                     tt_threshold,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX(
    "newrelic.transaction_tracer."
    "explain_threshold",
    "500",
    NR_PHP_REQUEST,
    nr_time_mh,
    ep_threshold,
    zend_newrelic_globals,
    newrelic_globals,
    0)
STD_PHP_INI_ENTRY_EX(
    "newrelic.transaction_tracer."
    "stack_trace_threshold",
    "500",
    NR_PHP_REQUEST,
    nr_time_mh,
    ss_threshold,
    zend_newrelic_globals,
    newrelic_globals,
    0)
STD_PHP_INI_ENTRY_EX(
    "newrelic.transaction_"
    "tracer.record_sql",
    "obfuscated",
    NR_PHP_REQUEST,
    nr_recordsql_mh,
    tt_recordsql,
    zend_newrelic_globals,
    newrelic_globals,
    0)
STD_PHP_INI_ENTRY_EX(
    "newrelic.transaction_"
    "tracer.gather_input_"
    "queries",
    "1",
    NR_PHP_REQUEST,
    nr_boolean_mh,
    tt_inputquery,
    zend_newrelic_globals,
    newrelic_globals,
    0)
PHP_INI_ENTRY_EX(
    "newrelic."
    "transaction_"
    "tracer.internal_"
    "functions_enabled",
    "0",
    NR_PHP_SYSTEM,
    nr_tt_internal_mh,
    nr_enabled_disabled_dh)

/*
 * Error Collector
 */
STD_PHP_INI_ENTRY_EX("newrelic.error_collector.enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     errors_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.error_collector.ignore_user_exception_handler",
                     "0",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     ignore_user_exception_handler,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_yes_no_dh)
STD_PHP_INI_ENTRY_EX("newrelic.error_collector.ignore_errors",
                     "",
                     NR_PHP_REQUEST,
                     nr_int_mh,
                     ignore_errors,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.error_collector.ignore_exceptions",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     ignore_exceptions,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.error_collector.record_database_errors",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     record_database_errors,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_yes_no_dh)
STD_PHP_INI_ENTRY_EX("newrelic.error_collector.prioritize_api_errors",
                     "0",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     prioritize_api_errors,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_yes_no_dh)
/* DEPRECATED */
STD_PHP_INI_ENTRY_EX("newrelic.error_collector.capture_attributes",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     error_collector_capture_attributes,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.error_collector.attributes.enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     error_collector_attributes.enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.error_collector.attributes.include",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     error_collector_attributes.include,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.error_collector.attributes.exclude",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     error_collector_attributes.exclude,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)

/*
 * Transaction Events
 */
/* DEPRECATED */
STD_PHP_INI_ENTRY_EX("newrelic.analytics_events.enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     analytics_events_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
/* DEPRECATED */
STD_PHP_INI_ENTRY_EX("newrelic.analytics_events.capture_attributes",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     analytics_events_capture_attributes,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.transaction_events.enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     transaction_events_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.error_collector.capture_events",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     error_events_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.transaction_events.attributes.enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     transaction_events_attributes.enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.transaction_events.attributes.include",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     transaction_events_attributes.include,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.transaction_events.attributes.exclude",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     transaction_events_attributes.exclude,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)

/*
 * Custom Events
 */
STD_PHP_INI_ENTRY_EX("newrelic.custom_insights_events.enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     custom_events_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.custom_events.max_samples_stored",
                     NR_STR2(NR_DEFAULT_CUSTOM_EVENTS_MAX_SAMPLES_STORED),
                     NR_PHP_REQUEST,
                     nr_custom_events_max_samples_stored_mh,
                     custom_events_max_samples_stored,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)

/*
 * Synthetics
 */
STD_PHP_INI_ENTRY_EX("newrelic.synthetics.enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     synthetics_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)

/*
 * Datastore Tracer
 */
STD_PHP_INI_ENTRY_EX("newrelic.datastore_tracer.instance_reporting.enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     instance_reporting_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX(
    "newrelic.datastore_tracer.database_name_reporting.enabled",
    "1",
    NR_PHP_REQUEST,
    nr_boolean_mh,
    database_name_reporting_enabled,
    zend_newrelic_globals,
    newrelic_globals,
    nr_enabled_disabled_dh)

/*
 * Library Support
 */
STD_PHP_INI_ENTRY_EX("newrelic.phpunit_events.enabled",
                     "0",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     phpunit_events_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)

/*
 * Browser Monitoring
 */
STD_PHP_INI_ENTRY_EX("newrelic.browser_monitoring.auto_instrument",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     browser_monitoring_auto_instrument,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.browser_monitoring.debug",
                     "0",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     browser_monitoring_debug,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.browser_monitoring.loader",
                     "rum",
                     NR_PHP_REQUEST,
                     nr_rum_loader_mh,
                     browser_monitoring_loader,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
/* DEPRECATED */
STD_PHP_INI_ENTRY_EX("newrelic.browser_monitoring.capture_attributes",
                     "0",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     browser_monitoring_capture_attributes,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.browser_monitoring.attributes.enabled",
                     "0",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     browser_monitoring_attributes.enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.browser_monitoring.attributes.include",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     browser_monitoring_attributes.include,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.browser_monitoring.attributes.exclude",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     browser_monitoring_attributes.exclude,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
/*
 * newrelic.browser_monitoring.ssl_for_http is omitted.
 */

/*
 * These use PHP_INI_ENTRY_EX because they do not directly set any request
 * variables, but instead are processed purely for side-effects.
 * Each has its own modify handler.
 */
PHP_INI_ENTRY_EX("newrelic.webtransaction.name.functions",
                 "",
                 NR_PHP_REQUEST,
                 nr_wtfuncs_mh,
                 0)
PHP_INI_ENTRY_EX("newrelic.transaction_tracer.custom",
                 "",
                 NR_PHP_REQUEST,
                 nr_ttcustom_mh,
                 0)

STD_PHP_INI_ENTRY_EX("newrelic.security_policies_token",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     security_policies_token,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)

/*
 * Private ini value to control whether we replace error
 * message with high security message
 */
STD_PHP_INI_ENTRY_EX("newrelic.allow_raw_exception_messages",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     allow_raw_exception_messages,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)

/*
 * Private ini value to control whether we allow users to send
 * custom parameters. We are introducing this ini value to give
 * new LASP security policies the ability to change this behavior.
 * Regular end users are still expected to use the attributes.include
 * configuration values.
 */
STD_PHP_INI_ENTRY_EX("newrelic.custom_parameters_enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     custom_parameters_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)

/**
 * Flag to turn the distributed tracing functionality on/off
 * When on, the agent will add the new distributed tracing intrinsics
 * to outgoing data and allow users to call the new distributed tracing API
 * functions
 */
STD_PHP_INI_ENTRY_EX("newrelic.distributed_tracing_enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     distributed_tracing_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)

/**
 * Flag to omit newrelic headers from distributed tracing outbound headers.
 * When this flag and newrelic.distributed_tracing_enabled are both on,
 * newrelic distributed tracing headers will not be added to the outbound
 * request. The agent will still add W3C trace context headers. When off,
 * both of the aforementioned header categories will be present in the
 * outbound headers if distributed tracing is enabled.
 */
STD_PHP_INI_ENTRY_EX("newrelic.distributed_tracing_exclude_newrelic_header",
                     "0",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     distributed_tracing_exclude_newrelic_header,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)

/**
 * Flag to turn the span events on/off
 * When on, the agent will create span events. Span events require that
 * distributed tracing is enabled.
 */
STD_PHP_INI_ENTRY_EX("newrelic.span_events_enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     span_events_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)

STD_PHP_INI_ENTRY_EX("newrelic.span_events.max_samples_stored",
                     NR_STR2(NR_DEFAULT_SPAN_EVENTS_MAX_SAMPLES_STORED),
                     NR_PHP_REQUEST,
                     nr_span_events_max_samples_stored_mh,
                     span_events_max_samples_stored,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)

STD_PHP_INI_ENTRY_EX("newrelic.span_events.attributes.enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     span_events_attributes.enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.span_events.attributes.include",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     span_events_attributes.include,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.span_events.attributes.exclude",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     span_events_attributes.exclude,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)

/**
 * Infinite tracing flags.
 */
STD_PHP_INI_ENTRY_EX("newrelic.infinite_tracing.trace_observer.host",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     trace_observer_host,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)

STD_PHP_INI_ENTRY_EX("newrelic.infinite_tracing.trace_observer.port",
                     "443",
                     NR_PHP_REQUEST,
                     nr_unsigned_int_mh,
                     trace_observer_port,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)

STD_PHP_INI_ENTRY_EX("newrelic.infinite_tracing.span_events.queue_size",
                     "100000",
                     NR_PHP_REQUEST,
                     nr_span_queue_size_mh,
                     span_queue_size,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)

STD_PHP_INI_ENTRY_EX("newrelic.infinite_tracing.span_events.agent_queue.size",
                     "1000",
                     NR_PHP_REQUEST,
                     nr_unsigned_int_mh,
                     agent_span_queue_size,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)

STD_PHP_INI_ENTRY_EX(
    "newrelic.infinite_tracing.span_events.agent_queue.timeout",
    "1s",
    NR_PHP_REQUEST,
    nr_time_mh,
    agent_span_queue_timeout,
    zend_newrelic_globals,
    newrelic_globals,
    0)

/*
 * Code Level Metrics
 */
STD_PHP_INI_ENTRY_EX("newrelic.code_level_metrics.enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     code_level_metrics_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
/*
 * Logging
 */
STD_PHP_INI_ENTRY_EX("newrelic.application_logging.enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     logging_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.application_logging.local_decorating.enabled",
                     "0",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     log_decorating_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.application_logging.forwarding.enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     log_forwarding_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX(
    "newrelic.application_logging.forwarding.max_samples_stored",
    NR_STR2(NR_DEFAULT_LOG_EVENTS_MAX_SAMPLES_STORED),
    NR_PHP_REQUEST,
    nr_log_events_max_samples_stored_mh,
    log_events_max_samples_stored,
    zend_newrelic_globals,
    newrelic_globals,
    0)
STD_PHP_INI_ENTRY_EX("newrelic.application_logging.forwarding.log_level",
                     "WARNING",
                     NR_PHP_REQUEST,
                     nr_log_forwarding_log_level_mh,
                     log_forwarding_log_level,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.application_logging.metrics.enabled",
                     "1",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     log_metrics_enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.application_logging.forwarding.context_data.enabled",
                     "0",
                     NR_PHP_REQUEST,
                     nr_boolean_mh,
                     log_context_data_attributes.enabled,
                     zend_newrelic_globals,
                     newrelic_globals,
                     nr_enabled_disabled_dh)
STD_PHP_INI_ENTRY_EX("newrelic.application_logging.forwarding.context_data.include",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     log_context_data_attributes.include,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)
STD_PHP_INI_ENTRY_EX("newrelic.application_logging.forwarding.context_data.exclude",
                     "",
                     NR_PHP_REQUEST,
                     nr_string_mh,
                     log_context_data_attributes.exclude,
                     zend_newrelic_globals,
                     newrelic_globals,
                     0)

PHP_INI_END() /* } */

void nr_php_register_ini_entries(int module_number TSRMLS_DC) {
#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO
  int type = MODULE_PERSISTENT;
#endif
  REGISTER_INI_ENTRIES();
}

void nr_php_unregister_ini_entries(int module_number TSRMLS_DC) {
#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO
  int type = MODULE_PERSISTENT;
#endif
  UNREGISTER_INI_ENTRIES();
}

static void nr_ini_displayer_cb(zend_ini_entry* ini_entry, int type TSRMLS_DC) {
  const char* display_string = NULL;
  nr_string_len_t display_string_length = 0;
  int esc_html = 0;

  if (ini_entry->displayer) {
    ini_entry->displayer(ini_entry, type);
    return;
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
  if ((ZEND_INI_DISPLAY_ORIG == type) && ini_entry->modified
      && PHP_INI_ENTRY_ORIG_VALUE(ini_entry)
      && PHP_INI_ENTRY_ORIG_VALUE_LEN(ini_entry)) {
    display_string = PHP_INI_ENTRY_ORIG_VALUE(ini_entry);
    display_string_length = PHP_INI_ENTRY_ORIG_VALUE_LEN(ini_entry);
    esc_html = sapi_module.phpinfo_as_text ? 0 : 1;
#pragma GCC diagnostic pop
  } else if (PHP_INI_ENTRY_VALUE(ini_entry)
             && PHP_INI_ENTRY_VALUE_LEN(ini_entry)) {
    display_string = PHP_INI_ENTRY_VALUE(ini_entry);
    display_string_length = PHP_INI_ENTRY_VALUE_LEN(ini_entry);
    esc_html = sapi_module.phpinfo_as_text ? 0 : 1;
  } else if (0 == sapi_module.phpinfo_as_text) {
    display_string = "<i>no value</i>";
    display_string_length = sizeof("<i>no value</i>") - 1;
  } else {
    display_string = "no value";
    display_string_length = sizeof("no value") - 1;
  }

  if (esc_html) {
    php_html_puts(display_string, display_string_length TSRMLS_CC);
  } else {
    PHPWRITE(display_string, display_string_length);
  }
}

/*
 * Print out the value of a global ini setting.
 *
 * This function must be compatible with
 *   typedef int (*apply_func_arg_t)(void *pDest, void *argument TSRMLS_DC);
 *
 * This function is called for every ini setting, even those in other
 * modules. We have to filter out only things for our module.
 */
static int nr_ini_displayer_global(zend_ini_entry* ini_entry,
                                   int* module_number,
                                   zend_hash_key* key NRUNUSED TSRMLS_DC) {
  if (ini_entry->module_number != *module_number) {
    return ZEND_HASH_APPLY_KEEP;
  }

  if (ini_entry->modifiable & PHP_INI_PERDIR) {
    return ZEND_HASH_APPLY_KEEP;
  }

  /*
   * If there is no value, then don't print anything for the "special" ini
   * settings.
   */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
  if (NULL == PHP_INI_ENTRY_VALUE(ini_entry)
      || 0 == PHP_INI_ENTRY_VALUE_LEN(ini_entry)) {
#pragma GCC diagnostic pop
    if (0
        == nr_strncmp(PHP_INI_ENTRY_NAME(ini_entry),
                      NR_PSTR("newrelic.special"))) {
      return ZEND_HASH_APPLY_KEEP;
    }
    if (0
        == nr_strncmp(PHP_INI_ENTRY_NAME(ini_entry),
                      NR_PSTR("newrelic.daemon.special"))) {
      return ZEND_HASH_APPLY_KEEP;
    }
  }

  if (!sapi_module.phpinfo_as_text) {
    PUTS("<tr>");
    PUTS("<td class=\"e\">");
    PHPWRITE(PHP_INI_ENTRY_NAME(ini_entry),
             PHP_INI_ENTRY_NAME_LEN(ini_entry) - 1);
    PUTS("</td><td class=\"v\">");
    nr_ini_displayer_cb(ini_entry, ZEND_INI_DISPLAY_ACTIVE TSRMLS_CC);
    PUTS("</td></tr>\n");
  } else {
    PHPWRITE(PHP_INI_ENTRY_NAME(ini_entry),
             PHP_INI_ENTRY_NAME_LEN(ini_entry) - 1);
    PUTS(" => ");
    nr_ini_displayer_cb(ini_entry, ZEND_INI_DISPLAY_ACTIVE TSRMLS_CC);
    PUTS("\n");
  }
  return ZEND_HASH_APPLY_KEEP;
}

/*
 * Print out the value of a per directory ini setting.
 *
 * This function must be compatible with
 *   typedef int (*apply_func_arg_t)(void *pDest, void *argument TSRMLS_DC);
 *
 * This function is called for every ini setting, even those in other
 * modules. We have to filter out only things for our module.
 */
static int nr_ini_displayer_perdir(zend_ini_entry* ini_entry,
                                   int* module_number,
                                   zend_hash_key* key NRUNUSED TSRMLS_DC) {
  if (ini_entry->module_number != *module_number) {
    return ZEND_HASH_APPLY_KEEP;
  }

  if (!(ini_entry->modifiable & PHP_INI_PERDIR)) {
    return ZEND_HASH_APPLY_KEEP;
  }

  if (!sapi_module.phpinfo_as_text) {
    PUTS("<tr>");
    PUTS("<td class=\"e\">");
    PHPWRITE(PHP_INI_ENTRY_NAME(ini_entry),
             PHP_INI_ENTRY_NAME_LEN(ini_entry) - 1);
    PUTS("</td><td class=\"v\">");
    nr_ini_displayer_cb(ini_entry, ZEND_INI_DISPLAY_ACTIVE TSRMLS_CC);
    PUTS("</td><td class=\"v\">");
    nr_ini_displayer_cb(ini_entry, ZEND_INI_DISPLAY_ORIG TSRMLS_CC);
    PUTS("</td></tr>\n");
  } else {
    PHPWRITE(PHP_INI_ENTRY_NAME(ini_entry),
             PHP_INI_ENTRY_NAME_LEN(ini_entry) - 1);
    PUTS(" => ");
    nr_ini_displayer_cb(ini_entry, ZEND_INI_DISPLAY_ACTIVE TSRMLS_CC);
    PUTS(" => ");
    nr_ini_displayer_cb(ini_entry, ZEND_INI_DISPLAY_ORIG TSRMLS_CC);
    PUTS("\n");
  }
  return ZEND_HASH_APPLY_KEEP;
}

static void nr_display_ini_entries_global(zend_module_entry* module TSRMLS_DC) {
  int module_number;

  if (module) {
    module_number = module->module_number;
  } else {
    module_number = 0;
  }

  php_info_print_table_header(2, "Directive Name", "Global Value");
  if (0 != EG(ini_directives)) {
    nr_php_zend_hash_ptr_apply(EG(ini_directives),
                               (nr_php_ptr_apply_t)nr_ini_displayer_global,
                               &module_number TSRMLS_CC);
  }
}

static void nr_display_ini_entries_perdir(zend_module_entry* module TSRMLS_DC) {
  int module_number;

  if (module) {
    module_number = module->module_number;
  } else {
    module_number = 0;
  }

  php_info_print_table_header(3, "Directive Name", "Local/Active Value",
                              "Master/Default Value");
  if (0 != EG(ini_directives)) {
    nr_php_zend_hash_ptr_apply(EG(ini_directives),
                               (nr_php_ptr_apply_t)nr_ini_displayer_perdir,
                               &module_number TSRMLS_CC);
  }
}

#ifdef TAGS
void zm_info_newrelic(void); /* ctags landing pad only */
#endif
PHP_MINFO_FUNCTION(newrelic) {
  php_info_print_table_start();
  php_info_print_table_header(2, "New Relic RPM Monitoring",
                              NR_PHP_PROCESS_GLOBALS(enabled) ? "enabled"
                              : NR_PHP_PROCESS_GLOBALS(mpm_bad)
                                  ? "disabled due to threaded MPM"
                                  : "disabled");
  php_info_print_table_row(2, "New Relic Version", nr_version_verbose());
  php_info_print_table_end();

  if (0 != NR_PHP_PROCESS_GLOBALS(mpm_bad)) {
    return;
  }

  php_info_print_table_start();
  php_info_print_table_colspan_header(2, "Global Directives");
  nr_display_ini_entries_global(zend_module TSRMLS_CC);
  php_info_print_table_end();

  php_info_print_table_start();
  php_info_print_table_colspan_header(3, "Per-Directory Directives");
  nr_display_ini_entries_perdir(zend_module TSRMLS_CC);
  php_info_print_table_end();
}

typedef struct _settings_st {
  int modnum;
  nrobj_t* obj;
} settings_st;

/*
 * This function must be compatible with typedef int
 * (*apply_func_arg_t)(void *pDest, void *argument TSRMLS_DC);
 */
static int nr_ini_settings(zend_ini_entry* ini_entry,
                           settings_st* setarg,
                           zend_hash_key* key NRUNUSED TSRMLS_DC) {
  NR_UNUSED_TSRMLS;

  if (ini_entry->module_number != setarg->modnum) {
    return 0;
  }

  if (!(ini_entry->modifiable & PHP_INI_PERDIR)) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
    if (NULL == PHP_INI_ENTRY_VALUE(ini_entry)
        || 0 == PHP_INI_ENTRY_VALUE_LEN(ini_entry)) {
#pragma GCC diagnostic pop
      if (0
          == nr_strncmp(PHP_INI_ENTRY_NAME(ini_entry),
                        NR_PSTR("newrelic.special"))) {
        return 0;
      }
      if (0
          == nr_strncmp(PHP_INI_ENTRY_NAME(ini_entry),
                        NR_PSTR("newrelic.daemon"))) {
        return 0;
      }
    }
  }

  if ((0
       == nr_strncmp(PHP_INI_ENTRY_NAME(ini_entry),
                     NR_PSTR("newrelic.browser_monitoring.debug")))
      || (0
          == nr_strncmp(PHP_INI_ENTRY_NAME(ini_entry),
                        NR_PSTR("newrelic.distributed_tracing_enabled")))) {
    /*
     * The collector requires that the value of
     * newrelic.browser_monitoring.debug is a bool, so we must convert it
     * here.
     *
     * Also, the daemon expects newrelic.distributed_tracing_enabled to be
     * sent up as a bool, so it must be converted here.
     */
    nro_set_hash_boolean(setarg->obj, PHP_INI_ENTRY_NAME(ini_entry),
                         nr_bool_from_str(PHP_INI_ENTRY_VALUE(ini_entry)));
    return 0;
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
  if (NULL == PHP_INI_ENTRY_VALUE(ini_entry)
      || 0 == PHP_INI_ENTRY_VALUE_LEN(ini_entry)) {
#pragma GCC diagnostic push
    nro_set_hash_string(setarg->obj, PHP_INI_ENTRY_NAME(ini_entry), "no value");
  } else {
    if (0
        == nr_strncmp(PHP_INI_ENTRY_NAME(ini_entry),
                      NR_PSTR("newrelic.license"))) {
      char* plicense
          = nr_app_create_printable_license(PHP_INI_ENTRY_VALUE(ini_entry));

      nro_set_hash_string(setarg->obj, PHP_INI_ENTRY_NAME(ini_entry),
                          plicense ? plicense : "INVALID_FORMAT");
      nr_free(plicense);
    } else if (0
               == nr_strncmp(PHP_INI_ENTRY_NAME(ini_entry),
                             NR_PSTR("newrelic.daemon.proxy"))) {
      char* pproxy = nr_url_proxy_clean(PHP_INI_ENTRY_VALUE(ini_entry));

      nro_set_hash_string(setarg->obj, PHP_INI_ENTRY_NAME(ini_entry),
                          pproxy ? pproxy : "INVALID_FORMAT");
      nr_free(pproxy);
    } else {
      nro_set_hash_string(setarg->obj, PHP_INI_ENTRY_NAME(ini_entry),
                          PHP_INI_ENTRY_VALUE(ini_entry));
    }
  }

  return 0;
}

nrobj_t* nr_php_app_settings(void) {
  settings_st setarg;
  TSRMLS_FETCH();

  setarg.modnum = NR_PHP_PROCESS_GLOBALS(our_module_number);
  setarg.obj = nro_new(NR_OBJECT_HASH);
  if (0 != EG(ini_directives)) {
    nr_php_zend_hash_ptr_apply(EG(ini_directives),
                               (nr_php_ptr_apply_t)nr_ini_settings,
                               &setarg TSRMLS_CC);
  }
  return setarg.obj;
}

int nr_php_ini_setting_is_set_by_user(const char* name) {
#ifdef PHP7
  int found;
  zend_string* zs;

  if (NULL == name) {
    return 0;
  }

  zs = zend_string_init(name, nr_strlen(name), 0);
  found = (NULL != zend_get_configuration_directive(zs));
  zend_string_free(zs);

  return found;
#else
  int zend_rv;
  uint name_length;
  zval default_value;

  if (0 == name) {
    return 0;
  }

  name_length = nr_strlen(name) + 1;

  nr_memset(&default_value, 0, sizeof(default_value));

  zend_rv = zend_get_configuration_directive(name, name_length, &default_value);
  if (SUCCESS == zend_rv) {
    return 1;
  } else {
    return 0;
  }
#endif /* PHP7 */
}
