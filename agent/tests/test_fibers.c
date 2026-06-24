/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include <stdint.h>

#include "nr_datastore_instance.h"
#include "php_agent.h"
#include "php_fibers.h"
#include "php_newrelic.h"
#include "nr_mysqli_metadata.h"
#include "util_hashmap.h"
#include "util_memory.h"
#include "util_stack.h"
#include "util_strings.h"
#include "util_time.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO

static void dtor_datastore(void* val) {
  nr_datastore_instance_t* d = (nr_datastore_instance_t*)val;
  nr_datastore_instance_destroy(&d);
}

static nr_datastore_instance_t* make_datastore_instance(const char* host,
                                                        const char* port,
                                                        const char* db) {
  nr_datastore_instance_t* d = nr_malloc(sizeof(nr_datastore_instance_t));
  d->host = nr_strdup(host);
  d->port_path_or_id = nr_strdup(port);
  d->database_name = nr_strdup(db);
  return d;
}

static nr_hashmap_t* dummy_hashmap_datastore() {
  nr_hashmap_t* h = nr_hashmap_create(dtor_datastore);
  nr_hashmap_set(h, NR_PSTR("a"),
                 make_datastore_instance("hostA", "portA", "dbA"));
  nr_hashmap_set(h, NR_PSTR("b"),
                 make_datastore_instance("hostB", "portB", "dbB"));
  nr_hashmap_set(h, NR_PSTR("c"),
                 make_datastore_instance("hostC", "portC", "dbC"));
  return h;
}

static void dtor_time(void* val) {
  nr_free(val);
}

static nr_hashmap_t* dummy_hashmap_time() {
  nr_hashmap_t* h = nr_hashmap_create(dtor_time);
  nrtime_t* ta = nr_malloc(sizeof(nrtime_t));
  nrtime_t* tb = nr_malloc(sizeof(nrtime_t));
  nrtime_t* tc = nr_malloc(sizeof(nrtime_t));
  *ta = 1000;
  *tb = 2000;
  *tc = 3000;
  nr_hashmap_set(h, NR_PSTR("a"), ta);
  nr_hashmap_set(h, NR_PSTR("b"), tb);
  nr_hashmap_set(h, NR_PSTR("c"), tc);
  return h;
}

static void dtor_free_stack_str(void* e, NRUNUSED void* d) {
  char* str = (char*)e;
  nr_free(str);
}

static nr_stack_t dummy_stack_data_str() {
  nr_stack_t s;
  nr_stack_init(&s, NR_STACK_DEFAULT_CAPACITY);
  s.dtor = dtor_free_stack_str;
  nr_stack_push(&s, nr_strdup("elemA"));
  nr_stack_push(&s, nr_strdup("elemB"));
  nr_stack_push(&s, nr_strdup("elemC"));
  return s;
}

static nr_stack_t dummy_stack_data_bool() {
  nr_stack_t s;
  nr_stack_init(&s, NR_STACK_DEFAULT_CAPACITY);
  nr_stack_push(&s, (void*)(uintptr_t)1);
  nr_stack_push(&s, NULL);
  nr_stack_push(&s, (void*)(uintptr_t)1);
  return s;
}

static void dtor_free_stack_zval(void* e, NRUNUSED void* d) {
  zval* zv = (zval*)e;
  nr_php_zval_free(&zv);
}

static nr_stack_t dummy_stack_data_zval() {
  nr_stack_t s;
  zval* z = NULL;
  zval* zz = NULL;
  zval* zzz = NULL;
  nr_stack_init(&s, NR_STACK_DEFAULT_CAPACITY);
  s.dtor = dtor_free_stack_zval;
  z = nr_php_zval_alloc();
  nr_php_zval_str(z, "stackA");
  nr_stack_push(&s, z);
  zz = nr_php_zval_alloc();
  nr_php_zval_str(zz, "stackB");
  nr_stack_push(&s, zz);
  zzz = nr_php_zval_alloc();
  nr_php_zval_str(zzz, "stackC");
  nr_stack_push(&s, zzz);
  return s;
}

