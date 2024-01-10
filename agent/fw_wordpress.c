/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_hash.h"
#include "php_internal_instrument.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_logging.h"
#include "util_matcher.h"
#include "util_memory.h"
#include "util_regex.h"
#include "util_strings.h"
#include "fw_wordpress.h"

#define NR_WORDPRESS_HOOK_PREFIX "Framework/WordPress/Hook/"
#define NR_WORDPRESS_PLUGIN_PREFIX "Framework/WordPress/Plugin/"

static nr_regex_t* wordpress_hook_regex;

static nr_matcher_t* create_matcher_for_constant(const char* constant,
                                                 const char* suffix) {
  zval* value = nr_php_get_constant(constant);

  if (nr_php_is_zval_valid_string(value)) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Wordpress: found value = %s for constant=%s",
                     Z_STRVAL_P(value), constant);
    nr_matcher_t* matcher = nr_matcher_create();
    char* prefix = nr_formatf("%s%s", Z_STRVAL_P(value), suffix);

    nr_matcher_add_prefix(matcher, prefix);

    nr_free(prefix);
    nr_php_zval_free(&value);
    return matcher;
  } else {
    /*
     * If the constant isn't set, that's not a problem, but if it is and it's
     * an unexpected type we should log a message.
     */
    if (value) {
      nrl_verbosedebug(NRL_FRAMEWORK, "%s: unexpected non-string value for %s",
                       __func__, constant);
    }
  }

  nr_php_zval_free(&value);
  return NULL;
}

/*
 * Purpose : Strip the ".php" file extension from a file name
 *
 * Params  : 1. The string filename
 *           2. The filename length
 *
 * Returns : A newly allocated string stripped of the .php extension
 *
 */
static inline char* nr_wordpress_strip_php_suffix(char* filename, int filename_len) {
  char* retval = NULL;

  if (!nr_striendswith(filename, filename_len, NR_PSTR(".php"))) {
    return filename;
  }

  retval = nr_strndup(filename, filename_len - (sizeof(".php") - 1));
  nr_free(filename);
  return retval;
}

static nr_matcher_t* nr_wordpress_core_matcher() {
  nr_matcher_t* matcher = NULL;

  if (NRPRG(wordpress_core_matcher)) {
    return NRPRG(wordpress_core_matcher);
  }

  matcher = create_matcher_for_constant("WPINC", "");
  if (NULL == matcher) {
    matcher = nr_matcher_create();
    nr_matcher_add_prefix(matcher, "/wp-includes");
  }

  NRPRG(wordpress_core_matcher) = matcher;
  return matcher;
}

static nr_matcher_t* nr_wordpress_plugin_matcher() {
  nr_matcher_t* matcher = NULL;

  if (NRPRG(wordpress_plugin_matcher)) {
    return NRPRG(wordpress_plugin_matcher);
  }

  /*
   * Basically, we're going to look for these constants in order, both of which
   * should be defined on WordPress 3.0 or later:
   *
   * 1. WP_PLUGIN_DIR: absolute path to the plugin directory.
   * 2. WP_CONTENT_DIR: absolute path to the content directory, which should
   *                    then contain "plugins" if WP_PLUGIN_DIR isn't set.
   *
   * If neither exists, we'll just look for "/plugins" and hope for
   * the best.
   */

  matcher = create_matcher_for_constant("WP_PLUGIN_DIR", "");
  if (NULL == matcher) {
    matcher = create_matcher_for_constant("WP_CONTENT_DIR", "/plugins");
  }

  /*
   * Fallback if the constants didn't exist or were invalid.
   */
  if (NULL == matcher) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "%s: neither WP_PLUGIN_DIR nor WP_CONTENT_DIR set",
                     __func__);

    matcher = nr_matcher_create();
    nr_matcher_add_prefix(matcher, "/wp-content/plugins");
  }

  NRPRG(wordpress_plugin_matcher) = matcher;
  return matcher;
}

