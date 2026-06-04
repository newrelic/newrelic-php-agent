/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_fibers.h"
#include "php_newrelic.h"
#include "nr_mysqli_metadata.h"
#include "util_hashmap.h"
#include "util_memory.h"
#include "util_stack.h"
#include "util_strings.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO

static nr_hashmap_t* dummy_hashmap_data() {
  nr_hashmap_t* h = NULL;
  h = nr_hashmap_create(NULL);
  nr_hashmap_set(h, NR_PSTR("a"), "valA");
  nr_hashmap_set(h, NR_PSTR("b"), "valB");
  nr_hashmap_set(h, NR_PSTR("c"), "valC");
  return h;
}

static nr_stack_t dummy_stack_data() {
  nr_stack_t s;
  nr_stack_init(&s, NR_STACK_DEFAULT_CAPACITY);
  nr_stack_push(&s, "elemA");
  nr_stack_push(&s, "elemB");
  nr_stack_push(&s, "elemC");
  return s;
}

static nr_stack_t dummy_stack_data_bool() {
  nr_stack_t s;
  nr_stack_init(&s, NR_STACK_DEFAULT_CAPACITY);
  nr_stack_push(&s, (void*)!NULL);
  nr_stack_push(&s, NULL);
  nr_stack_push(&s, (void*)!NULL);
  return s;
}

static nr_stack_t dummy_stack_data_zval() {
  nr_stack_t s;
  zval* z = NULL;
  zval* zz = NULL;
  zval* zzz = NULL;
  nr_stack_init(&s, NR_STACK_DEFAULT_CAPACITY);
  z = nr_php_zval_alloc();
  nr_php_zval_str(z, "stackA");
  nr_stack_push(&s, z);
  zz = nr_php_zval_alloc();
  nr_php_zval_str(z, "stackB");
  nr_stack_push(&s, zz);
  zzz = nr_php_zval_alloc();
  nr_php_zval_str(z, "stackC");
  nr_stack_push(&s, zzz);
  return s;
}

static txn_globals_t* dummy_txn_globals() {
  txn_globals_t* tg = NULL;
  tg = nr_malloc(sizeof(txn_globals_t));
  tg->execute_count = 2;
  tg->generating_explain_plan = 1;
  tg->guzzle_objs = dummy_hashmap_data();
  tg->mysqli_links = nr_mysqli_metadata_create();
  nr_mysqli_metadata_set_connect(tg->mysqli_links, 1, "db-host", "db-user",
                                 "db-password", "db-database", 3306,
                                 "db-socket", 1);
  tg->mysqli_queries = dummy_hashmap_data();
  tg->pdo_link_options = dummy_hashmap_data();
  tg->curl_ignore_setopt = 1;
  tg->curl_metadata = dummy_hashmap_data();
  tg->curl_multi_metadata = dummy_hashmap_data();
  tg->prepared_statements = dummy_hashmap_data();

  return tg;
}

static void free_txn_globals(txn_globals_t* tg) {
  nr_hashmap_destroy(&tg->guzzle_objs);
  nr_mysqli_metadata_destroy(&tg->mysqli_links);
  nr_hashmap_destroy(&tg->mysqli_queries);
  nr_hashmap_destroy(&tg->pdo_link_options);
  nr_hashmap_destroy(&tg->curl_metadata);
  nr_hashmap_destroy(&tg->curl_multi_metadata);
  nr_hashmap_destroy(&tg->prepared_statements);
  nr_free(tg);
}

static ctx_globals_t* dummy_ctx_globals() {
  ctx_globals_t* cg = NULL;
  cg = nr_malloc(sizeof(ctx_globals_t));
  cg->doctrine_dql = nr_strdup("doctrine_dql");
  cg->drupal_http_request_depth = 3;
  cg->php_cur_stack_depth = 4;
  cg->mysql_last_conn = nr_strdup("mysql_last_conn");
  cg->pgsql_last_conn = nr_strdup("pgsql_last_conn");
  cg->datastore_connections = dummy_hashmap_data();
  cg->deprecated_capture_request_parameters = 1;
  cg->check_cufa = true;
  cg->predis_commands = dummy_hashmap_data();
  cg->drupal_invoke_all_hooks = dummy_stack_data_zval();
  cg->drupal_invoke_all_states = dummy_stack_data_bool();
  // TODO: segment handling?
  cg->wordpress_tags = dummy_stack_data();
  cg->wordpress_tag_states = dummy_stack_data_bool();
  cg->predis_ctxs = dummy_stack_data();
  return cg;
}

