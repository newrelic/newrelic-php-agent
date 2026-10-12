/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_objtable.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static int dtor_count = 0;

/* Like a real destructor, this only frees memory. */
static void counting_dtor(void* value) {
  dtor_count += 1;
  nr_free(value);
}

static char* new_value(const char* s) {
  return nr_strdup(s);
}

static void test_null_and_bad_args(TSRMLS_D) {
  nr_php_objtable_t* table;
  zval* not_object;

  tlib_php_request_start();

  table = nr_php_objtable_create(counting_dtor);
  not_object = tlib_php_request_eval_expr("42" TSRMLS_CC);
  dtor_count = 0;

  /* NULL table and NULL object: quiet, and the caller keeps the value. */
  nr_php_objtable_set(NULL, not_object, NULL);
  nr_php_objtable_set(table, NULL, NULL);
  tlib_pass_if_null("NULL table", nr_php_objtable_get(NULL, not_object));
  tlib_pass_if_null("NULL object", nr_php_objtable_get(table, NULL));
  nr_php_objtable_del(NULL, not_object);
  nr_php_objtable_del(table, NULL);
  tlib_pass_if_size_t_equal("NULL count", 0, nr_php_objtable_count(NULL));
  nr_php_objtable_destroy(NULL);
  tlib_pass_if_int_equal("nothing destroyed", 0, dtor_count);

  /* A value that is not an object is rejected, and the value is destroyed. */
  nr_php_objtable_set(table, not_object, new_value("rejected"));
  tlib_pass_if_int_equal("rejected value destroyed", 1, dtor_count);
  tlib_pass_if_null("not an object", nr_php_objtable_get(table, not_object));
  tlib_pass_if_size_t_equal("empty", 0, nr_php_objtable_count(table));

  nr_php_zval_free(&not_object);
  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
  tlib_pass_if_null("destroy clears the pointer", table);
}

static void test_set_get_del_replace(TSRMLS_D) {
  nr_php_objtable_t* table;
  zval* a;
  zval* b;

  tlib_php_request_start();
  table = nr_php_objtable_create(counting_dtor);
  dtor_count = 0;

  tlib_php_request_eval("$a = new stdClass; $b = new stdClass;" TSRMLS_CC);
  a = tlib_php_request_eval_expr("$a" TSRMLS_CC);
  b = tlib_php_request_eval_expr("$b" TSRMLS_CC);

  nr_php_objtable_set(table, a, new_value("a1"));
  nr_php_objtable_set(table, b, new_value("b1"));
  tlib_pass_if_size_t_equal("two entries", 2, nr_php_objtable_count(table));
  tlib_pass_if_str_equal("a", "a1", nr_php_objtable_get(table, a));
  tlib_pass_if_str_equal("b", "b1", nr_php_objtable_get(table, b));

  /* Setting again for an object that has an entry replaces it. */
  nr_php_objtable_set(table, a, new_value("a2"));
  tlib_pass_if_int_equal("replaced value destroyed", 1, dtor_count);
  tlib_pass_if_size_t_equal("still two", 2, nr_php_objtable_count(table));
  tlib_pass_if_str_equal("a replaced", "a2", nr_php_objtable_get(table, a));
  tlib_pass_if_str_equal("b untouched", "b1", nr_php_objtable_get(table, b));

  nr_php_objtable_del(table, a);
  tlib_pass_if_int_equal("deleted value destroyed", 2, dtor_count);
  tlib_pass_if_null("a deleted", nr_php_objtable_get(table, a));
  tlib_pass_if_str_equal("b kept", "b1", nr_php_objtable_get(table, b));

  /* Deleting what is not there changes nothing. */
  nr_php_objtable_del(table, a);
  tlib_pass_if_int_equal("no double destroy", 2, dtor_count);

  nr_php_zval_free(&a);
  nr_php_zval_free(&b);
  tlib_php_request_end();

  nr_php_objtable_destroy(&table);
  tlib_pass_if_int_equal("destroy frees the rest", 3, dtor_count);
}

