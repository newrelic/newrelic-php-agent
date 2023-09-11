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
#include "util_memory.h"
#include "util_regex.h"
#include "util_strings.h"
#include "fw_wordpress.h"

#define NR_WORDPRESS_HOOK_PREFIX "Framework/WordPress/Hook/"
#define NR_WORDPRESS_PLUGIN_PREFIX "Framework/WordPress/Plugin/"

static size_t zval_len_without_trailing_slash(const zval* zstr) {
  nr_string_len_t len = Z_STRLEN_P(zstr);
  const char* str = Z_STRVAL_P(zstr);

  if ((len > 0) && ('/' == str[len - 1])) {
    len -= 1;
  }

  return (size_t)len;
}

static void add_wildcard_path_component(nrbuf_t* buf) {
  nr_buffer_add(buf, NR_PSTR("/(.*?)/|plugins/([^/]*)[.]php$"));
}

static nr_regex_t* compile_regex_for_path(const zval* path) {
  nrbuf_t* buf = nr_buffer_create(2 * Z_STRLEN_P(path), 0);
  nr_regex_t* regex = NULL;

  nr_regex_add_quoted_to_buffer(buf, Z_STRVAL_P(path),
                                zval_len_without_trailing_slash(path));
  add_wildcard_path_component(buf);
  nr_buffer_add(buf, NR_PSTR("\0"));
  regex = nr_regex_create(nr_buffer_cptr(buf), NR_REGEX_CASELESS, 1);

  nr_buffer_destroy(&buf);
  return regex;
}

static nr_regex_t* compile_regex_for_path_array(const zval* paths) {
  nrbuf_t* buf = nr_buffer_create(0, 0);
  int first = 1;
  zval* path = NULL;
  nr_regex_t* regex = NULL;

  nr_buffer_add(buf, NR_PSTR("("));
  ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(paths), path) {
    if (!nr_php_is_zval_valid_string(path)) {
      nrl_verbosedebug(NRL_FRAMEWORK,
                       "%s: unexpected non-string value in path array",
                       __func__);
    } else {
      if (first) {
        first = 0;
      } else {
        nr_buffer_add(buf, NR_PSTR("|"));
      }

      nr_regex_add_quoted_to_buffer(buf, Z_STRVAL_P(path),
                                    zval_len_without_trailing_slash(path));
    }
  }
  ZEND_HASH_FOREACH_END();
  nr_buffer_add(buf, NR_PSTR(")"));
  add_wildcard_path_component(buf);
  nr_buffer_add(buf, NR_PSTR("\0"));

  /*
   * If the first flag is still set, no valid theme paths existed and we
   * should abort.
   */
  if (first) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: no valid elements in the path array",
                     __func__);
  } else {
    regex = nr_regex_create(nr_buffer_cptr(buf), NR_REGEX_CASELESS, 1);
  }

  nr_buffer_destroy(&buf);
  return regex;
}

static nr_regex_t* compile_regex_for_constant(const char* constant,
                                              const char* suffix TSRMLS_DC) {
  nr_regex_t* regex = NULL;
  zval* value = nr_php_get_constant(constant TSRMLS_CC);

  if (nr_php_is_zval_valid_string(value)) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Wordpress: found value = %s for constant=%s",
                     Z_STRVAL_P(value), constant);

    nrbuf_t* buf = nr_buffer_create(2 * Z_STRLEN_P(value), 0);

    nr_regex_add_quoted_to_buffer(buf, Z_STRVAL_P(value),
                                  zval_len_without_trailing_slash(value));
    nr_buffer_add(buf, suffix, nr_strlen(suffix));
    add_wildcard_path_component(buf);
    nr_buffer_add(buf, NR_PSTR("\0"));

    regex = nr_regex_create(nr_buffer_cptr(buf), NR_REGEX_CASELESS, 1);

    nr_buffer_destroy(&buf);
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
  return regex;
}

static char* try_match_regex(const nr_regex_t* regex, const char* filename) {
  char* plugin = NULL;
  nr_regex_substrings_t* ss = NULL;

  ss = nr_regex_match_capture(regex, filename, nr_strlen(filename));
  if (NULL == ss) {
    return NULL;
  }

  /*
   * The last substring will be the plugin or theme name.
   */
  plugin = nr_regex_substrings_get(ss, nr_regex_substrings_count(ss));
  nr_regex_substrings_destroy(&ss);

  return plugin;
}