static nr_matcher_t* nr_wordpress_theme_matcher() {
  nr_matcher_t* matcher = NULL;
  zval* roots = NULL;

  if (NRPRG(wordpress_theme_matcher)) {
    return NRPRG(wordpress_theme_matcher);
  }

  matcher = nr_matcher_create();

  /*
   * WordPress 2.9.0 and later include get_theme_roots(), which will give us
   * either a string with the single theme root (the normal case) or an array
   * of theme roots.
   */
  roots = nr_php_call(NULL, "get_theme_roots");
  if (nr_php_is_zval_valid_string(roots)) {
    nr_matcher_add_prefix(matcher, Z_STRVAL_P(roots));
  } else if (nr_php_is_zval_valid_array(roots)
             && (nr_php_zend_hash_num_elements(Z_ARRVAL_P(roots)) > 0)) {
    zval* path = NULL;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(roots), path) {
      if (nr_php_is_zval_valid_string(path)) {
        nr_matcher_add_prefix(matcher, Z_STRVAL_P(path));
      }
    }
    ZEND_HASH_FOREACH_END();
  } else {
    nr_matcher_add_prefix(matcher, "/wp-content/themes");
  }

  nr_php_zval_free(&roots);

  NRPRG(wordpress_theme_matcher) = matcher;
  return matcher;
}

char* nr_php_wordpress_plugin_match_matcher(const char* filename) {
  char* plugin = NULL;
  int plugin_len;
  plugin = nr_matcher_match_ex(nr_wordpress_plugin_matcher(), filename, nr_strlen(filename), &plugin_len);
  plugin = nr_wordpress_strip_php_suffix(plugin, plugin_len);
  nr_matcher_destroy(&NRPRG(wordpress_plugin_matcher));
  return plugin;
}

char* nr_php_wordpress_theme_match_matcher(const char* filename) {
  char* theme = NULL;
  int plugin_len;
  theme = nr_matcher_match_ex(nr_wordpress_theme_matcher(), filename, nr_strlen(filename), &plugin_len);
  theme = nr_wordpress_strip_php_suffix(theme, plugin_len);
  nr_matcher_destroy(&NRPRG(wordpress_theme_matcher));
  return theme;
}

char* nr_php_wordpress_core_match_matcher(const char* filename) {
  char* core = NULL;
  int plugin_len;
  core = nr_matcher_match_r_ex(nr_wordpress_core_matcher(), filename, nr_strlen(filename), &plugin_len);
  core = nr_wordpress_strip_php_suffix(core, plugin_len);
  nr_matcher_destroy(&NRPRG(wordpress_core_matcher));
  return core;
}

static void nr_wordpress_create_metric(nr_segment_t* segment,
                                       const char* prefix,
                                       const char* name) {
  char* metric_name = NULL;

  if (NULL == name) {
    return;
  }

  metric_name = nr_formatf("%s%s", prefix, name);
  nr_segment_add_metric(segment, metric_name, false);
  nr_free(metric_name);
}

static void free_wordpress_metadata(void* metadata) {
  nr_free(metadata);
}

