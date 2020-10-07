/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

/*
 * Since we definitely want assertions in our test cases, we'll undefine NDEBUG
 * to be sure we get the right macros.
 */
#undef NDEBUG
#include <assert.h>

#include "php_agent.h"
#include "php_hash.h"
#include "nr_agent.h"
#include "nr_app.h"
#include "nr_app_private.h"
#include "nr_commands.h"
#include "nr_version.h"
#include "util_logging.h"
#include "util_strings.h"
#include "util_syscalls.h"

#include "ext/standard/dl.h"
#include "ext/standard/php_var.h"

/* {{{ Global variables
 *
 * True globals. We're not going to support multi-threaded execution in PHP
 * agent tests.
 */
static char* argv;
static int fake_daemon_fd = -1;
static zend_llist global_vars;
static char* ini;
static char* logfile;
static char* outfile = NULL;
static FILE* out = NULL;

/*
 * Fake resources need a fake type. It's OK for this to be a true global
 * regardless of thread safety because the resource ID is allocated at MINIT
 * before any threads have been created: core PHP extensions that use resources
 * also use true globals for this reason.
 */
int le_tlib;

/* }}} */

/* {{{ Default INI settings */
static const char DEFAULT_INI[]
    = "html_errors=0\n"
      "error_reporting=-1\n"
      "display_errors=1\n"
      "register_argc_argv=1\n"
      "implicit_flush=1\n"
      "output_buffering=0\n"
      "max_execution_time=0\n"
      "max_input_time=-1\n"
      "newrelic.dont_launch=3\n"
      "newrelic.loglevel=verbosedebug\n"
      "newrelic.license=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "newrelic.daemon.collector_host=collector.newrelic.com\n"
      "\0";
/* }}} */

/* {{{ Callbacks and internal functions */

/*
 * Creates an output filename by adding an extension to the binary name.
 */
static char* tlib_php_create_output_filename(const char* ext) {
  char* abs = realpath(tlib_argc > 0 ? tlib_argv[0] : "./", NULL);
  char* path = nr_formatf("%s.%s", abs, ext);

  free(abs);
  return path;
}

/*
 * A replacement unbuffered write callback to capture any script output for
 * further examination.
 */
#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO
static size_t tlib_php_engine_ub_write(const char* str, size_t len)
#else
static int tlib_php_engine_ub_write(const char* str, uint len TSRMLS_DC)
#endif /* PHP >= 7.0 */
{
  NR_UNUSED_TSRMLS;

  assert(NULL != out);
  fwrite(str, (size_t)len, 1, out);

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO
  return len;
#else
  return NRSAFELEN(len);
#endif /* PHP >= 7.0 */
}

/*
 * Replacement interned string callbacks to prevent interned strings from being
 * created or used: their use causes issues with dynamically loaded extensions
 * due to improper ordering of frees on request shutdown.
 */
#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
static zend_string* ZEND_FASTCALL
tlib_php_new_interned_string(zend_string* str) {
  return str;
}
#elif defined PHP7
static zend_string* tlib_php_new_interned_string(zend_string* str) {
  return str;
}
#elif ZEND_MODULE_API_NO >= ZEND_5_4_X_API_NO
static const char* tlib_php_new_interned_string(const char* key,
                                                int len NRUNUSED,
                                                int free_src NRUNUSED
                                                    TSRMLS_DC) {
  NR_UNUSED_TSRMLS;
  return key;
}
#endif

#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
static zend_string* ZEND_FASTCALL tlib_php_init_interned_string(const char* str,
                                                                size_t size,
                                                                int permanent) {
  return zend_string_init(str, size, permanent);
}
#endif /* PHP >= 7.3 */

#if ZEND_MODULE_API_NO >= ZEND_5_4_X_API_NO \
    && ZEND_MODULE_API_NO < ZEND_7_2_X_API_NO
static void tlib_php_interned_strings_restore(TSRMLS_D) {
  NR_UNUSED_TSRMLS;
}

static void tlib_php_interned_strings_snapshot(TSRMLS_D) {
  NR_UNUSED_TSRMLS;
}
#endif /* PHP >= 5.4 && PHP < 7.2 */

/* }}} */

static nr_status_t stub_cmd_appinfo_tx(int daemon_fd, nrapp_t* app);
static nr_status_t stub_cmd_txndata_tx(int daemon_fd, const nrtxn_t* txn);

/* {{{ Public API functions */

/*
 * Many of the below functions reimplement bits of the embed SAPI API.
 * Unfortunately, embed conflates module and request startup, whereas we need
 * to be able to manage them separately. As a result, we end up reimplementing
 * much of php_embed_init() and php_embed_shutdown(), as they're not modular at
 * all.
 */