static const nr_regex_t* nr_wordpress_core_regex(TSRMLS_D) {
  nr_regex_t* regex = NULL;

  if (NRPRG(wordpress_core_regex)) {
    return NRPRG(wordpress_core_regex);
  }

  /*
   * This will get all the Wordpress core functions located in the `wp-includes`
   * (or a subdirectory off of that directory) directory.
   */

  regex
      = nr_regex_create("wp-includes.*?/([^/]*)[.]php$", NR_REGEX_CASELESS, 1);

  NRPRG(wordpress_core_regex) = regex;
  return regex;
}

static const nr_regex_t* nr_wordpress_plugin_regex(TSRMLS_D) {
  nr_regex_t* regex = NULL;

  if (NRPRG(wordpress_plugin_regex)) {
    return NRPRG(wordpress_plugin_regex);
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

  regex = compile_regex_for_constant("WP_PLUGIN_DIR", "" TSRMLS_CC);
  if (!regex) {
    regex = compile_regex_for_constant("WP_CONTENT_DIR", "/plugins" TSRMLS_CC);
  }

  /*
   * Fallback if the constants didn't exist or were invalid.
   */
  if (NULL == regex) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "%s: neither WP_PLUGIN_DIR nor WP_CONTENT_DIR set",
                     __func__);

    regex = nr_regex_create("plugins/(.*?)/|plugins/([^/]*)[.]php$",
                            NR_REGEX_CASELESS, 1);
  }

  NRPRG(wordpress_plugin_regex) = regex;
  return regex;
}

static const nr_regex_t* nr_wordpress_theme_regex(TSRMLS_D) {
  nr_regex_t* regex = NULL;
  zval* roots = NULL;

  if (NRPRG(wordpress_theme_regex)) {
    return NRPRG(wordpress_theme_regex);
  }

  /*
   * WordPress 2.9.0 and later include get_theme_roots(), which will give us
   * either a string with the single theme root (the normal case) or an array
   * of theme roots.
   */
  roots = nr_php_call(NULL, "get_theme_roots");
  if (nr_php_is_zval_valid_string(roots)) {
    regex = compile_regex_for_path(roots);
  } else if (nr_php_is_zval_valid_array(roots)
             && (nr_php_zend_hash_num_elements(Z_ARRVAL_P(roots)) > 0)) {
    regex = compile_regex_for_path_array(roots);
  }
  nr_php_zval_free(&roots);

  /*
   * Fallback path if get_theme_roots() failed to give us anything useful.
   */
  if (NULL == regex) {
    regex = nr_regex_create("/wp-content/themes/(.*?)/", NR_REGEX_CASELESS, 1);
  }

  NRPRG(wordpress_theme_regex) = regex;
  return regex;
}

char* nr_php_wordpress_plugin_match_regex(const char* filename TSRMLS_DC) {
  char* plugin = NULL;
  plugin = try_match_regex(nr_wordpress_plugin_regex(TSRMLS_C), filename);
  nr_regex_destroy(&NRPRG(wordpress_plugin_regex));
  return plugin;
}

char* nr_php_wordpress_core_match_regex(const char* filename TSRMLS_DC) {
  char* plugin = NULL;
  plugin = try_match_regex(nr_wordpress_core_regex(TSRMLS_C), filename);
  nr_regex_destroy(&NRPRG(wordpress_core_regex));
  return plugin;
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
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
                     NRP_PHP((char*)nr_stack_get_top(&NRPRG(wordpress_tags)))
#else
                     NRP_PHP(NRPRG(wordpress_tag))
#endif /* OAPI */
    );
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
  plugin = try_match_regex(nr_wordpress_plugin_regex(TSRMLS_C), filename);
  if (plugin) {
    goto cache_and_return;
  }

  plugin = try_match_regex(nr_wordpress_theme_regex(TSRMLS_C), filename);
  if (plugin) {
    goto cache_and_return;
  }

  plugin = try_match_regex(nr_wordpress_core_regex(TSRMLS_C), filename);
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
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
                     NRP_PHP((char*)nr_stack_get_top(&NRPRG(wordpress_tags))),
