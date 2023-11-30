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
  plugin = nr_matcher_match(nr_wordpress_plugin_matcher(), filename);
  plugin = nr_file_basename(plugin, nr_strlen(plugin));
  nr_matcher_destroy(&NRPRG(wordpress_plugin_matcher));
  return plugin;
}

char* nr_php_wordpress_theme_match_matcher(const char* filename) {
  char* theme = NULL;
  theme = nr_matcher_match(nr_wordpress_theme_matcher(), filename);
  theme = nr_file_basename(theme, nr_strlen(theme));
  nr_matcher_destroy(&NRPRG(wordpress_theme_matcher));
  return theme;
}

char* nr_php_wordpress_core_match_matcher(const char* filename) {
  char* core = NULL;
  core = nr_matcher_match_core(nr_wordpress_core_matcher(), filename);
  core = nr_file_basename(core, nr_strlen(core));
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
  filename_len = nr_strlen(filename);

  if (NRPRG(wordpress_file_metadata)) {
    if (nr_hashmap_get_into(NRPRG(wordpress_file_metadata), filename,
                            filename_len, (void**)&plugin)) {
      nrl_verbosedebug(NRL_FRAMEWORK,
                       "Wordpress: found in cache: "
                       "plugin= %s and filename=" NRP_FMT,
                       plugin, NRP_FILENAME(filename));
      return plugin;
    }
  } else {
    NRPRG(wordpress_file_metadata) = nr_hashmap_create(free_wordpress_metadata);
  }
  nrl_verbosedebug(NRL_FRAMEWORK,
                   "Wordpress: NOT found in cache: "
                   "filename=" NRP_FMT,
                   NRP_FILENAME(filename));
  plugin = nr_matcher_match(nr_wordpress_plugin_matcher(), filename);
  plugin = nr_file_basename(plugin, nr_strlen(plugin));
  if (plugin) {
    goto cache_and_return;
  }

  plugin = nr_matcher_match(nr_wordpress_theme_matcher(), filename);
  plugin = nr_file_basename(plugin, nr_strlen(plugin));
  if (plugin) {
    goto cache_and_return;
  }

  plugin = nr_matcher_match_core(nr_wordpress_core_matcher(), filename);
  plugin = nr_file_basename(plugin, nr_strlen(plugin));
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
  zend_function* func = NULL;
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
  func = nr_php_execute_function(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  plugin = nr_wordpress_plugin_from_function(func TSRMLS_CC);

  NR_PHP_WRAPPER_CALL;

  nr_wordpress_create_metric(auto_segment, NR_WORDPRESS_HOOK_PREFIX,
                             NRPRG(wordpress_tag));
  nr_wordpress_create_metric(auto_segment, NR_WORDPRESS_PLUGIN_PREFIX, plugin);
}
NR_PHP_WRAPPER_END

/*
 * A call_user_func_array() callback to ensure that we wrap each hook function.
 */
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

/*
 * Some plugins generate transient tag names. We can detect these by checking
 * the substrings returned from our regex rule. If the tag is transient, we
 * assemble a new name without the offending hex.
 *
 * Example: (old) add_option__transient_timeout_twccr_382402301f44c883bc0137_cat
 *          (new) add_option__transient_timeout_twccr_*_cat
 */
static char* nr_wordpress_clean_tag(const zval* tag TSRMLS_DC) {
  char* orig_tag = NULL;
  char* clean_tag = NULL;
  nr_regex_t* regex = NULL;
  nr_regex_substrings_t* ss = NULL;

  if (0 == nr_php_is_zval_non_empty_string(tag)) {
    return NULL;
  }

  regex = NRPRG(wordpress_hook_regex);
  if (NULL == regex) {
    return NULL;
  }

  orig_tag = nr_strndup(Z_STRVAL_P(tag), Z_STRLEN_P(tag));
  ss = nr_regex_match_capture(regex, orig_tag, nr_strlen(orig_tag));
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
  nr_free(orig_tag);

  return clean_tag;
}

NR_PHP_WRAPPER(nr_wordpress_exec_handle_tag) {
  zval* tag = NULL;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_WORDPRESS);

  tag = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  if (1 == nr_php_is_zval_non_empty_string(tag)
      || (0 != NRINI(wordpress_hooks))) {
    /*
     * Our general approach here is to set the wordpress_tag global, then let
     * the call_user_func_array instrumentation take care of actually timing
     * the hooks by checking if it's set.
     */
    char* old_tag = NRPRG(wordpress_tag);

    NRPRG(wordpress_tag) = nr_wordpress_clean_tag(tag TSRMLS_CC);
    NR_PHP_WRAPPER_CALL;
    nr_free(NRPRG(wordpress_tag));
    NRPRG(wordpress_tag) = old_tag;
  } else {
    NR_PHP_WRAPPER_CALL;
  }

  nr_php_arg_release(&tag);
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

      NRPRG(wordpress_tag) = nr_wordpress_clean_tag(tag TSRMLS_CC);
      NR_PHP_WRAPPER_CALL;
      nr_free(NRPRG(wordpress_tag));
      NRPRG(wordpress_tag) = old_tag;
    } else {
      NR_PHP_WRAPPER_CALL;
    }

    nr_wordpress_name_the_wt(tag, retval_ptr TSRMLS_CC);
  } else {
    NR_PHP_WRAPPER_CALL;
  }

  nr_php_arg_release(&tag);
}
NR_PHP_WRAPPER_END

void nr_wordpress_enable(TSRMLS_D) {
  nr_php_wrap_user_function(NR_PSTR("apply_filters"),
                            nr_wordpress_apply_filters TSRMLS_CC);

  nr_php_wrap_user_function(NR_PSTR("apply_filters_ref_array"),
                            nr_wordpress_exec_handle_tag TSRMLS_CC);

  nr_php_wrap_user_function(NR_PSTR("do_action"),
                            nr_wordpress_exec_handle_tag TSRMLS_CC);

  nr_php_wrap_user_function(NR_PSTR("do_action_ref_array"),
                            nr_wordpress_exec_handle_tag TSRMLS_CC);

  nr_php_add_call_user_func_array_pre_callback(
      nr_wordpress_call_user_func_array TSRMLS_CC);
}