static void free_ctx_globals(ctx_globals_t* cg) {
  nr_free(cg->doctrine_dql);
  nr_free(cg->mysql_last_conn);
  nr_free(cg->pgsql_last_conn);
  nr_hashmap_destroy(&cg->datastore_connections);
  nr_hashmap_destroy(&cg->predis_commands);
  nr_stack_destroy_fields(&cg->drupal_invoke_all_hooks);
  nr_stack_destroy_fields(&cg->drupal_invoke_all_states);
  // TODO: segment handling?
  nr_stack_destroy_fields(&cg->wordpress_tags);
  nr_stack_destroy_fields(&cg->wordpress_tag_states);
  nr_stack_destroy_fields(&cg->predis_ctxs);
  nr_free(cg);
}

static void test_init_destroy_hashmap(void) {
  nr_hashmap_t* test_fiber_global_map = NULL;
  tlib_php_request_start();

  /*
   * Test : init creates the hashmap when one does not yet exist.
   */
  nrf_fiber_destroy_global_hashmap(&test_fiber_global_map);
  tlib_pass_if_null("hashmap is NULL after destroy", test_fiber_global_map);

  nrf_fiber_init_global_hashmap(&test_fiber_global_map);
  tlib_pass_if_not_null("init creates the hashmap", test_fiber_global_map);

  /*
   * Test : init is idempotent and does not replace an existing hashmap.
   */
  {
    nr_hashmap_t* original = test_fiber_global_map;
    nrf_fiber_init_global_hashmap(&test_fiber_global_map);
    tlib_pass_if_ptr_equal("init does not replace existing hashmap", original,
                           test_fiber_global_map);
  }

  /*
   * Test : destroy nulls out the hashmap pointer.
   */
  nrf_fiber_destroy_global_hashmap(&test_fiber_global_map);
  tlib_pass_if_null("hashmap is NULL after destroy", test_fiber_global_map);

  /*
   * Test : destroy is safe to call when hashmap is already NULL.
   */
  nrf_fiber_destroy_global_hashmap(&test_fiber_global_map);
  tlib_pass_if_null("destroy is null safe", test_fiber_global_map);

  tlib_php_request_end();
}

static void test_copy_txn_globals(void) {
  txn_globals_t* src;
  txn_globals_t* copy;

  tlib_php_request_start();

  src = dummy_txn_globals();

  copy = nrf_fiber_copy_txn_globals(src);

  tlib_pass_if_not_null("copy returns a non-NULL pointer", copy);
  tlib_pass_if_int_equal("execute_count is copied", 2, copy->execute_count);
  tlib_pass_if_int_equal("generating_explain_plan is copied", 1,
                         copy->generating_explain_plan);
  tlib_pass_if_int_equal("curl_ignore_setopt is copied", 1,
                         copy->curl_ignore_setopt);

  tlib_pass_if_not_null("mysqli_links is copied", copy->mysqli_links);

  tlib_pass_if_not_null("guzzle_objs is copied", copy->guzzle_objs);
  tlib_pass_if_not_null("mysqli_queries is copied", copy->mysqli_queries);
  tlib_pass_if_not_null("pdo_link_options is copied", copy->pdo_link_options);
  tlib_pass_if_not_null("curl_metadata is copied", copy->curl_metadata);
  tlib_pass_if_not_null("curl_multi_metadata is copied",
                        copy->curl_multi_metadata);
  tlib_pass_if_not_null("prepared_statements is copied",
                        copy->prepared_statements);

  free_txn_globals(copy);
  free_txn_globals(src);

  tlib_php_request_end();
}