nr_status_t tlib_php_engine_create(const char* extra_ini PTSRMLS_DC) {
  nr_cmd_appinfo_hook = stub_cmd_appinfo_tx;
  nr_cmd_txndata_hook = stub_cmd_txndata_tx;

  /*
   * Redirect any daemon communication that we don't somehow capture to stdout.
   * If you see flatbuffers on stdout when running tests, that's a bug!
   */
  fake_daemon_fd = nr_dup(1);
  nr_set_daemon_fd(fake_daemon_fd);

#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */
  sapi_module_struct tlib_module;

  /*
   * Set up our own module struct based on the default embed struct. This is
   * done through a memcpy rather than a complete definition because we don't
   * want to touch most of the default callbacks, but they are declared static
   * and we can't use them directly here.
   */
  nr_memcpy(&tlib_module, &php_embed_module, sizeof(sapi_module_struct));
  tlib_module.startup = NULL;
  tlib_module.ub_write = tlib_php_engine_ub_write;

  /*
   * Start up TSRM if required.
   */
#ifdef ZTS
#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO
  php_tsrm_startup();
  ZEND_TSRMLS_CACHE_UPDATE();
#elif defined PHP7
  tsrm_startup(1, 1, 0, NULL);
  ts_resource(0);
  ZEND_TSRMLS_CACHE_UPDATE();
#else
  tsrm_startup(1, 1, 0, NULL);
  tsrm_ls = ts_resource(0);
  *ptsrm_ls = tsrm_ls;
#endif /* PHP version */
#endif /* ZTS */

#if defined(PHP7) && defined(ZEND_SIGNALS)
  zend_signal_startup();
#endif /* PHP7 && ZEND_SIGNALS */

  /*
   * This currently creates real files for the agent log and output that are
   * subsequently deleted on successful runs. An alternative would be to
   * refactor this to keep them in memory during execution and only dump
   * them to disk if required.
   */
  assert(NULL == out);
  assert(NULL == outfile);
  outfile = tlib_php_create_output_filename("out");
  out = fopen(outfile, "wb");

  /*
   * Do the initial SAPI startup.
   */
  sapi_startup(&tlib_module);

  /*
   * Set up the temporary log file.
   */
  if (logfile) {
    nr_free(logfile);
  }
  logfile = tlib_php_create_output_filename("log");
  unlink(logfile);

  /*
   * Set the ini_entries within the module struct. This is important because
   * SYSTEM settings have to be set at this point.
   */
  if (ini) {
    nr_free(ini);
  }
  ini = nr_formatf("%s\nnewrelic.logfile=%s\n%s", DEFAULT_INI, logfile,
                   NRBLANKSTR(extra_ini));
  tlib_module.ini_entries = ini;

  /*
   * Ensure that PHP doesn't try to load any external php.ini files.
   */
  tlib_module.php_ini_ignore = 1;

  /*
   * We have to disable interned strings on versions of PHP that use them (5.4
   * onwards) in order to support dynamic extensions. The Zend Engine makes
   * assumptions about when interned strings are available that rely heavily on
   * the lifecycle of the normal SAPIs that users use that don't apply here:
   * namely, that extensions are either loaded en masse at the very start of
   * the PHP process, or only via dl() during the only request of the process's
   * lifetime (since dl() is only allowed in the CLI or CGI SAPIs, and they
   * never handle more than one request).
   *
   * We need to be able to support loading extensions after the engine has
   * started up, but also support multiple requests in the same process and
   * support loading extensions outside of requests. The interned string
   * subsystem is in an inconsistent, and usually unsafe state if you break
   * those assumptions, hence why we disable it.
   *
   * From PHP 7.2 onwards, we have to replace the request storage handler
   * callback before calling php_module_startup(), as part of the operation of
   * php_module_startup() involves switching storage handlers to use the
   * request storage handler. If we haven't done it beforehand, the first
   * request will use the default storage handler function instead of ours, and
   * will attempt to write to an uninitialised hash table in the compiler
   * globals.
   *
   * For PHP 5.4-7.1, we need to switch the function pointers after calling
   * php_module_startup(), as there are hard coded references in the Zend
   * Engine to the permanent interned string hash table, and changing those
   * function pointers beforehand means that hash table will never be
   * initialised.
   */
#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
  zend_interned_strings_set_request_storage_handlers(
      tlib_php_new_interned_string, tlib_php_init_interned_string);
#elif ZEND_MODULE_API_NO >= ZEND_7_2_X_API_NO
  zend_interned_strings_set_request_storage_handler(
      tlib_php_new_interned_string);
#endif /* PHP >= 7.2 */

  /*
   * Actually start the Zend Engine.
   */
  if (FAILURE == php_module_startup(&tlib_module, &newrelic_module_entry, 1)) {
    return NR_FAILURE;
  }

  /*
   * As noted above, we now replace the interned string callbacks on PHP
   * 5.4-7.1, inclusive. The effect of these replacements is to disable
   * interned strings.
   */
#if ZEND_MODULE_API_NO >= ZEND_5_4_X_API_NO \
    && ZEND_MODULE_API_NO < ZEND_7_2_X_API_NO
  zend_new_interned_string = tlib_php_new_interned_string;
  zend_interned_strings_restore = tlib_php_interned_strings_restore;
  zend_interned_strings_snapshot = tlib_php_interned_strings_snapshot;
#endif /* PHP >= 5.4 && PHP < 7.2 */

  /*
   * Register the resource type we use to fake resources. We are module 0
   * because we're the SAPI.
   */
  le_tlib = zend_register_list_destructors_ex(NULL, NULL, "tlib", 0);

  return NR_SUCCESS;
}

void tlib_php_engine_destroy(TSRMLS_D) {
  php_module_shutdown(TSRMLS_C);
  sapi_shutdown();

#ifdef ZTS
  tsrm_shutdown();
#endif /* ZTS */

  fclose(out);
  out = NULL;

  if (tlib_unexpected_failcount > 0) {
    /*
     * If one or more test failures have occurred, we're going to keep the
     * agent log and PHP output. Let's tell the user where to find them.
     */
    printf(
        "Errors occurred. Output files:\n"
        "\tAgent log:  %s\n"
        "\tPHP output: %s\n",
        logfile, outfile);
  } else {
    /*
     * No errors, no problem. Let's get rid of the log and output files.
     */
    unlink(logfile);
    unlink(outfile);
  }

  nr_free(ini);
  nr_free(logfile);
  nr_free(outfile);
}

