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
 * This will test whether the matcher checking works to determine
 * the name of a plugin from a filename when the "plugin" is a .php file.
 */
static void test_wordpress_core_matcher() {
  char* plugin = NULL;

  /* Test with invalid input. */
  plugin = nr_php_wordpress_core_match_matcher(NULL);
  tlib_pass_if_null(
      "Wordpress function matcher matching should return NULL when given NULL.",
      plugin);

  plugin = nr_php_wordpress_core_match_matcher(
      "/wp-content/plugins/affiliatelite.php");
  tlib_pass_if_null(
      "wordpress core matcher matching should not work from the regular plugins "
      "directory",
      plugin);

  plugin = nr_php_wordpress_core_match_matcher(
      "/www-data/premium.wpmudev.org/wp-content/affiliatelite.php");
  tlib_pass_if_null(
      "wordpress core matcher matching should not work from a non-standard "
      "directory.",
      plugin);

  /* Test with valid input. */

  plugin = nr_php_wordpress_core_match_matcher(
      "/wordpress/wordpress/wp-includes/query.php");
  tlib_pass_if_not_null(
      "wordpress core matcher matching should work from a standard "
      "directory.",
      plugin);
  tlib_pass_if_str_equal(
      "wordpress core matcher matching should work from a standard "
      "directory.",
      "query", plugin);

  nr_free(plugin);

  plugin = nr_php_wordpress_core_match_matcher(
      "/wordpress/wordpress/wp-includes/block/query.php");
  tlib_pass_if_not_null(
      "wordpress core matcher matching should work from a standard "
      "directory with a subdirectory",
      plugin);
  tlib_pass_if_str_equal(
      "wordpress core matcher matching should work from a standard "
      "directory with a subdirectory.",
      "query", plugin);

  nr_free(plugin);
}

/*
 * This will test whether the matcher checking works to determine
 * the name of a plugin from a filename when the plugin is not a .php file
 */
static void test_wordpress_plugin_matcher() {
  char* plugin = NULL;
  char* filename
      = "/www-data/premium.wpmudev.org/wp-content/plugins/plugin/"
        "affiliatelite.php";

  /* Test with invalid input. */
  plugin = nr_php_wordpress_plugin_match_matcher(NULL);
  tlib_pass_if_null(
      "Wordpress plugin matcher should return NULL when given NULL.", plugin);

  plugin = nr_php_wordpress_plugin_match_matcher(
      "/wp-content/affiliatelite.php");
  tlib_pass_if_null(
      "Wordpress plugin matcher should return NULL if the filename is not in the "
      "correct plugin directory.",
      plugin);

  /* Test with valid input. */

  plugin = nr_php_wordpress_plugin_match_matcher(
      "/wp-content/plugins/affiliatelite.php");
  tlib_pass_if_not_null(
      "Wordpress plugin matcher should return plugin name even if the plugin is "
      "a function "
      "not a directory.",
      plugin);
  tlib_pass_if_str_equal("Wordpress plugin matcher should work.", "affiliatelite",
                         plugin);

  nr_free(plugin);

  plugin = nr_php_wordpress_plugin_match_matcher(filename);
  tlib_pass_if_not_null("Wordpress plugin matcher should work.", plugin);
  tlib_pass_if_str_equal("Wordpress plugin matcher should work.", "plugin",
                         plugin);
  nr_free(plugin);
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */
  tlib_php_engine_create("" PTSRMLS_CC);
  test_wordpress_plugin_matcher();
  test_wordpress_core_matcher();
  tlib_php_engine_destroy(TSRMLS_C);
}
