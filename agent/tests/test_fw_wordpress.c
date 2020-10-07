/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

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
#include "fw_drupal_common.h"
#include "fw_wordpress.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

/*
 * This will test whether the regular expression checking works to determine
 * the name of a plugin from a filename when the "plugin" is a .php file.
 */
static void test_wordpress_core_regex(TSRMLS_D) {
  char* plugin = NULL;

  /* Test with invalid input. */
  plugin = nr_php_wordpress_core_match_regex(NULL TSRMLS_CC);
  tlib_pass_if_null(
      "Wordpress function regex matching should return NULL when given NULL.",
      plugin);

  plugin = nr_php_wordpress_core_match_regex(
      "/wp-content/plugins/affiliatelite.php" TSRMLS_CC);
  tlib_pass_if_null(
      "wordpress core regex matching should not work from the regular plugins "
      "directory",
      plugin);

  plugin = nr_php_wordpress_core_match_regex(
      "/www-data/premium.wpmudev.org/wp-content/affiliatelite.php" TSRMLS_CC);
  tlib_pass_if_null(
      "wordpress core regex matching should not work from a non-standard "
      "directory.",
      plugin);

  /* Test with valid input. */

  plugin = nr_php_wordpress_core_match_regex(
      "/wordpress/wordpress/wp-includes/query.php" TSRMLS_CC);
  tlib_pass_if_not_null(
      "wordpress core regex matching should work from a standard "
      "directory.",
      plugin);
  tlib_pass_if_str_equal(
      "wordpress core regex matching should work from a standard "
      "directory.",
      "query", plugin);

  nr_free(plugin);

  plugin = nr_php_wordpress_core_match_regex(
      "/wordpress/wordpress/wp-includes/block/query.php" TSRMLS_CC);
  tlib_pass_if_not_null(
      "wordpress core regex matching should work from a standard "
      "directory with a subdirectory",
      plugin);
  tlib_pass_if_str_equal(
      "wordpress core regex matching should work from a standard "
      "directory with a subdirectory.",
      "query", plugin);

  nr_free(plugin);
}

/*
 * This will test whether the regular expression checking works to determine
 * the name of a plugin from a filename when the plugin is not a .php file
 */
static void test_wordpress_plugin_regex(TSRMLS_D) {
  char* plugin = NULL;
  char* filename
      = "/www-data/premium.wpmudev.org/wp-content/plugins/plugin/"
        "affiliatelite.php";

  /* Test with invalid input. */
  plugin = nr_php_wordpress_plugin_match_regex(NULL TSRMLS_CC);
  tlib_pass_if_null(
      "Wordpress plugin regex should return NULL when given NULL.", plugin);

  plugin = nr_php_wordpress_plugin_match_regex(
      "/wp-content/affiliatelite.php" TSRMLS_CC);
  tlib_pass_if_null(
      "Wordpress plugin regex should return NULL if the filename is not in the "
      "correct plugin directory.",
      plugin);

  /* Test with valid input. */

  plugin = nr_php_wordpress_plugin_match_regex(
      "/wp-content/plugins/affiliatelite.php" TSRMLS_CC);
  tlib_pass_if_not_null(
      "Wordpress plugin regex should return plugin name even if the plugin is "
      "a function "
      "not a directory.",
      plugin);
  tlib_pass_if_str_equal("Wordpress plugin regex should work.", "affiliatelite",
                         plugin);

  nr_free(plugin);

  plugin = nr_php_wordpress_plugin_match_regex(filename TSRMLS_CC);
  tlib_pass_if_not_null("Wordpress plugin regex should work.", plugin);
  tlib_pass_if_str_equal("Wordpress plugin regex should work.", "plugin",
                         plugin);
  nr_free(plugin);
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */
  tlib_php_engine_create("" PTSRMLS_CC);
  test_wordpress_plugin_regex(TSRMLS_C);
  test_wordpress_core_regex(TSRMLS_C);
  tlib_php_engine_destroy(TSRMLS_C);
}