static void test_last(TSRMLS_D) {
  nr_php_objtable_t* table;
  zval* a;
  zval* b;
  zval* c;

  tlib_php_request_start();
  table = nr_php_objtable_create(counting_dtor);
  dtor_count = 0;

  tlib_php_request_eval(
      "$a = new stdClass; $b = new stdClass; $c = new stdClass;" TSRMLS_CC);
  a = tlib_php_request_eval_expr("$a" TSRMLS_CC);
  b = tlib_php_request_eval_expr("$b" TSRMLS_CC);
  c = tlib_php_request_eval_expr("$c" TSRMLS_CC);

  /* Bad arguments and an empty table. */
  nr_php_objtable_mark_last(NULL, a);
  nr_php_objtable_mark_last(table, NULL);
  tlib_pass_if_null("NULL table", nr_php_objtable_get_last(NULL));
  tlib_pass_if_null("nothing marked", nr_php_objtable_get_last(table));

  nr_php_objtable_set(table, a, new_value("a"));
  nr_php_objtable_set(table, b, new_value("b"));

  /* An object without an entry is not marked. */
  nr_php_objtable_mark_last(table, c);
  tlib_pass_if_null("no entry, nothing marked",
                    nr_php_objtable_get_last(table));

  nr_php_objtable_mark_last(table, a);
  tlib_pass_if_str_equal("a is last", "a", nr_php_objtable_get_last(table));
  nr_php_objtable_mark_last(table, b);
  tlib_pass_if_str_equal("b is last", "b", nr_php_objtable_get_last(table));

  /* Marking an object without an entry leaves the mark alone. */
  nr_php_objtable_mark_last(table, c);
  tlib_pass_if_str_equal("mark kept", "b", nr_php_objtable_get_last(table));

  /* Deleting another entry leaves the mark alone. */
  nr_php_objtable_del(table, a);
  tlib_pass_if_str_equal("other delete", "b", nr_php_objtable_get_last(table));

  /* Replacing the marked entry clears the mark until it is marked again. */
  nr_php_objtable_set(table, b, new_value("b2"));
  tlib_pass_if_null("replaced", nr_php_objtable_get_last(table));
  nr_php_objtable_mark_last(table, b);
  tlib_pass_if_str_equal("marked again", "b2", nr_php_objtable_get_last(table));

  /* Deleting the marked entry clears the mark. */
  nr_php_objtable_del(table, b);
  tlib_pass_if_null("deleted", nr_php_objtable_get_last(table));

  nr_php_zval_free(&a);
  nr_php_zval_free(&b);
  nr_php_zval_free(&c);
  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
}

#ifdef NR_PHP_OBJTABLE_WEAK

/* The mark is cleared when the marked object is freed. */
static void test_last_freed(TSRMLS_D) {
  nr_php_objtable_t* table;
  zval* a;

  tlib_php_request_start();
  table = nr_php_objtable_create(counting_dtor);
  dtor_count = 0;

  tlib_php_request_eval("$a = new stdClass;" TSRMLS_CC);
  a = tlib_php_request_eval_expr("$a" TSRMLS_CC);
  nr_php_objtable_set(table, a, new_value("a"));
  nr_php_objtable_mark_last(table, a);
  nr_php_zval_free(&a);
  tlib_pass_if_str_equal("marked", "a", nr_php_objtable_get_last(table));

  tlib_php_request_eval("unset($a);" TSRMLS_CC);
  tlib_pass_if_null("cleared when freed", nr_php_objtable_get_last(table));
  tlib_pass_if_int_equal("value destroyed", 1, dtor_count);

  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
  tlib_pass_if_int_equal("not destroyed twice", 1, dtor_count);
}

/* Runtime check 1: the entry goes when the object is freed by refcount. */
static void test_freed_by_refcount(TSRMLS_D) {
  nr_php_objtable_t* table;
  zval* a;

  tlib_php_request_start();
  table = nr_php_objtable_create(counting_dtor);
  dtor_count = 0;

  tlib_php_request_eval("$a = new stdClass;" TSRMLS_CC);
  a = tlib_php_request_eval_expr("$a" TSRMLS_CC);
  nr_php_objtable_set(table, a, new_value("a"));
  nr_php_zval_free(&a);

  tlib_pass_if_int_equal("alive", 0, dtor_count);
  tlib_php_request_eval("unset($a);" TSRMLS_CC);
  tlib_pass_if_int_equal("destroyed when freed", 1, dtor_count);
  tlib_pass_if_size_t_equal("entry removed", 0, nr_php_objtable_count(table));

  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
  tlib_pass_if_int_equal("not destroyed twice", 1, dtor_count);
}

/* Runtime check 1: the entry goes when the cycle collector frees the object. */
static void test_freed_by_cycle_collector(TSRMLS_D) {
  nr_php_objtable_t* table;
  zval* a;

  tlib_php_request_start();
  table = nr_php_objtable_create(counting_dtor);
  dtor_count = 0;

  tlib_php_request_eval("$a = new stdClass; $a->self = $a;" TSRMLS_CC);
  a = tlib_php_request_eval_expr("$a" TSRMLS_CC);
  nr_php_objtable_set(table, a, new_value("a"));
  nr_php_zval_free(&a);

  tlib_php_request_eval("unset($a);" TSRMLS_CC);
  tlib_pass_if_int_equal("a cycle is not freed yet", 0, dtor_count);
  tlib_pass_if_size_t_equal("entry kept", 1, nr_php_objtable_count(table));

  tlib_php_request_eval("gc_collect_cycles();" TSRMLS_CC);
  tlib_pass_if_int_equal("destroyed by the collector", 1, dtor_count);
  tlib_pass_if_size_t_equal("entry removed", 0, nr_php_objtable_count(table));

  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
  tlib_pass_if_int_equal("not destroyed twice", 1, dtor_count);
}