static void test_copy_txn_globals_with_hashmaps(void) {
  txn_globals_t* src;
  txn_globals_t* copy;

  tlib_php_request_start();

  src = dummy_txn_globals();

  copy = nrf_fiber_copy_txn_globals(src);

  tlib_pass_if_not_null("copy contains a guzzle_objs hashmap",
                        copy->guzzle_objs);
  tlib_fail_if_int_equal("guzzle_objs is a distinct allocation",
                         (int)(uintptr_t)src->guzzle_objs,
                         (int)(uintptr_t)copy->guzzle_objs);
  tlib_pass_if_size_t_equal("guzzle_objs has the same number of entries",
                            nr_hashmap_count(src->guzzle_objs),
                            nr_hashmap_count(copy->guzzle_objs));

  free_txn_globals(copy);
  free_txn_globals(src);

  tlib_php_request_end();
}

static void test_copy_ctx_globals(void) {
  ctx_globals_t* src;
  ctx_globals_t* copy;

  tlib_php_request_start();

  src = dummy_ctx_globals();

  copy = nrf_fiber_copy_ctx_globals(src);

  tlib_pass_if_not_null("copy returns a non-NULL pointer", copy);
  tlib_pass_if_size_t_equal("drupal_http_request_depth is copied", 3,
                            copy->drupal_http_request_depth);
  tlib_pass_if_int_equal("php_cur_stack_depth is copied", 4,
                         copy->php_cur_stack_depth);
  tlib_pass_if_int_equal("deprecated_capture_request_parameters is copied", 1,
                         copy->deprecated_capture_request_parameters);
  tlib_pass_if_int_equal("check_cufa is copied", true, copy->check_cufa);

  tlib_pass_if_not_null("datastore_connections hashmap is copied",
                        copy->datastore_connections);
  tlib_fail_if_int_equal("datastore_connections is a distinct allocation",
                         (int)(uintptr_t)src->datastore_connections,
                         (int)(uintptr_t)copy->datastore_connections);

  /*
   * drupal_http_request_segment is intentionally reset to NULL in the copy.
   */
  tlib_pass_if_null("drupal_http_request_segment is reset to NULL",
                    copy->drupal_http_request_segment);

  free_ctx_globals(copy);
  free_ctx_globals(src);

  tlib_php_request_end();
}

static void test_add_fiber_context_bad_input(void) {
  nr_hashmap_t* test_fiber_global_map = NULL;
  tlib_php_request_start();

  /*
   * Ensure the hashmap is not initialized.
   */
  nrf_fiber_destroy_global_hashmap(&test_fiber_global_map);

  /*
   * Test : NULL key returns failure.
   */
  tlib_pass_if_status_failure("NULL key fails when hashmap is uninitialized",
                              nrf_add_fiber_context_to_global_hashmap(
                                  test_fiber_global_map, NULL, NULL, NULL));

  /*
   * Test : Uninitialized hashmap returns failure even with a valid key.
   */
  tlib_pass_if_status_failure(
      "uninitialized hashmap fails",
      nrf_add_fiber_context_to_global_hashmap(test_fiber_global_map, NULL, NULL,
                                              "fiber-key"));

  /*
   * Test : NULL key still fails after init.
   */
  nrf_fiber_init_global_hashmap(&test_fiber_global_map);
  tlib_pass_if_status_failure("NULL key fails after init",
                              nrf_add_fiber_context_to_global_hashmap(
                                  test_fiber_global_map, NULL, NULL, NULL));

  nrf_fiber_destroy_global_hashmap(&test_fiber_global_map);

  tlib_php_request_end();
}

static void test_add_fiber_context_happy_path(void) {
  nr_hashmap_t* test_fiber_global_map = NULL;
  txn_globals_t* tg = NULL;
  ctx_globals_t* cg = NULL;
  tlib_php_request_start();

  tg = dummy_txn_globals();
  cg = dummy_ctx_globals();

  nrf_fiber_init_global_hashmap(&test_fiber_global_map);

  tlib_pass_if_status_success("adding a fiber context succeeds",
                              nrf_add_fiber_context_to_global_hashmap(
                                  test_fiber_global_map, tg, cg, "fiber-1"));

  tlib_pass_if_size_t_equal("hashmap has one entry after add", 1,
                            nr_hashmap_count(test_fiber_global_map));

  /*
   * Test : adding a second context increments the count.
   */
  tlib_pass_if_status_success("adding a second fiber context succeeds",
                              nrf_add_fiber_context_to_global_hashmap(
                                  test_fiber_global_map, tg, cg, "fiber-2"));

  tlib_pass_if_size_t_equal("hashmap has two entries", 2,
                            nr_hashmap_count(test_fiber_global_map));

  /* free_fiber_globals destructor cleans up the deep-copied snapshots. */
  nrf_fiber_destroy_global_hashmap(&test_fiber_global_map);
  free_txn_globals(tg);
  free_ctx_globals(cg);

  tlib_php_request_end();
}