nr_status_t tlib_php_request_start_impl(const char* file, int line TSRMLS_DC) {
  assert(NULL != out);
  assert(NULL == argv);

  fprintf(out, "*** Request started at %s:%d\n\n", file, line);

  argv = nr_strdup("-");

  /*
   * Reset the daemon FD, as the agent will close an existing connection on
   * MINIT if it thinks the SAPI isn't CLI.
   */
  nr_set_daemon_fd(fake_daemon_fd);

  /*
   * Much of the below seeks to emulate php_embed_init().
   *
   * Firstly, we want to set up the global variable list.
   */
  zend_llist_init(&global_vars, sizeof(char*), NULL, 0);

  /*
   * Set up the server globals required for request startup.
   */
  SG(options) |= SAPI_OPTION_NO_CHDIR;
  SG(request_info).argc = 1;
  SG(request_info).argv = &argv;

  if (FAILURE == php_request_startup(TSRMLS_C)) {
    return NR_FAILURE;
  }

  /*
   * Prevent header handling in PHP, since we're faking the CLI SAPI.
   */
  SG(headers_sent) = 1;
  SG(request_info).no_headers = 1;

  /*
   * Set $PHP_SELF.
   */
  php_register_variable("PHP_SELF", "-", NULL TSRMLS_CC);

  return NR_SUCCESS;
}

void tlib_php_request_end_impl(const char* file, int line) {
  assert(NULL != out);

  php_request_shutdown(NULL);

  zend_llist_destroy(&global_vars);
  nr_free(argv);

  fprintf(out, "\n\n*** Request ended at %s:%d\n", file, line);
}

bool tlib_php_request_is_active(void) {
  return (NULL != argv);
}

void tlib_php_request_eval(const char* code TSRMLS_DC) {
  char* copy = nr_strdup(code);

  assert(tlib_php_request_is_active());

  zend_eval_string(copy, NULL, "-" TSRMLS_CC);
  nr_free(copy);
}

zval* tlib_php_request_eval_expr(const char* code TSRMLS_DC) {
  char* copy = nr_strdup(code);
  zval* rv = nr_php_zval_alloc();

  assert(tlib_php_request_is_active());

  zend_eval_string(copy, rv, "-" TSRMLS_CC);
  nr_free(copy);
  return rv;
}

#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
static void tlib_php_error_silence_cb(int type NRUNUSED,
                                      const char* error_filename NRUNUSED,
                                      const uint32_t error_lineno NRUNUSED,
                                      const char* format NRUNUSED,
                                      va_list args NRUNUSED) {
  /* Squash the error by doing absolutely nothing. */
}
#endif /* PHP >= 7.3 */

int tlib_php_require_extension(const char* extension TSRMLS_DC) {
  char* file = NULL;
  int loaded = 0;
#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
  void (*prev_error_cb)(int, const char*, const uint32_t, const char*, va_list);
#else
  zend_error_handling_t prev_error;
#endif /* PHP >= 7.3 */

  /*
   * PHP 7.2.7 changed the behavior of zend_register_class_alias_ex() to always
   * use an interned string for the class alias name, which fixed
   * https://bugs.php.net/bug.php?id=76337. This function is invoked by the
   * redis MINIT function, which means that we need to have the interned string
   * system in a functional state when that extension is loaded.
   *
   * On PHP 7.1 and older, this is the case, because we set
   * zend_new_interned_string in tlib_php_engine_create().
   *
   * On PHP 7.2 and newer, however, we use the
   * zend_interned_strings_set_request_storage_handler() function, as
   * php_module_startup() switches the storage handler by resetting
   * zend_new_interned_string directly whenever a request starts or ends. (See
   * also the commentary in tlib_php_engine_create().)
   *
   * This works well for interned strings created within a request context, but
   * doesn't help us for interned strings created outside a request context. The
   * Zend Engine does not allow us to set a global non-request handler in the
   * same way, so instead, we'll temporarily set the handler before invoking
   * php_load_extension() and put it back at the end of this function.
   *
   * For PHP 7.3 and newer, we also need to do the same for
   * zend_string_init_interned to cover all possible interned string scenarios.
   */
#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
  zend_new_interned_string_func_t saved_new_interned_string;
  zend_string_init_interned_func_t saved_string_init_interned;

  saved_new_interned_string = zend_new_interned_string;
  zend_new_interned_string = tlib_php_new_interned_string;
  saved_string_init_interned = zend_string_init_interned;
  zend_string_init_interned = tlib_php_init_interned_string;
#elif ZEND_MODULE_API_NO >= ZEND_7_2_X_API_NO
  zend_string* (*saved_new_interned_string)(zend_string * str);

  saved_new_interned_string = zend_new_interned_string;
  zend_new_interned_string = tlib_php_new_interned_string;
#endif

  if (nr_php_extension_loaded(extension)) {
    loaded = 1;
    goto end;
  }

  file = nr_formatf("%s.so", extension);

  /*
   * We'll override the executor's error_handling setting to suppress the
   * warning that php_load_extension() will generate if the extension can't be
   * loaded.
   */
#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
  prev_error_cb = zend_error_cb;
  zend_error_cb = tlib_php_error_silence_cb;
#else
  prev_error = EG(error_handling);
  EG(error_handling) = EH_SUPPRESS;
#endif /* PHP >= 7.3 */

  php_load_extension(file, MODULE_PERSISTENT, 1 TSRMLS_CC);

  /*
   * Restore normal error service.
   */
#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
  zend_error_cb = prev_error_cb;
#else
  EG(error_handling) = prev_error;
#endif /* PHP >= 7.3 */

  loaded = nr_php_extension_loaded(extension);

end:
  nr_free(file);

#if ZEND_MODULE_API_NO >= ZEND_7_2_X_API_NO
  zend_new_interned_string = saved_new_interned_string;
#endif /* PHP >= 7.2 */

#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
  zend_string_init_interned = saved_string_init_interned;
#endif /* PHP >= 7.3 */

  return loaded;
}