static char* nr_wordpress_plugin_from_function(zend_function* func TSRMLS_DC) {
  const char* filename = NULL;
  size_t filename_len;
  char* plugin = NULL;
  int plugin_len;

  if (NULL == func) {
    return NULL;
  }

  filename = nr_php_function_filename(func);
  if (NULL == filename) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Wordpress: cannot determine plugin name:"
                     " missing filename, tag=" NRP_FMT,
                     NRP_PHP(NRPRG(wordpress_tag)));
    return NULL;
  }
  filename_len = nr_php_function_filename_len(func);

  if (NRPRG(wordpress_file_metadata)) {
    if (nr_hashmap_get_into(NRPRG(wordpress_file_metadata), filename,
                            filename_len, (void**)&plugin)) {
      nrl_verbosedebug(NRL_FRAMEWORK,
                       "Wordpress: found in cache: "
                       "plugin= %s and filename=" NRP_FMT,
                       NRSAFESTR(plugin), NRP_FILENAME(filename));
      return plugin;
    }
  } else {
    NRPRG(wordpress_file_metadata) = nr_hashmap_create(free_wordpress_metadata);
  }
  nrl_verbosedebug(NRL_FRAMEWORK,
                   "Wordpress: NOT found in cache: "
                   "filename=" NRP_FMT,
                   NRP_FILENAME(filename));
  plugin = nr_matcher_match_ex(nr_wordpress_plugin_matcher(), filename, filename_len, &plugin_len);
  plugin = nr_wordpress_strip_php_suffix(plugin, plugin_len);
  if (plugin) {
    goto cache_and_return;
  }

  plugin = nr_matcher_match_ex(nr_wordpress_theme_matcher(), filename, filename_len, &plugin_len);
  plugin = nr_wordpress_strip_php_suffix(plugin, plugin_len);
  if (plugin) {
    goto cache_and_return;
  }

  plugin = nr_matcher_match_r_ex(nr_wordpress_core_matcher(), filename, filename_len, &plugin_len);
  plugin = nr_wordpress_strip_php_suffix(plugin, plugin_len);
  if (plugin) {
    /*
     * The core wordpress functions are anonymized, so we don't need to return
     * the name of the php file, and we can release the plugin. Give a better
     * message, because this is not an error condition or unexpected format.
     */
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Wordpress: detected Wordpress Core filename, functions "
                     "will be anonymized:"
                     "tag=" NRP_FMT " filename=" NRP_FMT,
                     NRP_PHP(NRPRG(wordpress_tag)), NRP_FILENAME(filename));
    /* We can discard the plugin value, since these functions are anonymous. */
  } else {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Wordpress: cannot determine plugin name:"
                     " unexpected format, tag=" NRP_FMT " filename=" NRP_FMT,
                     NRP_PHP(NRPRG(wordpress_tag)), NRP_FILENAME(filename));
  }
  nr_free(plugin);

cache_and_return:
  /*
   * Even if plugin is NULL, we'll still cache that. Hooks in WordPress's core
   * will be NULL, and we need not re-run the matchers each time.
   */
  nr_hashmap_set(NRPRG(wordpress_file_metadata), filename, filename_len,
                 plugin);
  return plugin;
}

NR_PHP_WRAPPER(nr_wordpress_wrap_hook) {
#if ZEND_MODULE_API_NO < ZEND_7_4_X_API_NO
  zend_function* func = NULL;
#endif
  char* plugin = NULL;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  /*
   * We only want to hook the function being called if this is a WordPress
   * function, we're instrumenting hooks, and WordPress is currently executing
   * hooks (denoted by the wordpress_tag being set).
   */
  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_WORDPRESS);

  if ((0 == NRINI(wordpress_hooks)) || (NULL == NRPRG(wordpress_tag))) {
    NR_PHP_WRAPPER_LEAVE;
  }
#if ZEND_MODULE_API_NO < ZEND_7_4_X_API_NO
  func = nr_php_execute_function(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  plugin = nr_wordpress_plugin_from_function(func TSRMLS_CC);
#else
  plugin = wraprec->wordpress_plugin_theme;
#endif

  NR_PHP_WRAPPER_CALL;

  nr_wordpress_create_metric(auto_segment, NR_WORDPRESS_HOOK_PREFIX,
                             NRPRG(wordpress_tag));
  nr_wordpress_create_metric(auto_segment, NR_WORDPRESS_PLUGIN_PREFIX, plugin);
}
NR_PHP_WRAPPER_END

/*
 * PHP 7.3 and below uses old-style wraprec, and will use old style cufa calls.
 */
