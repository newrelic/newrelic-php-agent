/*
 * Copyright 2025 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_globals.h"
#include "php_user_instrument_wraprec_hashmap.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

// clang-format off

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO

#define SCOPE_NAME "Vendor\\Namespace\\ClassName"
#define METHOD_NAME "getSomething"
#define SCOPED_METHOD_NAME SCOPE_NAME "::" METHOD_NAME
#define FUNCTION_NAME "global_function"

static void test_wraprecs_hashmap() {
  nruserfn_t *wraprec, *found_wraprec;
  zend_string *func_name, *scope_name, *method_name;

  func_name = zend_string_init(NR_PSTR(FUNCTION_NAME), 0);
  scope_name = zend_string_init(NR_PSTR(SCOPE_NAME), 0);
  method_name = zend_string_init(NR_PSTR(METHOD_NAME), 0);

  // Non-ZTS: wraprec hashmap is initialized at MINIT.
  // ZTS: wraprec hashmap is initialized per-request at RINIT.
  // Destroy it here to test agent's behavior when it is not initialized.
  nr_php_user_instrument_wraprec_hashmap_destroy();

  // Test valid operations before initializing the hashmap
  wraprec = nr_php_user_instrument_wraprec_hashmap_add(NR_PSTR(FUNCTION_NAME));
  tlib_pass_if_null("adding valid function before init", wraprec);
  wraprec = nr_php_user_instrument_wraprec_hashmap_add(NR_PSTR(SCOPED_METHOD_NAME));
  tlib_pass_if_null("adding valid method before init", wraprec);

  // Initialize the hashmap
  nr_php_user_instrument_wraprec_hashmap_init();

  // Test valid operations after initializing the hashmap
  wraprec = nr_php_user_instrument_wraprec_hashmap_add(NR_PSTR(FUNCTION_NAME));
  tlib_pass_if_not_null("adding valid global function", wraprec);

  found_wraprec = nr_php_user_instrument_wraprec_hashmap_get(func_name, NULL);
  tlib_pass_if_ptr_equal("getting valid global function", wraprec, found_wraprec);

  found_wraprec = nr_php_user_instrument_wraprec_hashmap_get(func_name, scope_name);
  tlib_pass_if_null("getting global function with scope", found_wraprec);

  wraprec = nr_php_user_instrument_wraprec_hashmap_add(NR_PSTR(SCOPED_METHOD_NAME));
  tlib_pass_if_not_null("adding valid scoped method", wraprec);

  found_wraprec = nr_php_user_instrument_wraprec_hashmap_get(method_name, scope_name);
  tlib_pass_if_ptr_equal("getting scoped method", wraprec, found_wraprec);

  found_wraprec = nr_php_user_instrument_wraprec_hashmap_get(NULL, scope_name);
  tlib_pass_if_null("getting scoped method without method name", found_wraprec);

  found_wraprec = nr_php_user_instrument_wraprec_hashmap_get(method_name, NULL);
  tlib_pass_if_null("getting scoped method without scope", found_wraprec);

  nr_php_user_instrument_wraprec_hashmap_destroy();

  zend_string_free(func_name);
  zend_string_free(scope_name);
  zend_string_free(method_name);
}

#ifdef ZTS
static void test_ini_add_global_function() {
  nruserfn_t* wraprec;

  nr_php_user_instrument_wraprec_hashmap_ini_init();

  /* Add a global function via INI add */
  wraprec = nr_php_user_instrument_wraprec_hashmap_ini_add(
      NR_PSTR(FUNCTION_NAME));
  tlib_pass_if_not_null("ini_add global function", wraprec);
  tlib_pass_if_str_equal("ini_add funcname", FUNCTION_NAME, wraprec->funcname);
  tlib_pass_if_int_equal("ini_add is_method", 0, wraprec->is_method);

  /* Set flags as the INI handler callbacks would */
  wraprec->is_names_wt_simple = 1;
  tlib_pass_if_int_equal("ini_add flag set", 1, wraprec->is_names_wt_simple);

  /* Dedup: adding the same name returns the existing wraprec */
  nruserfn_t* wraprec2 = nr_php_user_instrument_wraprec_hashmap_ini_add(
      NR_PSTR(FUNCTION_NAME));
  tlib_pass_if_ptr_equal("ini_add dedup", wraprec, wraprec2);

  nr_php_user_instrument_wraprec_hashmap_ini_destroy();
  nr_php_user_instrument_wraprec_hashmap_destroy();
}