tlib_php_internal_function_handler_t tlib_php_replace_internal_function(
    const char* klass,
    const char* function,
    tlib_php_internal_function_handler_t handler TSRMLS_DC) {
  zend_internal_function* func = NULL;
  HashTable* function_table = NULL;
  char* lcname = nr_string_to_lowercase(function);
  tlib_php_internal_function_handler_t old = NULL;

  assert(NULL != function);
  assert(NULL != handler);

  nrl_verbosedebug(NRL_AGENT, "%s: replacing %s%s%s", __func__,
                   klass ? klass : "", klass ? "::" : "", function);

  if (klass) {
    zend_class_entry* ce = NULL;
    char* lcclass = nr_string_to_lowercase(klass);

#ifdef PHP7
    ce = (zend_class_entry*)nr_php_zend_hash_find_ptr(CG(class_table), lcclass);
#else
    zend_class_entry** ce_ptr
        = nr_php_zend_hash_find_ptr(CG(class_table), lcclass);

    if (ce_ptr) {
      ce = *ce_ptr;
    }
#endif /* PHP7 */

    nr_free(lcclass);
    if (NULL == ce) {
      nrl_verbosedebug(NRL_AGENT, "%s: cannot find class entry for %s",
                       __func__, klass);
      goto end;
    }

    function_table = &ce->function_table;
  } else {
    function_table = CG(function_table);
  }

  func = nr_php_zend_hash_find_ptr(function_table, function);
  if (NULL == func) {
    nrl_verbosedebug(NRL_AGENT, "%s: NULL function entry", __func__);
    goto end;
  } else if (ZEND_INTERNAL_FUNCTION != func->type) {
    nrl_verbosedebug(NRL_AGENT, "%s: function is not an internal function",
                     __func__);
    goto end;
  }

  old = func->handler;
  func->handler = handler;
  nrl_verbosedebug(NRL_AGENT, "%s: replacement complete", __func__);

end:
  nr_free(lcname);
  return old;
}

zval* tlib_php_zval_create_default(zend_uchar type TSRMLS_DC) {
  zval* zv = nr_php_zval_alloc();

  switch (type) {
    case IS_NULL:
      ZVAL_NULL(zv);
      break;

    case IS_LONG:
      ZVAL_LONG(zv, 0);
      break;

    case IS_DOUBLE:
      ZVAL_DOUBLE(zv, 0.0);
      break;

    case IS_ARRAY:
      array_init(zv);
      break;

    case IS_OBJECT:
      object_init(zv);
      break;

    case IS_STRING:
      nr_php_zval_str(zv, "");
      break;

#ifdef PHP7
    case IS_UNDEF:
      ZVAL_UNDEF(zv);
      break;

    case IS_FALSE:
      ZVAL_BOOL(zv, 0);
      break;

    case IS_TRUE:
      ZVAL_BOOL(zv, 1);
      break;

    case IS_RESOURCE:
      /*
       * PHP 7 requires a non-NULL pointer for the resource. The actual
       * pointer doesn't matter much, since we're never
       * going to use it again and we didn't set a resource destructor, so we'll
       * just use the address of the zval that we're modifying.
       */
      ZVAL_RES(zv, zend_register_resource(&zv, le_tlib));
      break;

    case IS_REFERENCE:
      /*
       * We used to just use ZVAL_NEW_EMPTY_REF() here to create an empty
       * IS_REFERENCE zval, which seemed to work on PHP 7.0. On PHP 7.1,
       * though, we discovered that calling zval_ptr_dtor() on an IS_REFERENCE
       * zval with an empty reference can segfault, as PHP doesn't properly
       * initialise the empty reference to make it safe to destroy (it simply
       * malloc()s a chunk of memory for the zend_reference structure and then
       * waits for the user to set it to a valid value).
       *
       * Although this only manifested on PHP 7.1 on 32 bit builds of PHP, the
       * theoretical bug exists on all PHP 7 versions.
       *
       * Instead, what we'll do here is to make the zval that we're creating a
       * reference to a NULL zval, which then allows destruction to continue
       * normally.
       */
      {
        zval refval;

        ZVAL_NULL(&refval);

        /*
         * This looks fairly weird: we're setting zv to be a reference to a
         * variable that's about to go out of scope. This is valid, however,
         * because of how ZVAL_NEW_REF() operates: it copies the value of the
         * second parameter, rather than actually keeping a true reference to
         * it. For a scalar, like the IS_NULL zval refval is, this means it
         * copies the entire value.
         *
         * The astute reader will notice that this means that, in spite of its
         * name, ZVAL_NEW_REF() doesn't actually create a true reference to the
         * zval pointer it's given. This is apparently intentional: a user
         * would need to construct and set their own zend_reference struct and
         * use ZVAL_REF() to create the reference in order to do this.
         */
        ZVAL_NEW_REF(zv, &refval);
      }
      break;
#else
    case IS_BOOL:
      ZVAL_BOOL(zv, 0);
      break;

    case IS_RESOURCE:
      ZEND_REGISTER_RESOURCE(zv, NULL, le_tlib);
      break;
#endif /* PHP7 */

    default:
      nr_php_zval_free(&zv);
      return NULL;
  }

  return zv;
}