#if ZEND_MODULE_API_NO < ZEND_7_4_X_API_NO
static void nr_wordpress_call_user_func_array(zend_function* func,
                                              const zend_function* caller
                                                  NRUNUSED TSRMLS_DC) {
  const char* filename;
  /*
   * We only want to hook the function being called if this is a WordPress
   * function, we're instrumenting hooks, and WordPress is currently executing
   * hooks (denoted by the wordpress_tag being set).
   */
  if ((NR_FW_WORDPRESS != NRPRG(current_framework))
      || (0 == NRINI(wordpress_hooks)) || (NULL == NRPRG(wordpress_tag))) {
    return;
  }

  if (NRINI(wordpress_hooks_skip_filename)
      && 0 != nr_strlen(NRINI(wordpress_hooks_skip_filename))) {
    filename = nr_php_op_array_file_name(&func->op_array);

    if (nr_strstr(filename, NRINI(wordpress_hooks_skip_filename))) {
      nrl_verbosedebug(NRL_FRAMEWORK, "skipping hooks for function from %s",
                       filename);
      return;
    }
  }

  /*
   * We'll wrap this as a callable to handle anonymous functions being
   * registered.
   */
  nr_php_wrap_callable(func, nr_wordpress_wrap_hook TSRMLS_CC);
}
#endif /* PHP < 7.4 */

static void free_tag(void* tag) {
  nr_free(tag);
}

/*
 * Some plugins generate transient tag names. We can detect these by checking
 * the substrings returned from our regex rule. If the tag is transient, we
 * assemble a new name without the offending hex.
 *
 * Example: (old) add_option__transient_timeout_twccr_382402301f44c883bc0137_cat
 *          (new) add_option__transient_timeout_twccr_*_cat
 */
static char* nr_wordpress_clean_tag(const zval* tag) {
  char* clean_tag = NULL;
  nr_regex_t* regex = NULL;
  nr_regex_substrings_t* ss = NULL;

  if (0 == nr_php_is_zval_non_empty_string(tag)) {
    return NULL;
  }

  regex = wordpress_hook_regex;
  if (NULL == regex) {
    return NULL;
  }

  if (NULL == NRPRG(wordpress_clean_tag_cache)) {
    NRPRG(wordpress_clean_tag_cache) = nr_hashmap_create(free_tag);
  }

  if (nr_hashmap_get_into(NRPRG(wordpress_clean_tag_cache), Z_STRVAL_P(tag),
                          Z_STRLEN_P(tag), (void**)&clean_tag)) {
    return clean_tag;
  }

  ss = nr_regex_match_capture(regex, Z_STRVAL_P(tag), Z_STRLEN_P(tag));
  clean_tag = nr_regex_substrings_get(ss, 5);

  /*
   * If clean_tag is set, it means there was nothing to strip from the name, and
   * we return it. If it's NULL, we assemble a name from the other substrings.
   */
  if (NULL == clean_tag) {
    /* The offending hex is the substring at index 3. */
    char* transient_prefix = nr_regex_substrings_get(ss, 2);
    char* transient_suffix = nr_regex_substrings_get(ss, 4);

    if ((NULL != transient_prefix) && (NULL != transient_suffix)) {
      clean_tag = nr_formatf("%s*%s", transient_prefix, transient_suffix);
    }

    nr_free(transient_prefix);
    nr_free(transient_suffix);
  }

  nr_regex_substrings_destroy(&ss);

  nr_hashmap_set(NRPRG(wordpress_clean_tag_cache), Z_STRVAL_P(tag),
                 Z_STRLEN_P(tag), clean_tag);

  return clean_tag;
}

NR_PHP_WRAPPER(nr_wordpress_exec_handle_tag) {
  zval* tag = NULL;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  if (nrlikely(0 != NRINI(wordpress_hooks))) {
    NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_WORDPRESS);

    tag = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

    if (1 == nr_php_is_zval_non_empty_string(tag)) {
      /*
      * Our general approach here is to set the wordpress_tag global, then let
      * the call_user_func_array instrumentation take care of actually timing
      * the hooks by checking if it's set.
      */
      char* old_tag = NRPRG(wordpress_tag);

      NRPRG(check_cufa) = true;

      NRPRG(wordpress_tag) = nr_wordpress_clean_tag(tag);
      NR_PHP_WRAPPER_CALL;
      NRPRG(wordpress_tag) = old_tag;
      if (NULL == NRPRG(wordpress_tag)) {
        NRPRG(check_cufa) = false;
      }
    } else {
      NRPRG(check_cufa) = false;
      NR_PHP_WRAPPER_CALL;
    }

    nr_php_arg_release(&tag);
  }
}
NR_PHP_WRAPPER_END