static ctx_globals_t* dummy_ctx_globals() {
  ctx_globals_t* cg = NULL;
  cg = nr_malloc(sizeof(ctx_globals_t));
  cg->doctrine_dql = nr_strdup("doctrine_dql");
  cg->drupal_http_request_depth = 3;
  cg->php_cur_stack_depth = 4;
  cg->mysql_last_conn = nr_strdup("mysql_last_conn");
  cg->pgsql_last_conn = nr_strdup("pgsql_last_conn");
  cg->datastore_connections = dummy_hashmap_datastore();
  cg->deprecated_capture_request_parameters = 1;
  cg->check_cufa = true;
  cg->predis_commands = dummy_hashmap_time();
  cg->drupal_invoke_all_hooks = dummy_stack_data_zval();
  cg->drupal_invoke_all_states = dummy_stack_data_bool();
  cg->wordpress_tags = dummy_stack_data_str();
  cg->wordpress_tag_states = dummy_stack_data_bool();
  cg->predis_ctxs = dummy_stack_data_str();
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
  nr_stack_destroy_fields(&cg->wordpress_tags);
  nr_stack_destroy_fields(&cg->wordpress_tag_states);
  nr_stack_destroy_fields(&cg->predis_ctxs);
  nr_free(cg);
}

/*
 * Cleanup for a ctx_globals returned by nr_fiber_copy_ctx_globals: skips the
 * fields that are shallow-copied (datastore_connections, predis_commands)
 * since those are still owned by the source. Mirrors the ctx_globals section
 * of free_fiber_globals.
 */
static void free_ctx_globals_copy(ctx_globals_t* cg) {
  nr_free(cg->doctrine_dql);
  nr_free(cg->mysql_last_conn);
  nr_free(cg->pgsql_last_conn);
  nr_stack_destroy_fields(&cg->drupal_invoke_all_hooks);
  nr_stack_destroy_fields(&cg->drupal_invoke_all_states);
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
  nr_fiber_destroy_global_hashmap(&test_fiber_global_map);
  tlib_pass_if_null("hashmap is NULL after destroy", test_fiber_global_map);

  nr_fiber_init_global_hashmap(&test_fiber_global_map);
  tlib_pass_if_not_null("init creates the hashmap", test_fiber_global_map);

  /*
   * Test : init is idempotent and does not replace an existing hashmap.
   */
  {
    nr_hashmap_t* original = test_fiber_global_map;
    nr_fiber_init_global_hashmap(&test_fiber_global_map);
    tlib_pass_if_ptr_equal("init does not replace existing hashmap", original,
                           test_fiber_global_map);
  }

  /*
   * Test : destroy nulls out the hashmap pointer.
   */
  nr_fiber_destroy_global_hashmap(&test_fiber_global_map);
  tlib_pass_if_null("hashmap is NULL after destroy", test_fiber_global_map);

  /*
   * Test : destroy is safe to call when hashmap is already NULL.
   */
  nr_fiber_destroy_global_hashmap(&test_fiber_global_map);
  tlib_pass_if_null("destroy is null safe", test_fiber_global_map);

  tlib_php_request_end();
}