/* Runtime check 2: a new object that reuses a freed handle finds no entry. */
static void test_handle_reuse(TSRMLS_D) {
  nr_php_objtable_t* table;
  zval* a;
  zval* b;
  int reused = 0;
  int i;

  tlib_php_request_start();
  table = nr_php_objtable_create(counting_dtor);
  dtor_count = 0;

  for (i = 0; i < 50; i++) {
    zval* handle_a;
    zval* handle_b;

    tlib_php_request_eval("$a = new stdClass;" TSRMLS_CC);
    handle_a = tlib_php_request_eval_expr("spl_object_id($a)" TSRMLS_CC);
    a = tlib_php_request_eval_expr("$a" TSRMLS_CC);
    nr_php_objtable_set(table, a, new_value("a"));
    nr_php_zval_free(&a);
    tlib_php_request_eval("unset($a); $b = new stdClass;" TSRMLS_CC);

    handle_b = tlib_php_request_eval_expr("spl_object_id($b)" TSRMLS_CC);
    b = tlib_php_request_eval_expr("$b" TSRMLS_CC);
    if (Z_LVAL_P(handle_a) == Z_LVAL_P(handle_b)) {
      reused += 1;
    }
    tlib_pass_if_null("the new object has no entry",
                      nr_php_objtable_get(table, b));

    nr_php_zval_free(&b);
    nr_php_zval_free(&handle_a);
    nr_php_zval_free(&handle_b);
    tlib_php_request_eval("unset($b);" TSRMLS_CC);
  }

  /* The point of the test is a reused handle, so make sure one happened. */
  tlib_pass_if_true("a handle was reused", reused > 0, "reused=%d", reused);
  tlib_pass_if_int_equal("every value destroyed", 50, dtor_count);

  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
}

/*
 * Runtime check 3: objects that are still alive at the end of the request. The
 * table is destroyed after the request, as it is in post-deactivate, and each
 * value is destroyed once.
 */
static void test_alive_at_request_end(TSRMLS_D) {
  nr_php_objtable_t* table;
  zval* a;
  zval* b;

  tlib_php_request_start();
  table = nr_php_objtable_create(counting_dtor);
  dtor_count = 0;

  tlib_php_request_eval("$a = new stdClass; $b = new ArrayObject();" TSRMLS_CC);
  a = tlib_php_request_eval_expr("$a" TSRMLS_CC);
  b = tlib_php_request_eval_expr("$b" TSRMLS_CC);
  nr_php_objtable_set(table, a, new_value("a"));
  nr_php_objtable_set(table, b, new_value("b"));
  nr_php_zval_free(&a);
  nr_php_zval_free(&b);

  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
  tlib_pass_if_int_equal("each value destroyed once", 2, dtor_count);
}

/*
 * Runtime check 3: a fatal error bails out of the request. The engine still
 * frees the objects, then the table is destroyed after the request, and each
 * value is destroyed once.
 */
static void test_fatal_error(TSRMLS_D) {
  nr_php_objtable_t* table;
  zval* a;
  char code[] = "trigger_error('fatal', E_USER_ERROR);";

  tlib_php_request_start();
  table = nr_php_objtable_create(counting_dtor);
  dtor_count = 0;

  tlib_php_request_eval("$a = new stdClass;" TSRMLS_CC);
  a = tlib_php_request_eval_expr("$a" TSRMLS_CC);
  nr_php_objtable_set(table, a, new_value("a"));
  nr_php_zval_free(&a);

  zend_try {
    zend_eval_string(code, NULL, "-" TSRMLS_CC);
  }
  zend_end_try();

  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
  tlib_pass_if_int_equal("each value destroyed once", 1, dtor_count);
}