#ifdef PHP7
static const zend_uchar default_zval_types[] = {
    IS_UNDEF,  IS_NULL,  IS_FALSE,  IS_TRUE,     IS_LONG,      IS_DOUBLE,
    IS_STRING, IS_ARRAY, IS_OBJECT, IS_RESOURCE, IS_REFERENCE,
};
#else
static const zend_uchar default_zval_types[] = {
    IS_NULL,  IS_LONG,   IS_DOUBLE, IS_BOOL,
    IS_ARRAY, IS_OBJECT, IS_STRING, IS_RESOURCE,
};
#endif

zval** tlib_php_zvals_of_all_types(TSRMLS_D) {
  zval** arr;
  size_t i;
  static const size_t num_types
      = sizeof(default_zval_types) / sizeof(default_zval_types[0]);

  /*
   * We allocate one additional entry so that the last element is NULL.
   */
  arr = nr_zalloc((num_types + 1) * sizeof(zval*));

  for (i = 0; i < num_types; i++) {
    arr[i] = tlib_php_zval_create_default(default_zval_types[i] TSRMLS_CC);
  }

  return arr;
}

zval** tlib_php_zvals_not_of_type(zend_uchar type TSRMLS_DC) {
  zval** arr;
  size_t i;
  size_t idx = 0;
  static const size_t num_types
      = sizeof(default_zval_types) / sizeof(default_zval_types[0]);

  /*
   * Although we're going to omit one type, we still zero-allocate the array to
   * be the same length as the full type array to ensure that the final element
   * is NULL.
   */
  arr = nr_zalloc(num_types * sizeof(zval*));

  for (i = 0; i < num_types; i++) {
    if (type != default_zval_types[i]) {
      arr[idx] = tlib_php_zval_create_default(default_zval_types[i] TSRMLS_CC);
      idx++;
    }
  }

  return arr;
}

void tlib_php_free_zval_array(zval*** arr_ptr) {
  zval** arr;
  size_t i;

  if ((NULL == arr_ptr) || (NULL == *arr_ptr)) {
    return;
  }

  arr = *arr_ptr;
  for (i = 0; arr[i]; i++) {
    nr_php_zval_free(&arr[i]);
  }

  nr_realfree((void**)arr_ptr);
}

char* tlib_php_zval_dump(zval* zv TSRMLS_DC) {
  char* dump = NULL;
  zval* result;

  if (NULL == zv) {
    return nr_strdup("<NULL pointer>");
  }

  tlib_php_request_eval("ob_start();" TSRMLS_CC);
#ifdef PHP7
  php_var_dump(zv, 0);
#else
  php_var_dump(&zv, 0 TSRMLS_CC);
#endif
  result = tlib_php_request_eval_expr("ob_get_clean()" TSRMLS_CC);
  if (nr_php_is_zval_valid_string(result)) {
    dump = nr_strndup(Z_STRVAL_P(result), Z_STRLEN_P(result));
  }

  nr_php_zval_free(&result);
  return dump;
}

/* }}} */

/* {{{ Axiom function replacements */

/*
 * We're going to hook functions in axiom to prevent daemon communication while
 * providing the agent with what looks like a real app definition.
 */

/*
 * The raw app connect reply that we're going to use as a fake. This was
 * literally copied out of the daemon log.
 */