static void test_copy_ctx_globals(void) {
  ctx_globals_t* src;
  ctx_globals_t* copy;

  tlib_php_request_start();

  src = dummy_ctx_globals();

  copy = nr_fiber_copy_ctx_globals(src);

  tlib_pass_if_not_null("copy returns a non-NULL pointer", copy);

  /*
   * drupal_http_request_depth is reset to 0 in the copy; the fiber starts
   * outside any drupal_http_request invocation regardless of main's depth.
   *
   * php_cur_stack_depth is copied from src rather than reset. The agent
   * tracks the PHP call depth across fiber switches so that exit handlers
   * pop the same number of frames they pushed; resetting to 0 in the fiber
   * snapshot caused depth underflow when control returned to main.
   */
  tlib_pass_if_size_t_equal("drupal_http_request_depth is reset to 0", 0,
                            copy->drupal_http_request_depth);
  tlib_pass_if_int_equal("php_cur_stack_depth is copied from src", 4,
                         copy->php_cur_stack_depth);

  tlib_pass_if_int_equal("deprecated_capture_request_parameters is copied", 1,
                         copy->deprecated_capture_request_parameters);
  tlib_pass_if_int_equal("check_cufa is copied", true, copy->check_cufa);

  /*
   * datastore_connections and predis_commands are shallow-copied: the copy
   * must share the same hashmap pointer as the source.
   */
  tlib_pass_if_ptr_equal("datastore_connections is shared with src",
                         src->datastore_connections,
                         copy->datastore_connections);
  tlib_pass_if_ptr_equal("predis_commands is shared with src",
                         src->predis_commands, copy->predis_commands);

  /*
   * drupal_http_request_segment is intentionally reset to NULL in the copy.
   */
  tlib_pass_if_null("drupal_http_request_segment is reset to NULL",
                    copy->drupal_http_request_segment);

  /*
   * wordpress_tags is COPY_STACK'd with copy_elem_str (which clones each
   * string), then the copy's stack dtor is explicitly set so the copy owns
   * the cloned strings. Without that dtor override, the cloned strings would
   * leak when the fiber snapshot is destroyed. We can't compare against the
   * static fiber_str_dtor symbol, but a non-NULL dtor proves the override
   * line ran.
   */
  tlib_pass_if_true(
      "wordpress_tags copy has a dtor set so cloned "
      "strings are freed by destroy",
      NULL != copy->wordpress_tags.dtor, "expected non-NULL dtor");

  free_ctx_globals_copy(copy);
  free_ctx_globals(src);

  tlib_php_request_end();
}

/*
 * Build a minimal ctx_globals_t with NULL string fields so we can verify
 * COPY_STRING preserves NULL rather than substituting the empty string ""
 * via nr_strdup. Callers like php_mysql.c and php_pgsql.c use these pointers
 * as a "valid last-connection set" sentinel; a copy that turned NULL into ""
 * would flip those checks truthy in the fiber snapshot.
 *
 * Hashmap and stack fields still need to be non-NULL because the COPY_STACK
 * helpers don't tolerate NULL sources.
 */
static ctx_globals_t* dummy_ctx_globals_null_strings() {
  ctx_globals_t* cg = NULL;
  cg = nr_malloc(sizeof(ctx_globals_t));
  cg->doctrine_dql = NULL;
  cg->drupal_http_request_depth = 0;
  cg->php_cur_stack_depth = 0;
  cg->mysql_last_conn = NULL;
  cg->pgsql_last_conn = NULL;
  cg->datastore_connections = dummy_hashmap_datastore();
  cg->deprecated_capture_request_parameters = 0;
  cg->check_cufa = false;
  cg->predis_commands = dummy_hashmap_time();
  cg->drupal_invoke_all_hooks = dummy_stack_data_zval();
  cg->drupal_invoke_all_states = dummy_stack_data_bool();
  cg->wordpress_tags = dummy_stack_data_str();
  cg->wordpress_tag_states = dummy_stack_data_bool();
  cg->predis_ctxs = dummy_stack_data_str();
  return cg;
}

static void test_copy_ctx_globals_null_strings(void) {
  ctx_globals_t* src;
  ctx_globals_t* copy;

  tlib_php_request_start();

  src = dummy_ctx_globals_null_strings();
  copy = nr_fiber_copy_ctx_globals(src);

  tlib_pass_if_not_null("copy returns a non-NULL pointer", copy);
  tlib_pass_if_null("NULL doctrine_dql copies as NULL, not \"\"",
                    copy->doctrine_dql);
  tlib_pass_if_null("NULL mysql_last_conn copies as NULL, not \"\"",
                    copy->mysql_last_conn);
  tlib_pass_if_null("NULL pgsql_last_conn copies as NULL, not \"\"",
                    copy->pgsql_last_conn);

  free_ctx_globals_copy(copy);
  free_ctx_globals(src);

  tlib_php_request_end();
}

static void test_copy_ctx_globals_null_src(void) {
  tlib_php_request_start();

  /*
   * nr_fiber_copy_ctx_globals returns NULL on a NULL source so callers can
   * detect the failure. add_fiber_context relies on this to refuse to insert
   * a partial entry into the fiber globals hashmap.
   */
  tlib_pass_if_null("copy returns NULL when src is NULL",
                    nr_fiber_copy_ctx_globals(NULL));

  tlib_php_request_end();
}