static void test_ini_add_scoped_method() {
  nruserfn_t* wraprec;

  nr_php_user_instrument_wraprec_hashmap_ini_init();

  wraprec = nr_php_user_instrument_wraprec_hashmap_ini_add(
      NR_PSTR(SCOPED_METHOD_NAME));
  tlib_pass_if_not_null("ini_add scoped method", wraprec);
  tlib_pass_if_str_equal("ini_add method funcname", METHOD_NAME,
                         wraprec->funcname);
  tlib_pass_if_str_equal("ini_add method classname", SCOPE_NAME,
                         wraprec->classname);
  tlib_pass_if_int_equal("ini_add is_method", 1, wraprec->is_method);

  /* Set flags as the custom tracer INI handler would */
  wraprec->create_metric = 1;
  wraprec->is_user_added = 1;

  nr_php_user_instrument_wraprec_hashmap_ini_destroy();
  nr_php_user_instrument_wraprec_hashmap_destroy();
}

static void test_ini_add_before_init() {
  nruserfn_t* wraprec;

  /* Ensure INI hashmaps are not initialized */
  nr_php_user_instrument_wraprec_hashmap_ini_destroy();

  wraprec = nr_php_user_instrument_wraprec_hashmap_ini_add(
      NR_PSTR(FUNCTION_NAME));
  tlib_pass_if_null("ini_add before init returns NULL", wraprec);
}

static void test_replay_ini_global_function() {
  nruserfn_t* wraprec;
  nruserfn_t* found;
  zend_string* func_name;

  func_name = zend_string_init(NR_PSTR(FUNCTION_NAME), 0);

  nr_php_user_instrument_wraprec_hashmap_ini_init();

  /* Add a naming function to INI hashmap */
  wraprec = nr_php_user_instrument_wraprec_hashmap_ini_add(
      NR_PSTR(FUNCTION_NAME));
  tlib_pass_if_not_null("replay: ini_add", wraprec);
  wraprec->is_names_wt_simple = 1;

  /* Replay into per-request hashmaps */
  nr_php_user_instrument_wraprec_hashmap_replay_ini();

  /* Verify the per-request hashmap has the entry with correct flags */
  found = nr_php_user_instrument_wraprec_hashmap_get(func_name, NULL);
  tlib_pass_if_not_null("replay: get after replay", found);
  tlib_pass_if_str_equal("replay: funcname", FUNCTION_NAME, found->funcname);
  tlib_pass_if_int_equal("replay: is_names_wt_simple", 1,
                         found->is_names_wt_simple);

  /* The per-request wraprec should be a copy, not the same pointer */
  tlib_pass_if_true("replay: different pointer (deep copy)",
                    found != wraprec, "%p != %p", found, wraprec);

  nr_php_user_instrument_wraprec_hashmap_destroy();
  nr_php_user_instrument_wraprec_hashmap_ini_destroy();

  zend_string_free(func_name);
}