/*
 * Determine the WT name from the Wordpress template. We look for the call to
 * apply_filters('template_include') (which is inside "template-loader.php") and
 * then use the result of that call (which is a template name) as the name of
 * the transaction. Usage: called from the user function execution hook:
 * nr_php_execute_enabled(op_array)
 */
static void nr_wordpress_name_the_wt(const zval* tag,
                                     zval** retval_ptr TSRMLS_DC) {
  int alen;
  char* action = NULL;
  char* shortened_action = NULL;
  char* s2 = NULL;

  if ((sizeof("template_include") - 1) != Z_STRLEN_P(tag)) {
    return;
  }
  if (0 != nr_strncmp("template_include", Z_STRVAL_P(tag), Z_STRLEN_P(tag))) {
    return;
  }
  if (NULL == retval_ptr) {
    return;
  }
  if (!nr_php_is_zval_non_empty_string(*retval_ptr)) {
    return;
  }

  alen = NRSAFELEN(Z_STRLEN_P(*retval_ptr));
  action = (char*)nr_alloca(alen + 1);

  nr_strxcpy(action, Z_STRVAL_P(*retval_ptr), alen);

  shortened_action = nr_strrchr(action, '/');
  if (NULL == shortened_action) {
    shortened_action = action;
  }

  s2 = nr_strrchr(shortened_action, '.'); /* trim .php extension */
  if (s2) {
    *s2 = '\0';
  }

  nr_txn_set_path("Wordpress", NRPRG(txn), shortened_action,
                  NR_PATH_TYPE_ACTION, NR_NOT_OK_TO_OVERWRITE);
}

/*
 * apply_filters() is special, since we're interested in it both for
 * WordPress hook/plugin metrics and for transaction naming.
 */
NR_PHP_WRAPPER(nr_wordpress_apply_filters) {
  zval* tag = NULL;
  zval** retval_ptr = nr_php_get_return_value_ptr(TSRMLS_C);

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_WORDPRESS);

  tag = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  if (1 == nr_php_is_zval_non_empty_string(tag)) {
    if (0 != NRINI(wordpress_hooks)) {
      /*
       * Our general approach here is to set the wordpress_tag global, then let
       * the call_user_func_array instrumentation take care of actually timing
       * the hooks by checking if it's set.
       */
      char* old_tag = NRPRG(wordpress_tag);

      NRPRG(check_cufa) = true;

      NRPRG(wordpress_tag) = nr_wordpress_clean_tag(tag);

      NR_PHP_WRAPPER_CALL;
      NRPRG(wordpress_tag) = old_tag;
      if (NULL == NRPRG(wordpress_tag)) {
        NRPRG(check_cufa) = false;
      }
    } else {
      NRPRG(check_cufa) = false;
      NR_PHP_WRAPPER_CALL;
    }

    nr_wordpress_name_the_wt(tag, retval_ptr TSRMLS_CC);
  } else {
    NR_PHP_WRAPPER_CALL;
  }

  nr_php_arg_release(&tag);
}
NR_PHP_WRAPPER_END

#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO
/*
 * Wrap the wordpress function add_filter
 *
 * function add_filter( $hook_name, $callback, $priority = 10, $accepted_args = 1 )
 *
 * @param string   $hook_name     The name of the filter to add the callback to.
 * @param callable $callback      The callback to be run when the filter is applied.
 * @param int      $priority      Optional. Used to specify the order in which the functions
 *                                associated with a particular filter are executed.
 *                                Lower numbers correspond with earlier execution,
 *                                and functions with the same priority are executed
 *                                in the order in which they were added to the filter. Default 10.
 * @param int      $accepted_args Optional. The number of arguments the function accepts. Default 1.
 * @return true Always returns true.
 */