/*
 * Exercises the free_fiber_globals NULL-tolerance and the partial-wrapper
 * cleanup path. Without the fix, a wrapper with NULL ctx_globals would be
 * leaked because the destructor early-returned without freeing f.
 */
static void test_free_fiber_globals_null_paths(void) {
  fiber_globals_t* f = NULL;

  tlib_php_request_start();

  /* NULL fiber_globals must not crash. */
  free_fiber_globals(NULL);

  /* fiber_globals_t with NULL ctx_globals: f itself must still be freed.
     If this path leaks, valgrind/asan will surface it. */
  f = nr_malloc(sizeof(fiber_globals_t));
  f->ctx_globals = NULL;
  free_fiber_globals(f);

  tlib_php_request_end();
}

static void test_add_fiber_context_null_src(void) {
  nr_hashmap_t* test_fiber_global_map = NULL;

  tlib_php_request_start();

  nr_fiber_init_global_hashmap(&test_fiber_global_map);

  /*
   * NULL src_ctx_globals must produce NR_FAILURE and must NOT insert a
   * partial wrapper into the hashmap. Without the guard, the destructor
   * would later warn-and-leak when the orphan was overwritten or the map
   * destroyed.
   */
  tlib_pass_if_status_failure(
      "add fails when src_ctx_globals is NULL",
      nr_add_fiber_context_to_global_hashmap(test_fiber_global_map, NULL,
                                             "fiber-null-src"));
  tlib_pass_if_size_t_equal("hashmap remains empty after failed add", 0,
                            nr_hashmap_count(test_fiber_global_map));

  nr_fiber_destroy_global_hashmap(&test_fiber_global_map);

  tlib_php_request_end();
}

static void test_add_fiber_context_bad_input(void) {
  nr_hashmap_t* test_fiber_global_map = NULL;
  tlib_php_request_start();

  /*
   * Ensure the hashmap is not initialized.
   */
  nr_fiber_destroy_global_hashmap(&test_fiber_global_map);

  /*
   * Test : NULL key returns failure.
   */
  tlib_pass_if_status_failure("NULL key fails when hashmap is uninitialized",
                              nr_add_fiber_context_to_global_hashmap(
                                  test_fiber_global_map, NULL, NULL));

  /*
   * Test : Uninitialized hashmap returns failure even with a valid key.
   */
  tlib_pass_if_status_failure("uninitialized hashmap fails",
                              nr_add_fiber_context_to_global_hashmap(
                                  test_fiber_global_map, NULL, "fiber-key"));

  /*
   * Test : NULL key still fails after init.
   */
  nr_fiber_init_global_hashmap(&test_fiber_global_map);
  tlib_pass_if_status_failure("NULL key fails after init",
                              nr_add_fiber_context_to_global_hashmap(
                                  test_fiber_global_map, NULL, NULL));

  nr_fiber_destroy_global_hashmap(&test_fiber_global_map);

  tlib_php_request_end();
}

static void test_add_fiber_context_happy_path(void) {
  nr_hashmap_t* test_fiber_global_map = NULL;
  ctx_globals_t* cg = NULL;
  tlib_php_request_start();

  cg = dummy_ctx_globals();

  nr_fiber_init_global_hashmap(&test_fiber_global_map);

  tlib_pass_if_status_success("adding a fiber context succeeds",
                              nr_add_fiber_context_to_global_hashmap(
                                  test_fiber_global_map, cg, "fiber-1"));

  tlib_pass_if_size_t_equal("hashmap has one entry after add", 1,
                            nr_hashmap_count(test_fiber_global_map));

  /*
   * Test : adding a second context increments the count.
   */
  tlib_pass_if_status_success("adding a second fiber context succeeds",
                              nr_add_fiber_context_to_global_hashmap(
                                  test_fiber_global_map, cg, "fiber-2"));

  tlib_pass_if_size_t_equal("hashmap has two entries", 2,
                            nr_hashmap_count(test_fiber_global_map));

  /* free_fiber_globals destructor cleans up the deep-copied snapshots. */
  nr_fiber_destroy_global_hashmap(&test_fiber_global_map);
  free_ctx_globals(cg);

  tlib_php_request_end();
}