static void test_replay_ini_scoped_method() {
  nruserfn_t* wraprec;
  nruserfn_t* found;
  zend_string* method_name;
  zend_string* scope_name;

  method_name = zend_string_init(NR_PSTR(METHOD_NAME), 0);
  scope_name = zend_string_init(NR_PSTR(SCOPE_NAME), 0);

  nr_php_user_instrument_wraprec_hashmap_ini_init();

  /* Add a scoped method to INI hashmap */
  wraprec = nr_php_user_instrument_wraprec_hashmap_ini_add(
      NR_PSTR(SCOPED_METHOD_NAME));
  tlib_pass_if_not_null("replay scoped: ini_add", wraprec);
  wraprec->create_metric = 1;
  wraprec->is_user_added = 1;

  /* Replay into per-request hashmaps */
  nr_php_user_instrument_wraprec_hashmap_replay_ini();

  /* Verify the per-request hashmap has the entry with correct flags */
  found = nr_php_user_instrument_wraprec_hashmap_get(method_name, scope_name);
  tlib_pass_if_not_null("replay scoped: get after replay", found);
  tlib_pass_if_str_equal("replay scoped: funcname", METHOD_NAME,
                         found->funcname);
  tlib_pass_if_str_equal("replay scoped: classname", SCOPE_NAME,
                         found->classname);
  tlib_pass_if_int_equal("replay scoped: create_metric", 1,
                         found->create_metric);
  tlib_pass_if_int_equal("replay scoped: is_user_added", 1,
                         found->is_user_added);

  nr_php_user_instrument_wraprec_hashmap_destroy();
  nr_php_user_instrument_wraprec_hashmap_ini_destroy();

  zend_string_free(method_name);
  zend_string_free(scope_name);
}

static void test_replay_ini_before_init() {
  /* Replay with no INI hashmaps should be safe (no-op / NULL deep copy) */
  nr_php_user_instrument_wraprec_hashmap_ini_destroy();
  nr_php_user_instrument_wraprec_hashmap_destroy();

  nr_php_user_instrument_wraprec_hashmap_replay_ini();

  /* Per-request hashmaps should still be NULL — get should return NULL */
  zend_string* func_name = zend_string_init(NR_PSTR(FUNCTION_NAME), 0);
  nruserfn_t* found
      = nr_php_user_instrument_wraprec_hashmap_get(func_name, NULL);
  tlib_pass_if_null("replay before init: get returns NULL", found);

  zend_string_free(func_name);
}

static void test_replay_ini_multiple_requests() {
  nruserfn_t* wraprec;
  nruserfn_t* found;
  zend_string* func_name;

  func_name = zend_string_init(NR_PSTR(FUNCTION_NAME), 0);

  nr_php_user_instrument_wraprec_hashmap_ini_init();
  wraprec = nr_php_user_instrument_wraprec_hashmap_ini_add(
      NR_PSTR(FUNCTION_NAME));
  wraprec->is_names_wt_simple = 1;

  /* First request */
  nr_php_user_instrument_wraprec_hashmap_replay_ini();
  found = nr_php_user_instrument_wraprec_hashmap_get(func_name, NULL);
  tlib_pass_if_not_null("multi-request: first replay", found);
  tlib_pass_if_int_equal("multi-request: first is_names_wt_simple", 1,
                         found->is_names_wt_simple);

  /* Simulate RSHUTDOWN — destroy per-request hashmaps */
  nr_php_user_instrument_wraprec_hashmap_destroy();

  /* Second request — replay again */
  nr_php_user_instrument_wraprec_hashmap_replay_ini();
  found = nr_php_user_instrument_wraprec_hashmap_get(func_name, NULL);
  tlib_pass_if_not_null("multi-request: second replay", found);
  tlib_pass_if_int_equal("multi-request: second is_names_wt_simple", 1,
                         found->is_names_wt_simple);

  nr_php_user_instrument_wraprec_hashmap_destroy();
  nr_php_user_instrument_wraprec_hashmap_ini_destroy();

  zend_string_free(func_name);
}
#endif /* ZTS */

#endif /* ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO */

// clang-format on

void test_main(void* p NRUNUSED) {

  tlib_php_engine_create("" PTSRMLS_CC);

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
  test_wraprecs_hashmap();
#ifdef ZTS
  test_ini_add_global_function();
  test_ini_add_scoped_method();
  test_ini_add_before_init();
  test_replay_ini_global_function();
  test_replay_ini_scoped_method();
  test_replay_ini_before_init();
  test_replay_ini_multiple_requests();
#endif
#endif

  tlib_php_engine_destroy(TSRMLS_C);
}