NR_PHP_WRAPPER(nr_wordpress_add_filter) {
  /* Wordpress's add_action() is just a wrapper around add_filter(),
   * so we only need to instrument this function */
  NR_UNUSED_SPECIALFN;
  (void)wraprec;
  const char* filename = NULL;
  bool wrap_hook = true;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_WORDPRESS);

  /*
   * We only want to hook the function being called if this is a WordPress
   * function, we're instrumenting hooks, and WordPress is currently executing
   * hooks (denoted by the wordpress_tag being set).
   */
  if ((NR_FW_WORDPRESS != NRPRG(current_framework))
      || (0 == NRINI(wordpress_hooks))) {
    wrap_hook = false;
  }

  if (NRINI(wordpress_hooks_skip_filename)
      && 0 != nr_strlen(NRINI(wordpress_hooks_skip_filename))) {
    filename = nr_php_op_array_file_name(NR_OP_ARRAY);

    if (nr_strstr(filename, NRINI(wordpress_hooks_skip_filename))) {
      nrl_verbosedebug(NRL_FRAMEWORK, "skipping hooks for function from %s",
                       filename);
      wrap_hook = false;
    }
  }

  if (true == wrap_hook) {
    nruserfn_t* callback_wraprec;
    zval* callback = nr_php_arg_get(2, NR_EXECUTE_ORIG_ARGS);
    /* the callback here can be any PHP callable. nr_php_wrap_generic_callable
   * checks that a valid callable is passed */
    callback_wraprec = nr_php_wrap_generic_callable(callback, nr_wordpress_wrap_hook);

    // We can cheat here: wraprecs on callables are always transient, so if
    // there's a wordpress_plugin_theme set we know it's from this transaction,
    // and we don't have any issues around a possible multi-tenant setup.
    if (callback_wraprec && NULL == callback_wraprec->wordpress_plugin_theme) {
      // Unlike Drupal, we don't free the wordpress_plugin_theme, since we know
      // it's transient anyway, and we only set the field if it was previously
      // NULL.
      zend_function* fn = nr_php_zval_to_function(callback);
      callback_wraprec->wordpress_plugin_theme
          = nr_wordpress_plugin_from_function(fn);
    }
    nr_php_arg_release(&callback);
  }
}
NR_PHP_WRAPPER_END
#endif /* PHP 7.4+ */

void nr_wordpress_enable(TSRMLS_D) {
  nr_php_wrap_user_function(NR_PSTR("apply_filters"),
                            nr_wordpress_apply_filters TSRMLS_CC);

  if (0 != NRINI(wordpress_hooks)) {
    nr_php_wrap_user_function(NR_PSTR("apply_filters_ref_array"),
                              nr_wordpress_exec_handle_tag TSRMLS_CC);

    nr_php_wrap_user_function(NR_PSTR("do_action"),
                              nr_wordpress_exec_handle_tag TSRMLS_CC);

    nr_php_wrap_user_function(NR_PSTR("do_action_ref_array"),
                              nr_wordpress_exec_handle_tag TSRMLS_CC);

#if ZEND_MODULE_API_NO < ZEND_7_4_X_API_NO
    nr_php_add_call_user_func_array_pre_callback(
        nr_wordpress_call_user_func_array TSRMLS_CC);
#else
    nr_php_wrap_user_function(NR_PSTR("add_filter"),
                                nr_wordpress_add_filter);
#endif
  }
}

void nr_wordpress_minit(void) {
  wordpress_hook_regex = nr_regex_create(
      "(^([a-z_-]+[_-])([0-9a-f_.]+[0-9][0-9a-f.]+)(_{0,1}.*)$|(.*))",
      NR_REGEX_CASELESS, 0);
}

void nr_wordpress_mshutdown(void) {
  nr_regex_destroy(&wordpress_hook_regex);
}