static void test_remove_fiber_context_bad_input(void) {
  nr_hashmap_t* test_fiber_global_map = NULL;
  tlib_php_request_start();

  /*
   * Test : NULL key fails when hashmap is uninitialized.
   */
  tlib_pass_if_status_failure(
      "NULL key fails when hashmap is uninitialized",
      nr_remove_fiber_context_from_global_hashmap(test_fiber_global_map, NULL));

  /*
   * Test : Empty key fails (nr_strlen("") < 1).
   */
  tlib_pass_if_status_failure(
      "empty key fails",
      nr_remove_fiber_context_from_global_hashmap(test_fiber_global_map, ""));

  /*
   * Test : valid key with uninitialized hashmap fails.
   */
  tlib_pass_if_status_failure("uninitialized hashmap fails",
                              nr_remove_fiber_context_from_global_hashmap(
                                  test_fiber_global_map, "fiber-1"));

  /*
   * Test : removing a nonexistent key from an initialized hashmap fails.
   */
  nr_fiber_init_global_hashmap(&test_fiber_global_map);
  tlib_pass_if_status_failure("missing key fails",
                              nr_remove_fiber_context_from_global_hashmap(
                                  test_fiber_global_map, "does-not-exist"));

  nr_fiber_destroy_global_hashmap(&test_fiber_global_map);

  tlib_php_request_end();
}

static void test_remove_fiber_context_happy_path(void) {
  static const char* key = "fiber-remove";
  nr_hashmap_t* test_fiber_global_map = NULL;
  ctx_globals_t* cg = NULL;

  tlib_php_request_start();

  cg = dummy_ctx_globals();

  nr_fiber_init_global_hashmap(&test_fiber_global_map);
  tlib_pass_if_status_success(
      "add succeeds before remove",
      nr_add_fiber_context_to_global_hashmap(test_fiber_global_map, cg, key));

  tlib_pass_if_size_t_equal("hashmap has one entry before remove", 1,
                            nr_hashmap_count(test_fiber_global_map));

  tlib_pass_if_status_success(
      "remove succeeds for existing key",
      nr_remove_fiber_context_from_global_hashmap(test_fiber_global_map, key));

  tlib_pass_if_size_t_equal("hashmap is empty after remove", 0,
                            nr_hashmap_count(test_fiber_global_map));

  /*
   * Test : removing the same key again fails.
   */
  tlib_pass_if_status_failure(
      "remove fails when key has already been removed",
      nr_remove_fiber_context_from_global_hashmap(test_fiber_global_map, key));

  nr_fiber_destroy_global_hashmap(&test_fiber_global_map);
  free_ctx_globals(cg);

  tlib_php_request_end();
}

static void test_switch_global_context_bad_input(void) {
  nr_hashmap_t* test_fiber_global_map = NULL;
  fiber_globals_t* fiber_globals = NULL;
  /*
   * A non-NULL sentinel value used to detect that the failure paths actively
   * clear *fiber_global_ptr instead of leaving a stale pointer. The address
   * itself is never dereferenced.
   */
  fiber_globals_t stale_global_ptr;

  tlib_php_request_start();

  /*
   * Test : NULL fiber_global_ptr fails without crashing.
   */
  tlib_pass_if_status_failure(
      "NULL fiber_global_ptr fails",
      nr_fiber_switch_global_context(test_fiber_global_map, NULL, "fiber-1"));

  /*
   * Test : NULL key resolves to "main PHP context" and clears
   * *fiber_global_ptr to NULL with NR_SUCCESS.
   */
  fiber_globals = &stale_global_ptr;
  tlib_pass_if_status_success("NULL key succeeds (main context)",
                              nr_fiber_switch_global_context(
                                  test_fiber_global_map, &fiber_globals, NULL));
  tlib_pass_if_null("NULL key clears fiber_globals to NULL", fiber_globals);

  /*
   * Test : valid key with uninitialized hashmap fails AND clears any stale
   * fiber_globals pointer the caller may have held. Without the fix, a
   * caller's pointer would keep aliasing whatever fiber's snapshot was last
   * active, producing silent cross-fiber state corruption.
   */
  fiber_globals = &stale_global_ptr;
  tlib_pass_if_status_failure(
      "uninitialized hashmap fails",
      nr_fiber_switch_global_context(test_fiber_global_map, &fiber_globals,
                                     "fiber-1"));
  tlib_pass_if_null(
      "uninitialized hashmap failure path clears fiber_globals to NULL",
      fiber_globals);

  /*
   * Test : switching to a key that has no snapshot fails and clears any
   * stale fiber_globals pointer.
   */
  nr_fiber_init_global_hashmap(&test_fiber_global_map);
  fiber_globals = &stale_global_ptr;
  tlib_pass_if_status_failure(
      "missing key fails",
      nr_fiber_switch_global_context(test_fiber_global_map, &fiber_globals,
                                     "does-not-exist"));
  tlib_pass_if_null("missing key failure path clears fiber_globals to NULL",
                    fiber_globals);

  nr_fiber_destroy_global_hashmap(&test_fiber_global_map);

  tlib_php_request_end();
}