const char* app_connect_reply
    = "{"
      "   \"data_report_period\" : 60,"
      "   \"browser_monitoring.debug\" : null,"
      "   \"episodes_file\" : \"js-agent.newrelic.com/nr-106.js\","
      "   \"url_rules\" : ["
      "      {"
      "         \"replacement\" : \"\\\\1\","
      "         \"ignore\" : false,"
      "         \"eval_order\" : 0,"
      "         \"terminate_chain\" : true,"
      "         \"each_segment\" : false,"
      "         \"replace_all\" : false,"
      "         \"match_expression\" : \"^(test_match_nothing)$\""
      "      },"
      "      {"
      "         \"ignore\" : false,"
      "         \"each_segment\" : false,"
      "         \"eval_order\" : 0,"
      "         \"terminate_chain\" : true,"
      "         \"replace_all\" : false,"
      "         \"match_expression\" : \"^(test_match_nothing)$\","
      "         \"replacement\" : \"\\\\1\""
      "      },"
      "      {"
      "         \"eval_order\" : 0,"
      "         \"ignore\" : false,"
      "         \"terminate_chain\" : true,"
      "         \"each_segment\" : false,"
      "         \"replace_all\" : false,"
      "         \"match_expression\" : "
      "\".*\\\\.(css|gif|ico|jpe?g|js|png|swf)$\","
      "         \"replacement\" : \"/*.\\\\1\""
      "      },"
      "      {"
      "         \"match_expression\" : "
      "\".*\\\\.(css|gif|ico|jpe?g|js|png|swf)$\","
      "         \"eval_order\" : 0,"
      "         \"ignore\" : false,"
      "         \"terminate_chain\" : true,"
      "         \"each_segment\" : false,"
      "         \"replace_all\" : false,"
      "         \"replacement\" : \"/*.\\\\1\""
      "      },"
      "      {"
      "         \"replacement\" : \"\\\\1\","
      "         \"ignore\" : false,"
      "         \"replace_all\" : false,"
      "         \"eval_order\" : 0,"
      "         \"each_segment\" : false,"
      "         \"terminate_chain\" : true,"
      "         \"match_expression\" : \"^(test_match_nothing)$\""
      "      },"
      "      {"
      "         \"replacement\" : \"/*.\\\\1\","
      "         \"eval_order\" : 0,"
      "         \"ignore\" : false,"
      "         \"each_segment\" : false,"
      "         \"replace_all\" : false,"
      "         \"terminate_chain\" : true,"
      "         \"match_expression\" : "
      "\".*\\\\.(css|gif|ico|jpe?g|js|png|swf)$\""
      "      },"
      "      {"
      "         \"match_expression\" : \"^(test_match_nothing)$\","
      "         \"ignore\" : false,"
      "         \"eval_order\" : 0,"
      "         \"replace_all\" : false,"
      "         \"each_segment\" : false,"
      "         \"terminate_chain\" : true,"
      "         \"replacement\" : \"\\\\1\""
      "      },"
      "      {"
      "         \"eval_order\" : 0,"
      "         \"ignore\" : false,"
      "         \"each_segment\" : false,"
      "         \"terminate_chain\" : true,"
      "         \"replace_all\" : false,"
      "         \"match_expression\" : "
      "\".*\\\\.(css|gif|ico|jpe?g|js|png|swf)$\","
      "         \"replacement\" : \"/*.\\\\1\""
      "      },"
      "      {"
      "         \"replacement\" : \"*\","
      "         \"eval_order\" : 1,"
      "         \"ignore\" : false,"
      "         \"replace_all\" : false,"
      "         \"each_segment\" : true,"
      "         \"terminate_chain\" : false,"
      "         \"match_expression\" : \"^[0-9][0-9a-f_,.-]*$\""
      "      },"
      "      {"
      "         \"replacement\" : \"*\","
      "         \"match_expression\" : \"^[0-9][0-9a-f_,.-]*$\","
      "         \"terminate_chain\" : false,"
      "         \"ignore\" : false,"
      "         \"eval_order\" : 1,"
      "         \"each_segment\" : true,"
      "         \"replace_all\" : false"
      "      },"
      "      {"
      "         \"replacement\" : \"*\","
      "         \"ignore\" : false,"
      "         \"eval_order\" : 1,"
      "         \"terminate_chain\" : false,"
      "         \"each_segment\" : true,"
      "         \"replace_all\" : false,"
      "         \"match_expression\" : \"^[0-9][0-9a-f_,.-]*$\""
      "      },"
      "      {"
      "         \"replacement\" : \"*\","
      "         \"ignore\" : false,"
      "         \"eval_order\" : 1,"
      "         \"each_segment\" : true,"
      "         \"terminate_chain\" : false,"
      "         \"replace_all\" : false,"
      "         \"match_expression\" : \"^[0-9][0-9a-f_,.-]*$\""
      "      },"
      "      {"
      "         \"replacement\" : \"\\\\1/.*\\\\2\","
      "         \"ignore\" : false,"
      "         \"eval_order\" : 2,"
      "         \"each_segment\" : false,"
      "         \"replace_all\" : false,"
      "         \"terminate_chain\" : false,"
      "         \"match_expression\" : "
      "\"^(.*)/[0-9][0-9a-f_,-]*\\\\.([0-9a-z][0-9a-z]*)$\""
      "      },"
      "      {"
      "         \"ignore\" : false,"
      "         \"eval_order\" : 2,"
      "         \"each_segment\" : false,"
      "         \"terminate_chain\" : false,"
      "         \"replace_all\" : false,"
      "         \"match_expression\" : "
      "\"^(.*)/[0-9][0-9a-f_,-]*\\\\.([0-9a-z][0-9a-z]*)$\","
      "         \"replacement\" : \"\\\\1/.*\\\\2\""
      "      },"
      "      {"
      "         \"replacement\" : \"\\\\1/.*\\\\2\","
      "         \"replace_all\" : false,"
      "         \"ignore\" : false,"
      "         \"eval_order\" : 2,"
      "         \"each_segment\" : false,"
      "         \"terminate_chain\" : false,"
      "         \"match_expression\" : "
      "\"^(.*)/[0-9][0-9a-f_,-]*\\\\.([0-9a-z][0-9a-z]*)$\""
      "      },"
      "      {"
      "         \"match_expression\" : "
      "\"^(.*)/[0-9][0-9a-f_,-]*\\\\.([0-9a-z][0-9a-z]*)$\","
      "         \"ignore\" : false,"
      "         \"eval_order\" : 2,"
      "         \"terminate_chain\" : false,"
      "         \"each_segment\" : false,"
      "         \"replace_all\" : false,"
      "         \"replacement\" : \"\\\\1/.*\\\\2\""
      "      },"
      "      {"
      "         \"ignore\" : false,"
      "         \"eval_order\" : 1000,"
      "         \"terminate_chain\" : true,"
      "         \"each_segment\" : false,"
      "         \"replace_all\" : false,"
      "         \"match_expression\" : "
      "\".*\\\\.(ace|arj|ini|txt|udl|plist|css|gif|ico|jpe?g|js|png|swf|woff|"
      "caf|aiff|m4v|mpe?g|mp3|mp4|mov)$\","
      "         \"replacement\" : \"/*.\\\\1\""
      "      },"
      "      {"
      "         \"ignore\" : false,"
      "         \"each_segment\" : true,"
      "         \"eval_order\" : 1001,"
      "         \"terminate_chain\" : false,"
      "         \"replace_all\" : false,"
      "         \"match_expression\" : \"^[0-9][0-9a-f_,.-]*$\","
      "         \"replacement\" : \"*\""
      "      },"
      "      {"
      "         \"eval_order\" : 1002,"
      "         \"ignore\" : false,"
      "         \"terminate_chain\" : false,"
      "         \"each_segment\" : false,"
      "         \"replace_all\" : false,"
      "         \"match_expression\" : "
      "\"^(.*)/[0-9][0-9a-f_,-]*\\\\.([0-9a-z][0-9a-z]*)$\","
      "         \"replacement\" : \"\\\\1/.*\\\\2\""
      "      }"
      "   ],"
      "   \"error_beacon\" : \"collector.newrelic.com\","
      "   \"browser_monitoring.loader\" : null,"
      "   \"application_id\" : \"18303\","
      "   \"collect_traces\" : true,"
      "   \"agent_run_id\" : \"851236749585274\","
      "   \"js_agent_loader\" : "
      "\"window.NREUM||(NREUM={}),__nr_require=function(e,t,n){function "
      "r(n){if(!t[n]){var "
      "o=t[n]={exports:{}};e[n][0].call(o.exports,function(t){var "
      "o=e[n][1][t];return r(o||t)},o,o.exports)}return "
      "t[n].exports}if(\\\"function\\\"==typeof __nr_require)return "
      "__nr_require;for(var o=0;o<n.length;o++)r(n[o]);return "
      "r}({1:[function(e,t,n){function r(e,t){return function(){o(e,[(new "
      "Date).getTime()].concat(a(arguments)),null,t)}}var "
      "o=e(\\\"handle\\\"),i=e(2),a=e(3);\\\"undefined\\\"==typeof "
      "window.newrelic&&(newrelic=NREUM);var "
      "u=[\\\"setPageViewName\\\",\\\"addPageAction\\\","
      "\\\"setCustomAttribute\\\",\\\"finished\\\",\\\"addToTrace\\\","
      "\\\"inlineHit\\\"],c=[\\\"addPageAction\\\"],f=\\\"api-\\\";i(u,"
      "function(e,t){newrelic[t]=r(f+t,\\\"api\\\")}),i(c,function(e,t){"
      "newrelic[t]=r(f+t)}),t.exports=newrelic,newrelic.noticeError=function(e)"
      "{\\\"string\\\"==typeof e&&(e=new Error(e)),o(\\\"err\\\",[e,(new "
      "Date).getTime()])}},{}],2:[function(e,t,n){function r(e,t){var "
      "n=[],r=\\\"\\\",i=0;for(r in "
      "e)o.call(e,r)&&(n[i]=t(r,e[r]),i+=1);return n}var "
      "o=Object.prototype.hasOwnProperty;t.exports=r},{}],3:[function(e,t,n){"
      "function r(e,t,n){t||(t=0),\\\"undefined\\\"==typeof "
      "n&&(n=e?e.length:0);for(var "
      "r=-1,o=n-t||0,i=Array(0>o?0:o);++r<o;)i[r]=e[t+r];return "
      "i}t.exports=r},{}],ee:[function(e,t,n){function r(){}function "
      "o(e){function t(e){return e&&e instanceof r?e:e?u(e,a,i):i()}function "
      "n(n,r,o){e&&e(n,r,o);for(var "
      "i=t(o),a=l(n),u=a.length,c=0;u>c;c++)a[c].apply(i,r);var "
      "s=f[g[n]];return s&&s.push([m,n,r,i]),i}function "
      "p(e,t){w[e]=l(e).concat(t)}function l(e){return w[e]||[]}function "
      "d(e){return s[e]=s[e]||o(n)}function "
      "v(e,t){c(e,function(e,n){t=t||\\\"feature\\\",g[n]=t,t in "
      "f||(f[t]=[])})}var "
      "w={},g={},m={on:p,emit:n,get:d,listeners:l,context:t,buffer:v};return "
      "m}function i(){return new r}var "
      "a=\\\"nr@context\\\",u=e(\\\"gos\\\"),c=e(2),f={},s={},p=t.exports=o();"
      "p.backlog=f},{}],gos:[function(e,t,n){function "
      "r(e,t,n){if(o.call(e,t))return e[t];var "
      "r=n();if(Object.defineProperty&&Object.keys)try{return "
      "Object.defineProperty(e,t,{value:r,writable:!0,enumerable:!1}),r}catch("
      "i){}return e[t]=r,r}var "
      "o=Object.prototype.hasOwnProperty;t.exports=r},{}],handle:[function(e,t,"
      "n){function r(e,t,n,r){o.buffer([e],r),o.emit(e,t,n)}var "
      "o=e(\\\"ee\\\").get(\\\"handle\\\");t.exports=r,r.ee=o},{}],id:["
      "function(e,t,n){function r(e){var t=typeof "
      "e;return!e||\\\"object\\\"!==t&&\\\"function\\\"!==t?-1:e===window?0:a("
      "e,i,function(){return o++})}var "
      "o=1,i=\\\"nr@id\\\",a=e(\\\"gos\\\");t.exports=r},{}],loader:[function("
      "e,t,n){function r(){if(!w++){var "
      "e=v.info=NREUM.info,t=s.getElementsByTagName(\\\"script\\\")[0];if(e&&e."
      "licenseKey&&e.applicationID&&t){c(l,function(t,n){e[t]||(e[t]=n)});var "
      "n=\\\"https\\\"===p.split(\\\":\\\")[0]||e.sslForHttp;v.proto=n?"
      "\\\"https://\\\":\\\"http://"
      "\\\",u(\\\"mark\\\",[\\\"onload\\\",a()],null,\\\"api\\\");var "
      "r=s.createElement(\\\"script\\\");r.src=v.proto+e.agent,t.parentNode."
      "insertBefore(r,t)}}}function "
      "o(){\\\"complete\\\"===s.readyState&&i()}function "
      "i(){u(\\\"mark\\\",[\\\"domContent\\\",a()],null,\\\"api\\\")}function "
      "a(){return(new Date).getTime()}var "
      "u=e(\\\"handle\\\"),c=e(2),f=window,s=f.document;NREUM.o={ST:setTimeout,"
      "CT:clearTimeout,XHR:f.XMLHttpRequest,REQ:f.Request,EV:f.Event,PR:f."
      "Promise,MO:f.MutationObserver},e(1);var "
      "p=\\\"\\\"+location,l={beacon:\\\"collector.newrelic.com\\\","
      "errorBeacon:"
      "\\\"collector.newrelic.com\\\",agent:\\\"js-agent.newrelic.com/"
      "nr-918.min.js\\\"},d=window.XMLHttpRequest&&XMLHttpRequest.prototype&&"
      "XMLHttpRequest.prototype.addEventListener&&!/CriOS/"
      ".test(navigator.userAgent),v=t.exports={offset:a(),origin:p,features:{},"
      "xhrWrappable:d};s.addEventListener?(s.addEventListener("
      "\\\"DOMContentLoaded\\\",i,!1),f.addEventListener(\\\"load\\\",r,!1)):("
      "s.attachEvent(\\\"onreadystatechange\\\",o),f.attachEvent("
      "\\\"onload\\\",r)),u(\\\"mark\\\",[\\\"firstbyte\\\",a()],null,"
      "\\\"api\\\");var w=0},{}]},{},[\\\"loader\\\"]);\","
      "   \"messages\" : ["
      "      {"
      "         \"level\" : \"INFO\","
      "         \"message\" : \"Reporting to: "
      "https://collector.newrelic.com/accounts/000000/applications/00000\""
      "      }"
      "   ],"
      "   \"browser_key\" : \"fa68e5730a\","
      "   \"collect_errors\" : true,"
      "   \"js_agent_loader_version\" : \"nr-loader-full-476.min.js\","
      "   \"trusted_account_ids\" : ["
      "      204549"
      "   ],"
      "   \"apdex_t\" : 0.5,"
      "   \"cross_process_id\" : \"000000#00000\","
      "   \"episodes_url\" : \"https://collector.newrelic.com/nr-106.js\","
      "   \"browser_monitoring.loader_version\" : \"918\","
      "   \"transaction_naming_scheme\" : \"legacy\","
      "   \"collect_analytics_events\" : true,"
      "   \"transaction_segment_terms\" : ["
      "      {"
      "         \"terms\" : ["
      "            \"display.php\","
      "            \"myblog2\","
      "            \"phpinfo.php\""
      "         ],"
      "         \"prefix\" : "
      "\"Browser/PageView/localhost/internal_white_terms/\""
      "      }"
      "   ],"
      "   \"encoding_key\" : \"d67afc830dab717fd163bfcb0b8b88423e9a1a3b\","
      "   \"sampling_rate\" : 0,"
      "   \"js_agent_file\" : \"\","
      "   \"collect_error_events\" : true,"
      "   \"beacon\" : \"collector.newrelic.com\","
      "   \"product_level\" : 40"
      "}";

