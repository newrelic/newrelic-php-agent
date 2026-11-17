/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_objtable.h"
#include "zend_exceptions.h"

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

static void test_null_and_bad_args() {
  nr_php_objtable_t* table;
  zval* not_object;

  tlib_php_request_start();

  table = nr_php_objtable_create(counting_dtor);
  not_object = tlib_php_request_eval_expr("42");
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

static void test_set_get_del_replace() {
  nr_php_objtable_t* table;
  zval* a;
  zval* b;

  tlib_php_request_start();
  table = nr_php_objtable_create(counting_dtor);
  dtor_count = 0;

  tlib_php_request_eval("$a = new stdClass; $b = new stdClass;");
  a = tlib_php_request_eval_expr("$a");
  b = tlib_php_request_eval_expr("$b");

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

static void test_last() {
  nr_php_objtable_t* table;
  zval* a;
  zval* b;
  zval* c;

  tlib_php_request_start();
  table = nr_php_objtable_create(counting_dtor);
  dtor_count = 0;

  tlib_php_request_eval(
      "$a = new stdClass; $b = new stdClass; $c = new stdClass;");
  a = tlib_php_request_eval_expr("$a");
  b = tlib_php_request_eval_expr("$b");
  c = tlib_php_request_eval_expr("$c");

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

  /* Clearing the mark keeps the entry and its value. */
  nr_php_objtable_clear_last(NULL);
  nr_php_objtable_clear_last(table);
  tlib_pass_if_size_t_equal("nothing marked, nothing changed", 0,
                            nr_php_objtable_count(table));
  nr_php_objtable_set(table, a, new_value("a"));
  nr_php_objtable_set(table, b, new_value("b"));
  nr_php_objtable_mark_last(table, a);
  dtor_count = 0;
  nr_php_objtable_clear_last(table);
  tlib_pass_if_int_equal("no value destroyed", 0, dtor_count);
  tlib_pass_if_null("mark cleared", nr_php_objtable_get_last(table));
  tlib_pass_if_str_equal("a kept", "a", nr_php_objtable_get(table, a));
  tlib_pass_if_str_equal("b kept", "b", nr_php_objtable_get(table, b));

  nr_php_zval_free(&a);
  nr_php_zval_free(&b);
  nr_php_zval_free(&c);
  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
}

#ifdef NR_PHP_OBJTABLE_WEAK

/* The mark is cleared when the marked object is freed. */
static void test_last_freed() {
  nr_php_objtable_t* table;
  zval* a;

  tlib_php_request_start();
  table = nr_php_objtable_create(counting_dtor);
  dtor_count = 0;

  tlib_php_request_eval("$a = new stdClass;");
  a = tlib_php_request_eval_expr("$a");
  nr_php_objtable_set(table, a, new_value("a"));
  nr_php_objtable_mark_last(table, a);
  nr_php_zval_free(&a);
  tlib_pass_if_str_equal("marked", "a", nr_php_objtable_get_last(table));

  tlib_php_request_eval("unset($a);");
  tlib_pass_if_null("cleared when freed", nr_php_objtable_get_last(table));
  tlib_pass_if_int_equal("value destroyed", 1, dtor_count);

  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
  tlib_pass_if_int_equal("not destroyed twice", 1, dtor_count);
}

/* The entry goes when the object is freed by refcount. */
static void test_freed_by_refcount() {
  nr_php_objtable_t* table;
  zval* a;

  tlib_php_request_start();
  table = nr_php_objtable_create(counting_dtor);
  dtor_count = 0;

  tlib_php_request_eval("$a = new stdClass;");
  a = tlib_php_request_eval_expr("$a");
  nr_php_objtable_set(table, a, new_value("a"));
  nr_php_zval_free(&a);

  tlib_pass_if_int_equal("alive", 0, dtor_count);
  tlib_php_request_eval("unset($a);");
  tlib_pass_if_int_equal("destroyed when freed", 1, dtor_count);
  tlib_pass_if_size_t_equal("entry removed", 0, nr_php_objtable_count(table));

  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
  tlib_pass_if_int_equal("not destroyed twice", 1, dtor_count);
}

/* The entry goes when the cycle collector frees the object. */
static void test_freed_by_cycle_collector() {
  nr_php_objtable_t* table;
  zval* a;

  tlib_php_request_start();
  table = nr_php_objtable_create(counting_dtor);
  dtor_count = 0;

  tlib_php_request_eval("$a = new stdClass; $a->self = $a;");
  a = tlib_php_request_eval_expr("$a");
  nr_php_objtable_set(table, a, new_value("a"));
  nr_php_zval_free(&a);

  tlib_php_request_eval("unset($a);");
  tlib_pass_if_int_equal("a cycle is not freed yet", 0, dtor_count);
  tlib_pass_if_size_t_equal("entry kept", 1, nr_php_objtable_count(table));

  tlib_php_request_eval("gc_collect_cycles();");
  tlib_pass_if_int_equal("destroyed by the collector", 1, dtor_count);
  tlib_pass_if_size_t_equal("entry removed", 0, nr_php_objtable_count(table));

  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
  tlib_pass_if_int_equal("not destroyed twice", 1, dtor_count);
}

/* A new object that reuses a freed handle finds no entry. */
static void test_handle_reuse() {
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

    tlib_php_request_eval("$a = new stdClass;");
    handle_a = tlib_php_request_eval_expr("spl_object_id($a)");
    a = tlib_php_request_eval_expr("$a");
    nr_php_objtable_set(table, a, new_value("a"));
    nr_php_zval_free(&a);
    tlib_php_request_eval("unset($a); $b = new stdClass;");

    handle_b = tlib_php_request_eval_expr("spl_object_id($b)");
    b = tlib_php_request_eval_expr("$b");
    if (Z_LVAL_P(handle_a) == Z_LVAL_P(handle_b)) {
      reused += 1;
    }
    tlib_pass_if_null("the new object has no entry",
                      nr_php_objtable_get(table, b));

    nr_php_zval_free(&b);
    nr_php_zval_free(&handle_a);
    nr_php_zval_free(&handle_b);
    tlib_php_request_eval("unset($b);");
  }

  /* The point of the test is a reused handle, so make sure one happened. */
  tlib_pass_if_true("a handle was reused", reused > 0, "reused=%d", reused);
  tlib_pass_if_int_equal("every value destroyed", 50, dtor_count);

  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
}

/*
 * Objects that are still alive at the end of the request. The
 * table is destroyed after the request, as it is in post-deactivate, and each
 * value is destroyed once.
 */
static void test_alive_at_request_end() {
  nr_php_objtable_t* table;
  zval* a;
  zval* b;

  tlib_php_request_start();
  table = nr_php_objtable_create(counting_dtor);
  dtor_count = 0;

  tlib_php_request_eval("$a = new stdClass; $b = new ArrayObject();");
  a = tlib_php_request_eval_expr("$a");
  b = tlib_php_request_eval_expr("$b");
  nr_php_objtable_set(table, a, new_value("a"));
  nr_php_objtable_set(table, b, new_value("b"));
  nr_php_zval_free(&a);
  nr_php_zval_free(&b);

  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
  tlib_pass_if_int_equal("each value destroyed once", 2, dtor_count);
}

/* exit() ends the request with objects still alive. */
static void test_exit() {
  nr_php_objtable_t* table;
  zval* a;
  char code[] = "exit(0);";

  tlib_php_request_start();
  table = nr_php_objtable_create(counting_dtor);
  dtor_count = 0;

  tlib_php_request_eval("$a = new stdClass;");
  a = tlib_php_request_eval_expr("$a");
  nr_php_objtable_set(table, a, new_value("a"));
  nr_php_zval_free(&a);

  zend_try {
    zend_eval_string(code, NULL, "-");
  }
  zend_end_try();

  /* From PHP 8.0 exit() leaves an internal exception pending. */
  if (EG(exception)) {
    zend_clear_exception();
  }

  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
  tlib_pass_if_int_equal("each value destroyed once", 1, dtor_count);
}

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO

/*
 * The table and its entries are the same inside a Fiber, after
 * it suspends and after it resumes.
 */
static void test_fiber_table_stable() {
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
      "$inner = $fiber->start();");
  from_main = tlib_php_request_eval_expr("$main");
  from_fiber = tlib_php_request_eval_expr("$inner");

  nr_php_objtable_set(table, from_main, new_value("main"));
  nr_php_objtable_set(table, from_fiber, new_value("fiber"));

  /* The Fiber is suspended. Resume it and let it finish. */
  tlib_pass_if_str_equal("main before resume", "main",
                         nr_php_objtable_get(table, from_main));
  tlib_pass_if_str_equal("fiber before resume", "fiber",
                         nr_php_objtable_get(table, from_fiber));
  tlib_php_request_eval("$fiber->resume();");
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
 * An object created in a Fiber that is destroyed before it
 * finishes is freed with the Fiber, and its entry goes with it.
 */
static void test_fiber_destroyed_unfinished() {
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
      "$inner = $fiber->start();");
  inner = tlib_php_request_eval_expr("$inner");
  nr_php_objtable_set(table, inner, new_value("inner"));
  nr_php_zval_free(&inner);

  /* The suspended Fiber and our copy of the object are the only owners. */
  tlib_php_request_eval("unset($inner);");
  tlib_pass_if_int_equal("alive while the Fiber holds it", 0, dtor_count);
  tlib_php_request_eval("unset($fiber);");
  tlib_pass_if_int_equal("destroyed with the Fiber", 1, dtor_count);
  tlib_pass_if_size_t_equal("entry removed", 0, nr_php_objtable_count(table));

  tlib_php_request_end();
  nr_php_objtable_destroy(&table);
  tlib_pass_if_int_equal("not destroyed twice", 1, dtor_count);
}

#endif /* PHP 8.1 */

#endif /* NR_PHP_OBJTABLE_WEAK */

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("");

  test_null_and_bad_args();
  test_set_get_del_replace();
  test_last();
#ifdef NR_PHP_OBJTABLE_WEAK
  test_last_freed();
  test_freed_by_refcount();
  test_freed_by_cycle_collector();
  test_handle_reuse();
  test_alive_at_request_end();
  test_exit();
#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
  test_fiber_table_stable();
  test_fiber_destroyed_unfinished();
#endif
#endif

  tlib_php_engine_destroy();
}