static void test_switch_global_context_happy_path(void) {
  static const char* key = "fiber-switch";
  nr_hashmap_t* test_fiber_global_map = NULL;
  fiber_globals_t* fiber_globals = NULL;
  ctx_globals_t* cg = NULL;

  tlib_php_request_start();

  cg = dummy_ctx_globals();

  nr_fiber_init_global_hashmap(&test_fiber_global_map);
  fiber_globals = NULL;

  tlib_pass_if_status_success(
      "add succeeds before switch",
      nr_add_fiber_context_to_global_hashmap(test_fiber_global_map, cg, key));

  tlib_pass_if_status_success("switch succeeds for an existing key",
                              nr_fiber_switch_global_context(
                                  test_fiber_global_map, &fiber_globals, key));

  tlib_pass_if_not_null("switch sets fiber_globals to a non-NULL snapshot",
                        fiber_globals);
  tlib_pass_if_not_null("the active snapshot has ctx_globals",
                        fiber_globals->ctx_globals);

  /*
   * destroy frees the snapshot via free_fiber_globals; clear the dangling
   * pointer first so other code in request shutdown does not dereference it.
   */
  nr_fiber_destroy_global_hashmap(&test_fiber_global_map);
  fiber_globals = NULL;
  free_ctx_globals(cg);

  tlib_php_request_end();
}

static void test_destroy_frees_entries(void) {
  nr_hashmap_t* test_fiber_global_map = NULL;
  ctx_globals_t* cg = NULL;

  tlib_php_request_start();

  cg = dummy_ctx_globals();

  nr_fiber_init_global_hashmap(&test_fiber_global_map);

  tlib_pass_if_status_success("add a context",
                              nr_add_fiber_context_to_global_hashmap(
                                  test_fiber_global_map, cg, "entry-a"));
  tlib_pass_if_status_success("add another context",
                              nr_add_fiber_context_to_global_hashmap(
                                  test_fiber_global_map, cg, "entry-b"));

  /*
   * Destroy invokes free_fiber_globals on every entry. We can't directly
   * assert the destructor fired, but we can confirm the hashmap pointer is
   * cleared and the request can complete cleanly with no leaks.
   */
  nr_fiber_destroy_global_hashmap(&test_fiber_global_map);
  tlib_pass_if_null("hashmap is cleared by destroy", test_fiber_global_map);

  free_ctx_globals(cg);

  tlib_php_request_end();
}

#endif /* PHP 8.1+ */

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("" PTSRMLS_CC);

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
  test_init_destroy_hashmap();
  test_copy_ctx_globals();
  test_copy_ctx_globals_null_strings();
  test_copy_ctx_globals_null_src();
  test_free_fiber_globals_null_paths();
  test_add_fiber_context_bad_input();
  test_add_fiber_context_null_src();
  test_add_fiber_context_happy_path();
  test_remove_fiber_context_bad_input();
  test_remove_fiber_context_happy_path();
  test_switch_global_context_bad_input();
  test_switch_global_context_happy_path();
  test_destroy_frees_entries();
#endif

  tlib_php_engine_destroy(TSRMLS_C);
}