static void test_remove_fiber_context_bad_input(void) {
  nr_hashmap_t* test_fiber_global_map = NULL;
  tlib_php_request_start();

  /*
   * Test : NULL key fails when hashmap is uninitialized.
   */
  tlib_pass_if_status_failure("NULL key fails when hashmap is uninitialized",
                              nrf_remove_fiber_context_from_global_hashmap(
                                  test_fiber_global_map, NULL));

  /*
   * Test : Empty key fails (nr_strlen("") < 1).
   */
  tlib_pass_if_status_failure(
      "empty key fails",
      nrf_remove_fiber_context_from_global_hashmap(test_fiber_global_map, ""));

  /*
   * Test : valid key with uninitialized hashmap fails.
   */
  tlib_pass_if_status_failure("uninitialized hashmap fails",
                              nrf_remove_fiber_context_from_global_hashmap(
                                  test_fiber_global_map, "fiber-1"));

  /*
   * Test : removing a nonexistent key from an initialized hashmap fails.
   */
  nrf_fiber_init_global_hashmap(&test_fiber_global_map);
  tlib_pass_if_status_failure("missing key fails",
                              nrf_remove_fiber_context_from_global_hashmap(
                                  test_fiber_global_map, "does-not-exist"));

  nrf_fiber_destroy_global_hashmap(&test_fiber_global_map);

  tlib_php_request_end();
}

static void test_remove_fiber_context_happy_path(void) {
  static const char* key = "fiber-remove";
  nr_hashmap_t* test_fiber_global_map = NULL;
  txn_globals_t* tg = NULL;
  ctx_globals_t* cg = NULL;

  tlib_php_request_start();

  tg = dummy_txn_globals();
  cg = dummy_ctx_globals();

  nrf_fiber_init_global_hashmap(&test_fiber_global_map);
  tlib_pass_if_status_success("add succeeds before remove",
                              nrf_add_fiber_context_to_global_hashmap(
                                  test_fiber_global_map, tg, cg, key));

  tlib_pass_if_size_t_equal("hashmap has one entry before remove", 1,
                            nr_hashmap_count(test_fiber_global_map));

  tlib_pass_if_status_success(
      "remove succeeds for existing key",
      nrf_remove_fiber_context_from_global_hashmap(test_fiber_global_map, key));

  tlib_pass_if_size_t_equal("hashmap is empty after remove", 0,
                            nr_hashmap_count(test_fiber_global_map));

  /*
   * Test : removing the same key again fails.
   */
  tlib_pass_if_status_failure(
      "remove fails when key has already been removed",
      nrf_remove_fiber_context_from_global_hashmap(test_fiber_global_map, key));

  nrf_fiber_destroy_global_hashmap(&test_fiber_global_map);
  free_txn_globals(tg);
  free_ctx_globals(cg);

  tlib_php_request_end();
}