/* Runtime check 3: exit() ends the request with objects still alive. */
static void test_exit(TSRMLS_D) {
  nr_php_objtable_t* table;
  zval* a;
  char code[] = "exit(0);";

  tlib_php_request_start();
  table = nr_php_objtable_create(counting_dtor);
  dtor_count = 0;

  tlib_php_request_eval("$a = new stdClass;" TSRMLS_CC);
  a = tlib_php_request_eval_expr("$a" TSRMLS_CC);
  nr_php_objtable_set(table, a, new_value("a"));
  nr_php_zval_free(&a);

  zend_try {
    zend_eval_string(code, NULL, "-" TSRMLS_CC);
  }
  zend_end_try();

  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
  tlib_pass_if_int_equal("each value destroyed once", 1, dtor_count);
}

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO

/*
 * Runtime check 5: the table and its entries are the same inside a Fiber, after
 * it suspends and after it resumes.
 */
static void test_fiber_table_stable(TSRMLS_D) {
  nr_php_objtable_t* table;
  zval* from_fiber;
  zval* from_main;

  tlib_php_request_start();
  table = nr_php_objtable_create(counting_dtor);
  dtor_count = 0;

  tlib_php_request_eval(
      "$main = new stdClass;"
      "$fiber = new Fiber(function () {"
      "  $inner = new stdClass;"
      "  Fiber::suspend($inner);"
      "  return 1;"
      "});"
      "$inner = $fiber->start();" TSRMLS_CC);
  from_main = tlib_php_request_eval_expr("$main" TSRMLS_CC);
  from_fiber = tlib_php_request_eval_expr("$inner" TSRMLS_CC);

  nr_php_objtable_set(table, from_main, new_value("main"));
  nr_php_objtable_set(table, from_fiber, new_value("fiber"));

  /* The Fiber is suspended. Resume it and let it finish. */
  tlib_pass_if_str_equal("main before resume", "main",
                         nr_php_objtable_get(table, from_main));
  tlib_pass_if_str_equal("fiber before resume", "fiber",
                         nr_php_objtable_get(table, from_fiber));
  tlib_php_request_eval("$fiber->resume();" TSRMLS_CC);
  tlib_pass_if_str_equal("main after resume", "main",
                         nr_php_objtable_get(table, from_main));
  tlib_pass_if_str_equal("fiber after resume", "fiber",
                         nr_php_objtable_get(table, from_fiber));
  tlib_pass_if_size_t_equal("two entries", 2, nr_php_objtable_count(table));

  nr_php_zval_free(&from_main);
  nr_php_zval_free(&from_fiber);
  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
  tlib_pass_if_int_equal("each value destroyed once", 2, dtor_count);
}

/*
 * Runtime check 5: an object created in a Fiber that is destroyed before it
 * finishes is freed with the Fiber, and its entry goes with it.
 */
static void test_fiber_destroyed_unfinished(TSRMLS_D) {
  nr_php_objtable_t* table;
  zval* inner;

  tlib_php_request_start();
  table = nr_php_objtable_create(counting_dtor);
  dtor_count = 0;

  tlib_php_request_eval(
      "$fiber = new Fiber(function () {"
      "  $inner = new stdClass;"
      "  Fiber::suspend($inner);"
      "});"
      "$inner = $fiber->start();" TSRMLS_CC);
  inner = tlib_php_request_eval_expr("$inner" TSRMLS_CC);
  nr_php_objtable_set(table, inner, new_value("inner"));
  nr_php_zval_free(&inner);

  /* The suspended Fiber and our copy of the object are the only owners. */
  tlib_php_request_eval("unset($inner);" TSRMLS_CC);
  tlib_pass_if_int_equal("alive while the Fiber holds it", 0, dtor_count);
  tlib_php_request_eval("unset($fiber);" TSRMLS_CC);
  tlib_pass_if_int_equal("destroyed with the Fiber", 1, dtor_count);
  tlib_pass_if_size_t_equal("entry removed", 0, nr_php_objtable_count(table));

  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
  tlib_pass_if_int_equal("not destroyed twice", 1, dtor_count);
}

#endif /* PHP 8.1 */

#endif /* NR_PHP_OBJTABLE_WEAK */

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("" PTSRMLS_CC);

  test_null_and_bad_args(TSRMLS_C);
  test_set_get_del_replace(TSRMLS_C);
  test_last(TSRMLS_C);
#ifdef NR_PHP_OBJTABLE_WEAK
  test_last_freed(TSRMLS_C);
  test_freed_by_refcount(TSRMLS_C);
  test_freed_by_cycle_collector(TSRMLS_C);
  test_handle_reuse(TSRMLS_C);
  test_alive_at_request_end(TSRMLS_C);
  test_fatal_error(TSRMLS_C);
  test_exit(TSRMLS_C);
#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
  test_fiber_table_stable(TSRMLS_C);
  test_fiber_destroyed_unfinished(TSRMLS_C);
#endif
#endif

  tlib_php_engine_destroy(TSRMLS_C);
}