#else
                     NRP_PHP(NRPRG(wordpress_tag)),
#endif /* OAPI */
                     NRP_FILENAME(filename));
    /* We can discard the plugin value, since these functions are anonymous. */
  } else {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Wordpress: cannot determine plugin name:"
                     " unexpected format, tag=" NRP_FMT " filename=" NRP_FMT,
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
                     NRP_PHP((char*)nr_stack_get_top(&NRPRG(wordpress_tags))),
#else
                     NRP_PHP(NRPRG(wordpress_tag)),
#endif /* OAPI */
                     NRP_FILENAME(filename));
  }
  nr_free(plugin);

cache_and_return:
  /*
   * Even if plugin is NULL, we'll still cache that. Hooks in WordPress's core
   * will be NULL, and we need not re-run the regexes each time.
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

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  char* tag = nr_stack_get_top(&NRPRG(wordpress_tags));
  if ((0 == NRINI(wordpress_hooks)) || (NULL == tag)) {
#else
  if ((0 == NRINI(wordpress_hooks)) || (NULL == NRPRG(wordpress_tag))) {
#endif /* OAPI */
    NR_PHP_WRAPPER_LEAVE;
  }
  func = nr_php_execute_function(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  plugin = nr_wordpress_plugin_from_function(func TSRMLS_CC);

  NR_PHP_WRAPPER_CALL;

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_wordpress_create_metric(auto_segment, NR_WORDPRESS_HOOK_PREFIX, tag);
#else
  nr_wordpress_create_metric(auto_segment, NR_WORDPRESS_HOOK_PREFIX,
                             NRPRG(wordpress_tag));
#endif /* OAPI */
  nr_wordpress_create_metric(auto_segment, NR_WORDPRESS_PLUGIN_PREFIX, plugin);
}
NR_PHP_WRAPPER_END

#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO \
    || defined OVERWRITE_ZEND_EXECUTE_DATA
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
#endif /* not OAPI */

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

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  /* We will ignore the global wordpress_tag_states stack if wodpress_hooks is
   * off */
  if (0 != NRINI(wordpress_hooks)) {
    if (1 == nr_php_is_zval_non_empty_string(tag)) {
      /*
       * Our general approach here is to set the wordpress_tag global, then let
       * the call_user_func_array instrumentation take care of actually timing
       * the hooks by checking if it's set.
       */
      NRPRG(check_cufa) = true;
      nr_stack_push(&NRPRG(wordpress_tags),
                    nr_wordpress_clean_tag(tag TSRMLS_CC));
      nr_stack_push(&NRPRG(wordpress_tag_states), (void*)!NULL);
    } else {
      nr_stack_push(&NRPRG(wordpress_tag_states), NULL);
    }
  }
#else
  if (1 == nr_php_is_zval_non_empty_string(tag)
      && (0 != NRINI(wordpress_hooks))) {
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
#endif /* OAPI */

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
 *
 * * txn naming scheme:
 * In this case, `nr_txn_set_path` is called after `NR_PHP_WRAPPER_CALL` with
 * `NR_NOT_OK_TO_OVERWRITE`.  There is an explicit after function
 * `nr_wordpress_apply_filters_after`. This entails that the last wrapped call
 * gets to name the txn.
 */
NR_PHP_WRAPPER(nr_wordpress_apply_filters) {
  zval* tag = NULL;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_WORDPRESS);

  tag = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  /* We will ignore the global wordpress_tag_states stack if wodpress_hooks is
   * off */
  if (0 != NRINI(wordpress_hooks)) {
    if (1 == nr_php_is_zval_non_empty_string(tag)) {
      /*
       * Our general approach here is to set the wordpress_tag global, then let
       * the call_user_func_array instrumentation take care of actually timing
       * the hooks by checking if it's set.
       */
      NRPRG(check_cufa) = true;
      nr_stack_push(&NRPRG(wordpress_tags),
                    nr_wordpress_clean_tag(tag TSRMLS_CC));
      nr_stack_push(&NRPRG(wordpress_tag_states), (void*)!NULL);
    } else {
      // Keep track of whether we pushed to NRPRG(wordpress_tags)
      nr_stack_push(&NRPRG(wordpress_tag_states), NULL);
    }
  }
#else
  zval** retval_ptr = NR_GET_RETURN_VALUE_PTR;
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
#endif /* OAPI */

  nr_php_arg_release(&tag);
}
NR_PHP_WRAPPER_END

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
static void clean_wordpress_tag_stack() {
  if ((bool)nr_stack_pop(&NRPRG(wordpress_tag_states))) {
    char* cleaned_tag = nr_stack_pop(&NRPRG(wordpress_tags));
    nr_free(cleaned_tag);
  }
  if (nr_stack_is_empty(&NRPRG(wordpress_tags))) {
    NRPRG(check_cufa) = false;
  }
}