static void test_switch_global_context_bad_input(void) {
  nr_hashmap_t* test_fiber_global_map = NULL;
  fiber_globals_t* fiber_globals = NULL;
  tlib_php_request_start();

  /*
   * Test : NULL key fails when hashmap is uninitialized.
   */
  tlib_pass_if_status_failure("NULL key fails when hashmap is uninitialized",
                              nrf_fiber_switch_global_context(
                                  test_fiber_global_map, &fiber_globals, NULL));

  /*
   * Test : valid key with uninitialized hashmap fails.
   */
  tlib_pass_if_status_failure(
      "uninitialized hashmap fails",
      nrf_fiber_switch_global_context(test_fiber_global_map, &fiber_globals,
                                      "fiber-1"));

  /*
   * Test : switching to a key that has no snapshot fails and does not
   * mutate fiber_globals.
   */
  nrf_fiber_init_global_hashmap(&test_fiber_global_map);
  fiber_globals = NULL;
  tlib_pass_if_status_failure(
      "missing key fails",
      nrf_fiber_switch_global_context(test_fiber_global_map, &fiber_globals,
                                      "does-not-exist"));
  tlib_pass_if_null("fiber_globals is unchanged after a failed switch",
                    fiber_globals);

  nrf_fiber_destroy_global_hashmap(&test_fiber_global_map);

  tlib_php_request_end();
}

static void test_switch_global_context_happy_path(void) {
  static const char* key = "fiber-switch";
  nr_hashmap_t* test_fiber_global_map = NULL;
  fiber_globals_t* fiber_globals = NULL;
  txn_globals_t* tg = NULL;
  ctx_globals_t* cg = NULL;

  tlib_php_request_start();

  tg = dummy_txn_globals();
  cg = dummy_ctx_globals();

  nrf_fiber_init_global_hashmap(&test_fiber_global_map);
  fiber_globals = NULL;

  tlib_pass_if_status_success("add succeeds before switch",
                              nrf_add_fiber_context_to_global_hashmap(
                                  test_fiber_global_map, tg, cg, key));

  tlib_pass_if_status_success("switch succeeds for an existing key",
                              nrf_fiber_switch_global_context(
                                  test_fiber_global_map, &fiber_globals, key));

  tlib_pass_if_not_null("switch sets fiber_globals to a non-NULL snapshot",
                        fiber_globals);
  tlib_pass_if_not_null("the active snapshot has txn_globals",
                        fiber_globals->txn_globals);
  tlib_pass_if_not_null("the active snapshot has ctx_globals",
                        fiber_globals->ctx_globals);

  /*
   * destroy frees the snapshot via free_fiber_globals; clear the dangling
   * pointer first so other code in request shutdown does not dereference it.
   */
  nrf_fiber_destroy_global_hashmap(&test_fiber_global_map);
  fiber_globals = NULL;
  free_txn_globals(tg);
  free_ctx_globals(cg);

  tlib_php_request_end();
}

static void test_destroy_frees_entries(void) {
  nr_hashmap_t* test_fiber_global_map = NULL;
  txn_globals_t* tg = NULL;
  ctx_globals_t* cg = NULL;

  tlib_php_request_start();

  tg = dummy_txn_globals();
  cg = dummy_ctx_globals();

  nrf_fiber_init_global_hashmap(&test_fiber_global_map);

  tlib_pass_if_status_success("add a context",
                              nrf_add_fiber_context_to_global_hashmap(
                                  test_fiber_global_map, tg, cg, "entry-a"));
  tlib_pass_if_status_success("add another context",
                              nrf_add_fiber_context_to_global_hashmap(
                                  test_fiber_global_map, tg, cg, "entry-b"));

  /*
   * Destroy invokes free_fiber_globals on every entry. We can't directly
   * assert the destructor fired, but we can confirm the hashmap pointer is
   * cleared and the request can complete cleanly with no leaks.
   */
  nrf_fiber_destroy_global_hashmap(&test_fiber_global_map);
  tlib_pass_if_null("hashmap is cleared by destroy", test_fiber_global_map);

  free_txn_globals(tg);
  free_ctx_globals(cg);

  tlib_php_request_end();
}

#endif /* PHP 8.1+ */

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("" PTSRMLS_CC);

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
  test_init_destroy_hashmap();
  test_copy_txn_globals();
  test_copy_txn_globals_with_hashmaps();
  test_copy_ctx_globals();
  test_add_fiber_context_bad_input();
  test_add_fiber_context_happy_path();
  test_remove_fiber_context_bad_input();
  test_remove_fiber_context_happy_path();
  test_switch_global_context_bad_input();
  test_switch_global_context_happy_path();
  test_destroy_frees_entries();
#endif

  tlib_php_engine_destroy(TSRMLS_C);
}