static nr_status_t stub_cmd_appinfo_tx(int daemon_fd NRUNUSED, nrapp_t* app) {
  /*
   * Fake just enough of the app to satisfy the agent.
   */
  app->state = NR_APP_OK;
  app->connect_reply = nro_create_from_json(app_connect_reply);

  nr_free(app->agent_run_id);
  app->agent_run_id = nr_strdup(
      nro_get_hash_string(app->connect_reply, "agent_run_id", NULL));
  app->state = NR_APP_OK;
  nr_rules_destroy(&app->url_rules);
  app->url_rules = nr_rules_create_from_obj(
      nro_get_hash_array(app->connect_reply, "url_rules", 0));
  nr_rules_destroy(&app->txn_rules);
  app->txn_rules = nr_rules_create_from_obj(
      nro_get_hash_array(app->connect_reply, "transaction_name_rules", 0));
  nr_segment_terms_destroy(&app->segment_terms);
  app->segment_terms = nr_segment_terms_create_from_obj(
      nro_get_hash_array(app->connect_reply, "transaction_segment_terms", 0));

  return NR_SUCCESS;
}

static nr_status_t stub_cmd_txndata_tx(int daemon_fd NRUNUSED,
                                       const nrtxn_t* txn NRUNUSED) {
  /*
   * Discard any TXNDATA. In the longer term, we may want to capture this for
   * testing purposes.
   */
  return NR_SUCCESS;
}
/* }}} */

void tlib_pass_if_zval_identical_f(const char* msg,
                                   zval* expected,
                                   zval* actual,
                                   const char* file,
                                   int line TSRMLS_DC) {
  char* actual_str = tlib_php_zval_dump(actual TSRMLS_CC);
  char* expected_str = tlib_php_zval_dump(expected TSRMLS_CC);
  zval* result = nr_php_zval_alloc();

  /*
   * This shouldn't fail under normal circumstances: if it does,
   * that's probably an indication that the expected or actual zval is bogus.
   */
  tlib_pass_if_true_f(
      msg, SUCCESS == is_identical_function(result, expected, actual TSRMLS_CC),
      file, line,
      "SUCCESS == is_identical_function(result, expected, actual TSRMLS_CC)",
      "expected=%s actual=%s", expected_str, actual_str);

  tlib_pass_if_true_f(msg, nr_php_is_zval_true(result), file, line,
                      "expected === actual", "expected=%s actual=%s",
                      expected_str, actual_str);

  nr_free(actual_str);
  nr_free(expected_str);
  nr_php_zval_free(&result);
}

/* vim: set fdm=marker: */