NR_PHP_WRAPPER(nr_wordpress_handle_tag_stack_after) {
  (void)wraprec;
  if (0 != NRINI(wordpress_hooks)) {
    clean_wordpress_tag_stack();
  }
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_wordpress_handle_tag_stack_clean) {
  NR_UNUSED_SPECIALFN;
  NR_UNUSED_FUNC_RETURN_VALUE;
  (void)wraprec;
  if (0 != NRINI(wordpress_hooks)) {
    clean_wordpress_tag_stack();
  }
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_wordpress_apply_filters_after) {
  /* using nr_php_get_user_func_arg() so that we don't perform another copy
   * when all we want to do is check the string length */
  zval* tag = nr_php_get_user_func_arg(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (1 == nr_php_is_zval_non_empty_string(tag)) {
    zval** retval_ptr = NR_GET_RETURN_VALUE_PTR;
    nr_wordpress_name_the_wt(tag, retval_ptr TSRMLS_CC);
  }

  nr_wordpress_handle_tag_stack_after(NR_SPECIALFNPTR_ORIG_ARGS);
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_wordpress_add_action_filter) {
  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_WORDPRESS);

  zval* callback = nr_php_arg_get(2, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  /* the callback here can be any PHP callable. nr_php_wrap_generic_callable
   * checks that a valid callable is passed */
  nr_php_wrap_generic_callable(callback, nr_wordpress_wrap_hook);
  nr_php_arg_release(&callback);
}
NR_PHP_WRAPPER_END

#endif /* OAPI */

void nr_wordpress_enable(TSRMLS_D) {
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_php_wrap_user_function_before_after_clean(
      NR_PSTR("apply_filters"), nr_wordpress_apply_filters,
      nr_wordpress_apply_filters_after, nr_wordpress_handle_tag_stack_clean);

  nr_php_wrap_user_function_before_after_clean(
      NR_PSTR("apply_filters_ref_array"), nr_wordpress_exec_handle_tag,
      nr_wordpress_handle_tag_stack_after, nr_wordpress_handle_tag_stack_clean);

  nr_php_wrap_user_function_before_after_clean(
      NR_PSTR("do_action"), nr_wordpress_exec_handle_tag,
      nr_wordpress_handle_tag_stack_after, nr_wordpress_handle_tag_stack_clean);

  nr_php_wrap_user_function_before_after_clean(
      NR_PSTR("do_action_ref_array"), nr_wordpress_exec_handle_tag,
      nr_wordpress_handle_tag_stack_after, nr_wordpress_handle_tag_stack_clean);

  nr_php_wrap_user_function(NR_PSTR("add_action"),
                            nr_wordpress_add_action_filter);
  nr_php_wrap_user_function(NR_PSTR("add_filter"),
                            nr_wordpress_add_action_filter);

#else
  nr_php_wrap_user_function(NR_PSTR("apply_filters"),
                            nr_wordpress_apply_filters TSRMLS_CC);

  nr_php_wrap_user_function(NR_PSTR("apply_filters_ref_array"),
                            nr_wordpress_exec_handle_tag TSRMLS_CC);

  nr_php_wrap_user_function(NR_PSTR("do_action"),
                            nr_wordpress_exec_handle_tag TSRMLS_CC);

  nr_php_wrap_user_function(NR_PSTR("do_action_ref_array"),
                            nr_wordpress_exec_handle_tag TSRMLS_CC);

  if (0 != NRINI(wordpress_hooks)) {
    nr_php_add_call_user_func_array_pre_callback(
        nr_wordpress_call_user_func_array TSRMLS_CC);
  }
#endif /* OAPI */
}
