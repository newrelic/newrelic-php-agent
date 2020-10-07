/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_segment_traces.h"
#include "nr_segment_tree.h"
#include "nr_segment_private.h"
#include "nr_span_event_private.h"
#include "nr_txn_private.h"
#include "test_segment_helpers.h"
#include "util_number_converter.h"
#include "util_set.h"

#include "tlib_main.h"

#define assert_null_result(M, RESULT)                \
  do {                                               \
    const char* anr_msg = (M);                       \
    const nrtxnfinal_t anr_res = (RESULT);           \
                                                     \
    tlib_pass_if_null(anr_msg, anr_res.trace_json);  \
    tlib_pass_if_null(anr_msg, anr_res.span_events); \
  } while (0)

static void test_finalise_bad_params(void) {
  nrtxn_t txn = {.abs_start_time = 1000};
  size_t trace_limit = 1;
  size_t span_limit = 1;

  /*
   * Test : Bad parameters
   */
  assert_null_result(
      "Traversing the segments of a NULL transaction must NOT populate a "
      "result",
      nr_segment_tree_finalise(NULL, trace_limit, span_limit, NULL, NULL));

  assert_null_result(
      "Traversing a segment-less transaction must NOT populate a result",
      nr_segment_tree_finalise(&txn, trace_limit, span_limit, NULL, NULL));
}

static void test_finalise_span_priority(void) {
  nrtxn_t txn = {0};
  nrtxnfinal_t result;
  nr_span_event_t* span;
  nr_segment_t* root;
  nr_segment_t* custom;
  nr_segment_t* external;
  nr_segment_t* long_seg;

  /* Mock up the transaction */
  txn.abs_start_time = 1000;
  txn.distributed_trace = nr_distributed_trace_create();
  nr_distributed_trace_set_sampled(txn.distributed_trace, true);
  txn.options.distributed_tracing_enabled = true;
  txn.options.span_events_enabled = true;

  txn.segment_count = 1;
  txn.segment_slab = nr_slab_create(sizeof(nr_segment_t), 0);
  txn.trace_strings = nr_string_pool_create();
  txn.scoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  txn.unscoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  txn.options.tt_threshold = 5000;

  /* Mock up a long custom segment */
  long_seg = nr_slab_next(txn.segment_slab);
  long_seg->name = nr_string_add(txn.trace_strings, "Long");
  long_seg->txn = &txn;
  long_seg->start_time = 2000;
  long_seg->stop_time = 20000;

  /* Mock up the external segment */
  external = nr_slab_next(txn.segment_slab);
  external->name = nr_string_add(txn.trace_strings, "External");
  external->txn = &txn;
  external->start_time = 2000;
  external->stop_time = 8000;
  external->id = nr_strdup("id");

  /* Mock up the custom segment */
  custom = nr_slab_next(txn.segment_slab);
  custom->name = nr_string_add(txn.trace_strings, "Custom");
  custom->txn = &txn;
  custom->start_time = 1000;
  custom->stop_time = 9000;
  nr_segment_children_init(&custom->children);
  nr_segment_add_child(custom, external);

  /* Mock up the root segment */
  root = nr_slab_next(txn.segment_slab);
  root->name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  root->txn = &txn;
  root->start_time = 0;
  root->stop_time = 20000;
  nr_segment_children_init(&root->children);
  nr_segment_add_child(root, custom);
  nr_segment_add_child(root, long_seg);

  txn.segment_root = root;
  txn.segment_count = 4;

  nr_segment_set_priority_flag(root, NR_SEGMENT_PRIORITY_ROOT);
  nr_segment_set_priority_flag(external, NR_SEGMENT_PRIORITY_DT);

  /*
   * Our internal heap implementation doesn't allow for a heap of size
   * 1. That's why we start with a span limit of 2 here.
   */

  /*
   * Test : External and root segments should be kept.
   */
  result = nr_segment_tree_finalise(&txn, 0, 2, NULL, NULL);
  tlib_pass_if_int_equal("2 span events: root and external",
                         nr_vector_size(result.span_events), 2);
  span = (nr_span_event_t*)nr_vector_get(result.span_events, 0);
  tlib_pass_if_str_equal("2 span events: root and external",
                         nr_span_event_get_name(span), "WebTransaction/*");
  span = (nr_span_event_t*)nr_vector_get(result.span_events, 1);
  tlib_pass_if_str_equal("2 span events: root and external",
                         nr_span_event_get_name(span), "External");

  nr_txn_final_destroy_fields(&result);

  /*
   * Test : External, root and the longest custom segments should be kept.
   */
  result = nr_segment_tree_finalise(&txn, 0, 3, NULL, NULL);
  tlib_pass_if_int_equal("3 span events: root, external and long custom",
                         nr_vector_size(result.span_events), 3);
  span = (nr_span_event_t*)nr_vector_get(result.span_events, 0);
  tlib_pass_if_str_equal("3 span events: root, external and long custom",
                         nr_span_event_get_name(span), "WebTransaction/*");
  span = (nr_span_event_t*)nr_vector_get(result.span_events, 1);
  tlib_pass_if_str_equal("3 span events: root, external and long custom",
                         nr_span_event_get_name(span), "External");
  span = (nr_span_event_t*)nr_vector_get(result.span_events, 2);
  tlib_pass_if_str_equal("two span events: root and external",
                         nr_span_event_get_name(span), "Long");

  nr_txn_final_destroy_fields(&result);

  /*
   * Test : All segments should be kept.
   */
  result = nr_segment_tree_finalise(&txn, 0, 4, NULL, NULL);
  tlib_pass_if_int_equal("all span events", nr_vector_size(result.span_events),
                         4);
  span = (nr_span_event_t*)nr_vector_get(result.span_events, 0);
  tlib_pass_if_str_equal("all span events", nr_span_event_get_name(span),
                         "WebTransaction/*");
  span = (nr_span_event_t*)nr_vector_get(result.span_events, 1);
  tlib_pass_if_str_equal("all span events", nr_span_event_get_name(span),
                         "Custom");
  span = (nr_span_event_t*)nr_vector_get(result.span_events, 2);
  tlib_pass_if_str_equal("all span events", nr_span_event_get_name(span),
                         "External");
  span = (nr_span_event_t*)nr_vector_get(result.span_events, 3);
  tlib_pass_if_str_equal("two span events: root and external",
                         nr_span_event_get_name(span), "Long");

  nr_txn_final_destroy_fields(&result);
  nr_txn_destroy_fields(&txn);
}

static void test_finalise_one_only_with_metrics(void) {
  nrtxn_t txn = {.abs_start_time = 1000};
  size_t trace_limit = 1;
  size_t span_limit = 0;
  nrtxnfinal_t result;
  nrobj_t* obj;

  nr_segment_t* root;

  /* Mock up the transaction */
  txn.segment_slab = nr_slab_create(sizeof(nr_segment_t), 0);
  txn.segment_count = 1;
  txn.trace_strings = nr_string_pool_create();
  txn.scoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  txn.unscoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);

  /* Mock up the segment */
  root = nr_slab_next(txn.segment_slab);
  root->txn = &txn;
  root->start_time = 0;
  root->stop_time = 3000;
  root->name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  nr_segment_add_metric(root, "Custom/Unscoped", false);
  nr_segment_add_metric(root, "Custom/Scoped", true);

  txn.segment_root = root;

  txn.options.tt_threshold = 5000;

  /*
   * Test : A too-short transaction does not yield a trace.
   */
  result = nr_segment_tree_finalise(&txn, trace_limit, span_limit, NULL, NULL);
  tlib_pass_if_null(
      "Traversing the segments of a should-not-trace transaction must NOT "
      "populate a trace JSON result",
      result.trace_json);
  test_metric_created(
      "Traversing the segments of a should-not-trace transaction must create a "
      "specific unscoped metric",
      txn.unscoped_metrics, 0, 3000, "Custom/Unscoped");
  test_metric_created(
      "Traversing the segments of a should-not-trace transaction must create a "
      "specific scoped metric",
      txn.scoped_metrics, 0, 3000, "Custom/Scoped");

  nr_txn_final_destroy_fields(&result);
  nrm_table_destroy(&txn.scoped_metrics);
  nrm_table_destroy(&txn.unscoped_metrics);
  txn.scoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  txn.unscoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);

  /*
   * Test : A zero limit does not yield a trace.
   */

  /* Make the transaction long enough so that a trace should be made */
  root->stop_time = 9000;

  result = nr_segment_tree_finalise(&txn, 0, span_limit, NULL, NULL);
  tlib_pass_if_null(
      "Traversing the segments of a 0-limit trace must NOT populate a trace "
      "JSON result",
      result.trace_json);
  test_metric_created(
      "Traversing the segments of a 0-limit transaction must create a "
      "specific unscoped metric",
      txn.unscoped_metrics, 0, 9000, "Custom/Unscoped");
  test_metric_created(
      "Traversing the segments of a 0-limit transaction must create a "
      "specific scoped metric",
      txn.scoped_metrics, 0, 9000, "Custom/Scoped");

  nr_txn_final_destroy_fields(&result);
  nrm_table_destroy(&txn.scoped_metrics);
  nrm_table_destroy(&txn.unscoped_metrics);
  txn.scoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  txn.unscoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);

  /*
   * Test : Normal operation
   */

  result = nr_segment_tree_finalise(&txn, trace_limit, span_limit, NULL, NULL);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must populate a "
      "trace JSON result",
      result.trace_json);

  tlib_pass_if_str_equal(
      "Traversing the segments of a should-trace transaction must create "
      "expected trace JSON",
      result.trace_json,
      "[[0,{},{},[0,9,\"ROOT\",{},[[0,9,\"`0\",{},[]]]],{}],["
      "\"WebTransaction\\/*\"]]");

  obj = nro_create_from_json(result.trace_json);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must create valid "
      "JSON",
      obj);

  test_metric_created(
      "Traversing the segments of a should-trace transaction must create a "
      "specific unscoped metric",
      txn.unscoped_metrics, 0, 9000, "Custom/Unscoped");
  test_metric_created(
      "Traversing the segments of a should-trace transaction must create a "
      "specific scoped metric",
      txn.scoped_metrics, 0, 9000, "Custom/Scoped");

  nro_delete(obj);
  nr_txn_final_destroy_fields(&result);
  nrm_table_destroy(&txn.scoped_metrics);
  nrm_table_destroy(&txn.unscoped_metrics);
  nr_string_pool_destroy(&txn.trace_strings);

  nr_segment_destroy_tree(txn.segment_root);
  nr_slab_destroy(&txn.segment_slab);
}

#define NR_TEST_SEGMENT_TREE_SIZE 4
static void test_finalise(void) {
  int i;
  nrobj_t* obj;
  nr_segment_t* current;

  nrtxn_t txn = {.abs_start_time = 1000};

  nrtime_t start_time = 0;
  nrtime_t stop_time = 9000;

  size_t trace_limit = NR_TEST_SEGMENT_TREE_SIZE;
  size_t span_limit = 0;
  char* segment_names[NR_TEST_SEGMENT_TREE_SIZE];

  nrtxnfinal_t result;
  nr_segment_t* root;

  txn.segment_slab = nr_slab_create(sizeof(nr_segment_t), 0);
  txn.trace_strings = nr_string_pool_create();
  txn.scoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  txn.unscoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);

  root = nr_slab_next(txn.segment_slab);
  root->txn = &txn;
  root->start_time = start_time;
  root->stop_time = stop_time;
  root->name = nr_string_add(txn.trace_strings, "WebTransaction/*");

  txn.segment_root = root;
  current = root;

  for (i = 0; i < NR_TEST_SEGMENT_TREE_SIZE; i++) {
    nr_segment_t* segment = nr_slab_next(txn.segment_slab);

    segment_names[i] = nr_alloca(5 * sizeof(char));
    nr_itoa(segment_names[i], 5, i);

    segment->start_time = start_time + ((i + 1) * 1000);
    segment->stop_time = stop_time - ((i + 1) * 1000);
    segment->name = nr_string_add(txn.trace_strings, segment_names[i]);
    segment->txn = &txn;

    nr_segment_add_metric(segment, segment_names[i], false);
    nr_segment_add_metric(segment, segment_names[i], true);

    nr_segment_children_init(&current->children);
    nr_segment_add_child(current, segment);

    current = segment;
  }

  txn.segment_count = NR_TEST_SEGMENT_TREE_SIZE;

  result = nr_segment_tree_finalise(&txn, trace_limit, span_limit, NULL, NULL);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-sample transaction must populate a "
      "result",
      result.trace_json);

  tlib_pass_if_str_equal(
      "Traversing the segments of a should-trace transaction must create "
      "expected trace JSON with all "
      "segments",
      result.trace_json,
      "[[0,{},{},[0,9,\"ROOT\",{},[[0,9,\"`0\",{},[[1,8,\"`1\",{},[[2,7,\"`2\","
      "{},[[3,6,\"`3\",{},[[4,5,\"`4\",{},[]]]]]]]]]]]],{}],["
      "\"WebTransaction\\/*\",\"0\",\"1\",\"2\",\"3\"]]");

  obj = nro_create_from_json(result.trace_json);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must create valid "
      "JSON",
      obj);

  for (i = 0; i < NR_TEST_SEGMENT_TREE_SIZE; i++) {
    nrtime_t expected_duration = nr_time_duration(start_time + ((i + 1) * 1000),
                                                  stop_time - ((i + 1) * 1000));

    test_metric_created_ex(
        "Traversing the segments of a should-trace transaction must create "
        "unscoped metrics as needed",
        txn.unscoped_metrics, 0, expected_duration, i == 3 ? 1000 : 2000,
        segment_names[i]);

    test_metric_created_ex(
        "Traversing the segments of a should-trace transaction must create "
        "scoped metrics as needed",
        txn.scoped_metrics, 0, expected_duration, i == 3 ? 1000 : 2000,
        segment_names[i]);
  }

  nro_delete(obj);
  nr_txn_final_destroy_fields(&result);
  nrm_table_destroy(&txn.scoped_metrics);
  nrm_table_destroy(&txn.unscoped_metrics);
  nr_string_pool_destroy(&txn.trace_strings);

  nr_segment_destroy_tree(txn.segment_root);
  nr_slab_destroy(&txn.segment_slab);
}

typedef struct {
  const nrtxn_t* txn;
  nrtime_t total_time;
  size_t call_count;
} test_finalise_callback_expected_t;

static void test_finalise_callback(nrtxn_t* txn,
                                   nrtime_t total_time,
                                   void* userdata) {
  test_finalise_callback_expected_t* expected
      = (test_finalise_callback_expected_t*)userdata;

  tlib_pass_if_ptr_equal(
      "A registered finalise callback must get the correct transaction",
      expected->txn, txn);
  tlib_pass_if_time_equal(
      "A registered finalise callback must get the correct total time",
      expected->total_time, total_time);

  expected->call_count += 1;
}

static void test_finalise_total_time(void) {
  nrobj_t* obj;
  nr_segment_t *a, *b, *c, *d;
  test_finalise_callback_expected_t cb_userdata = {.call_count = 0};

  nrtxn_t txn = {.abs_start_time = 1000};

  size_t trace_limit = 10;
  size_t span_limit = 0;

  nrtxnfinal_t result;
  nr_segment_t* root;

  txn.segment_slab = nr_slab_create(sizeof(nr_segment_t), 0);
  txn.trace_strings = nr_string_pool_create();
  txn.scoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  txn.unscoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  txn.options.tt_threshold = 0;
  txn.status.recording = 1;

  root = nr_slab_next(txn.segment_slab);
  root->txn = &txn;
  root->name = nr_string_add(txn.trace_strings, "WebTransaction/*");

  txn.segment_root = root;

  /*
   * In order to exercise the total and exclusive time calculation, we're going
   * to set up a basic async structure:
   *
   * time (ms): 0    10    20    30    40    50
   *            ROOT------------------------->
   *                 a (ctx 1)--------->
   *                       b (ctx 1)--->
   *                 c (ctx 2)--------->
   *                             d (ctx 2)--->
   *
   * On the main context, there is only the ROOT segment, which lasts 50 ms.
   *
   * Context 1 has two segments, which sum to an exclusive time of 30 ms: a has
   * an exclusive time of 10 ms, and b has an exclusive time of 20 ms.
   *
   * Context 2 has two segments, which also sum to an exclusive time of 40 ms: c
   * has an exclusive time of 20 ms, and d has an exclusive time of 20 ms.
   *
   * Therefore, we should have a total time of 120 ms, and a duration of 50 ms.
   */
  nr_segment_set_timing(root, 0, 50 * NR_TIME_DIVISOR_MS);

  a = nr_segment_start(&txn, root, "1");
  nr_segment_set_name(a, "a");
  nr_segment_set_timing(a, 10 * NR_TIME_DIVISOR_MS, 30 * NR_TIME_DIVISOR_MS);
  b = nr_segment_start(&txn, a, "1");
  nr_segment_set_name(b, "b");
  nr_segment_set_timing(b, 20 * NR_TIME_DIVISOR_MS, 20 * NR_TIME_DIVISOR_MS);
  c = nr_segment_start(&txn, root, "2");
  nr_segment_set_name(c, "c");
  nr_segment_set_timing(c, 10 * NR_TIME_DIVISOR_MS, 30 * NR_TIME_DIVISOR_MS);
  d = nr_segment_start(&txn, c, "2");
  nr_segment_set_name(d, "d");
  nr_segment_set_timing(d, 30 * NR_TIME_DIVISOR_MS, 20 * NR_TIME_DIVISOR_MS);

  nr_segment_end(&a);
  nr_segment_end(&b);
  nr_segment_end(&c);
  nr_segment_end(&d);
  nr_segment_end(&root);

  cb_userdata.txn = &txn;
  cb_userdata.total_time = 120 * NR_TIME_DIVISOR_MS;
  result = nr_segment_tree_finalise(&txn, trace_limit, span_limit,
                                    test_finalise_callback, &cb_userdata);
  tlib_pass_if_size_t_equal(
      "Traversing the segments of a should-sample transaction must invoke the "
      "finalise callback",
      1, cb_userdata.call_count);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-sample transaction must populate a "
      "result",
      result.trace_json);

  tlib_pass_if_str_equal(
      "Traversing the segments of a should-trace transaction must create "
      "expected trace JSON with all "
      "segments",
      result.trace_json,
      // clang-format off
      "["
        "[0,{},{},"
          "[0,50,\"ROOT\",{},["
            "[0,50,\"`0\",{},["
              "[10,40,\"`1\",{\"async_context\":\"`2\"},["
                "[20,40,\"`3\",{\"async_context\":\"`2\"},[]]"
              "]],"
              "[10,40,\"`4\",{\"async_context\":\"`5\"},["
                "[30,50,\"`6\",{\"async_context\":\"`5\"},[]]"
              "]]"
            "]]"
          "]]"
        ",{}]"
        ","
        "[\"WebTransaction\\/*\",\"a\",\"1\",\"b\",\"c\",\"2\",\"d\"]"
      "]"
      // clang-format on
  );

  obj = nro_create_from_json(result.trace_json);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must create valid "
      "JSON",
      obj);

  nro_delete(obj);
  nr_txn_final_destroy_fields(&result);
  nrm_table_destroy(&txn.scoped_metrics);
  nrm_table_destroy(&txn.unscoped_metrics);
  nr_string_pool_destroy(&txn.trace_strings);

  nr_segment_destroy_tree(txn.segment_root);
  nr_slab_destroy(&txn.segment_slab);
}

static void test_finalise_total_time_discounted_async(void) {
  nrobj_t* obj;
  nr_segment_t *a, *b, *c, *d;
  test_finalise_callback_expected_t cb_userdata = {.call_count = 0};

  nrtxn_t txn = {.abs_start_time = 1000};

  size_t trace_limit = 10;
  size_t span_limit = 0;

  nrtxnfinal_t result;
  nr_segment_t* root;

  txn.segment_slab = nr_slab_create(sizeof(nr_segment_t), 0);
  txn.trace_strings = nr_string_pool_create();
  txn.scoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  txn.unscoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  txn.options.tt_threshold = 0;
  txn.options.discount_main_context_blocking = true;
  txn.status.recording = 1;

  root = nr_slab_next(txn.segment_slab);
  root->txn = &txn;
  root->name = nr_string_add(txn.trace_strings, "WebTransaction/*");

  txn.segment_root = root;

  /*
   * In order to exercise the total and exclusive time calculation, we're going
   * to set up a basic async structure:
   *
   * time (ms): 0    10    20    30    40    50
   *            ROOT------------------------->
   *                 a (ctx 1)--------->
   *                       b (ctx 1)--->
   *                 c (ctx 2)--------->
   *                             d (ctx 2)--->
   *
   * On the main context, there is only the ROOT segment, which lasts 50 ms.
   *
   * Context 1 has two segments, which sum to an exclusive time of 30 ms: a has
   * an exclusive time of 10 ms, and b has an exclusive time of 20 ms.
   *
   * Context 2 has two segments, which also sum to an exclusive time of 40 ms: c
   * has an exclusive time of 20 ms, and d has an exclusive time of 20 ms.
   *
   * Finally, we have enabled main context discounting above, which means that
   * the time spent off the main context should be subtracted from the total
   * time. The sum of all exclusive times is 120 ms, and the total time spent
   * off the main context is 40 ms, so the total time should be 80 ms, with a
   * duration of 50 ms.
   */
  nr_segment_set_timing(root, 0, 50 * NR_TIME_DIVISOR_MS);

  a = nr_segment_start(&txn, root, "1");
  nr_segment_set_name(a, "a");
  nr_segment_set_timing(a, 10 * NR_TIME_DIVISOR_MS, 30 * NR_TIME_DIVISOR_MS);
  b = nr_segment_start(&txn, a, "1");
  nr_segment_set_name(b, "b");
  nr_segment_set_timing(b, 20 * NR_TIME_DIVISOR_MS, 20 * NR_TIME_DIVISOR_MS);
  c = nr_segment_start(&txn, root, "2");
  nr_segment_set_name(c, "c");
  nr_segment_set_timing(c, 10 * NR_TIME_DIVISOR_MS, 30 * NR_TIME_DIVISOR_MS);
  d = nr_segment_start(&txn, c, "2");
  nr_segment_set_name(d, "d");
  nr_segment_set_timing(d, 30 * NR_TIME_DIVISOR_MS, 20 * NR_TIME_DIVISOR_MS);

  nr_segment_end(&a);
  nr_segment_end(&b);
  nr_segment_end(&c);
  nr_segment_end(&d);
  nr_segment_end(&root);

  cb_userdata.txn = &txn;
  cb_userdata.total_time = 80 * NR_TIME_DIVISOR_MS;
  result = nr_segment_tree_finalise(&txn, trace_limit, span_limit,
                                    test_finalise_callback, &cb_userdata);
  tlib_pass_if_size_t_equal(
      "Traversing the segments of a should-sample transaction must invoke the "
      "finalise callback",
      1, cb_userdata.call_count);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-sample transaction must populate a "
      "result",
      result.trace_json);

  tlib_pass_if_str_equal(
      "Traversing the segments of a should-trace transaction must create "
      "expected trace JSON with all "
      "segments",
      result.trace_json,
      // clang-format off
      "["
        "[0,{},{},"
          "[0,50,\"ROOT\",{},["
            "[0,50,\"`0\",{},["
              "[10,40,\"`1\",{\"async_context\":\"`2\"},["
                "[20,40,\"`3\",{\"async_context\":\"`2\"},[]]"
              "]],"
              "[10,40,\"`4\",{\"async_context\":\"`5\"},["
                "[30,50,\"`6\",{\"async_context\":\"`5\"},[]]"
              "]]"
            "]]"
          "]]"
        ",{}]"
        ","
        "[\"WebTransaction\\/*\",\"a\",\"1\",\"b\",\"c\",\"2\",\"d\"]"
      "]"
      // clang-format on
  );

  obj = nro_create_from_json(result.trace_json);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must create valid "
      "JSON",
      obj);

  nro_delete(obj);
  nr_txn_final_destroy_fields(&result);
  nrm_table_destroy(&txn.scoped_metrics);
  nrm_table_destroy(&txn.unscoped_metrics);
  nr_string_pool_destroy(&txn.trace_strings);

  nr_segment_destroy_tree(txn.segment_root);
  nr_slab_destroy(&txn.segment_slab);
}

static void test_finalise_total_time_discounted_sync(void) {
  nrobj_t* obj;
  nr_segment_t *a, *b;
  test_finalise_callback_expected_t cb_userdata = {.call_count = 0};

  nrtxn_t txn = {.abs_start_time = 1000};

  size_t trace_limit = 10;
  size_t span_limit = 0;

  nrtxnfinal_t result;
  nr_segment_t* root;

  txn.segment_slab = nr_slab_create(sizeof(nr_segment_t), 0);
  txn.trace_strings = nr_string_pool_create();
  txn.scoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  txn.unscoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  txn.options.tt_threshold = 0;
  txn.options.discount_main_context_blocking = true;
  txn.status.recording = 1;

  root = nr_slab_next(txn.segment_slab);
  root->txn = &txn;
  root->name = nr_string_add(txn.trace_strings, "WebTransaction/*");

  txn.segment_root = root;

  /*
   * In order to exercise the total and exclusive time calculation, we're going
   * to set up a basic structure of synchronous segments:
   *
   * time (ms): 0    10    20    30    40    50
   *            ROOT------------------------->
   *                 a----------------->
   *                       b----------->
   *
   * On the main context, there is only the ROOT segment, which lasts 50 ms.
   *
   * We have enabled main context discounting above, which means that the time
   * spent off the main context should be subtracted from the total time. Since
   * this transaction is synchronous, there is no time off the main context, so
   * the total time should remain 50 ms.
   */
  nr_segment_set_timing(root, 0, 50 * NR_TIME_DIVISOR_MS);

  a = nr_segment_start(&txn, root, NULL);
  nr_segment_set_name(a, "a");
  nr_segment_set_timing(a, 10 * NR_TIME_DIVISOR_MS, 30 * NR_TIME_DIVISOR_MS);
  b = nr_segment_start(&txn, a, NULL);
  nr_segment_set_name(b, "b");
  nr_segment_set_timing(b, 20 * NR_TIME_DIVISOR_MS, 20 * NR_TIME_DIVISOR_MS);

  nr_segment_end(&a);
  nr_segment_end(&b);
  nr_segment_end(&root);

  cb_userdata.txn = &txn;
  cb_userdata.total_time = 50 * NR_TIME_DIVISOR_MS;
  result = nr_segment_tree_finalise(&txn, trace_limit, span_limit,
                                    test_finalise_callback, &cb_userdata);
  tlib_pass_if_size_t_equal(
      "Traversing the segments of a should-sample transaction must invoke the "
      "finalise callback",
      1, cb_userdata.call_count);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-sample transaction must populate a "
      "result",
      result.trace_json);

  tlib_pass_if_str_equal(
      "Traversing the segments of a should-trace transaction must create "
      "expected trace JSON with all "
      "segments",
      result.trace_json,
      // clang-format off
      "["
        "[0,{},{},"
          "[0,50,\"ROOT\",{},["
            "[0,50,\"`0\",{},["
              "[10,40,\"`1\",{},["
                "[20,40,\"`2\",{},[]]"
              "]]"
            "]]"
          "]]"
        ",{}]"
        ","
        "[\"WebTransaction\\/*\",\"a\",\"b\"]"
      "]"
      // clang-format on
  );

  obj = nro_create_from_json(result.trace_json);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace transaction must create valid "
      "JSON",
      obj);

  nro_delete(obj);
  nr_txn_final_destroy_fields(&result);
  nrm_table_destroy(&txn.scoped_metrics);
  nrm_table_destroy(&txn.unscoped_metrics);
  nr_string_pool_destroy(&txn.trace_strings);

  nr_segment_destroy_tree(txn.segment_root);
  nr_slab_destroy(&txn.segment_slab);
}

static void test_finalise_with_sampling(void) {
  int i;
  nrobj_t* obj;
  nr_segment_t* current;

  nrtxn_t txn = {.abs_start_time = 1000};

  nrtime_t start_time = 0;
  nrtime_t stop_time = 9000;

  size_t trace_limit = NR_TEST_SEGMENT_TREE_SIZE;
  size_t span_limit = 0;
  char* segment_names[NR_TEST_SEGMENT_TREE_SIZE];

  nrtxnfinal_t result;
  nr_segment_t* root;

  txn.segment_slab = nr_slab_create(sizeof(nr_segment_t), 0);
  txn.trace_strings = nr_string_pool_create();
  txn.scoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  txn.unscoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);

  root = nr_slab_next(txn.segment_slab);
  root->txn = &txn;
  root->start_time = start_time;
  root->stop_time = stop_time;
  root->name = nr_string_add(txn.trace_strings, "WebTransaction/*");

  txn.segment_root = root;
  current = root;

  for (i = 0; i < NR_TEST_SEGMENT_TREE_SIZE; i++) {
    nr_segment_t* segment = nr_slab_next(txn.segment_slab);

    segment_names[i] = nr_alloca(5 * sizeof(char));
    nr_itoa(segment_names[i], 5, i);

    segment->start_time = start_time + ((i + 1) * 1000);
    segment->stop_time = stop_time - ((i + 1) * 1000);
    segment->name = nr_string_add(txn.trace_strings, segment_names[i]);
    segment->txn = &txn;

    nr_segment_add_metric(segment, segment_names[i], false);
    nr_segment_add_metric(segment, segment_names[i], true);

    nr_segment_children_init(&current->children);
    nr_segment_add_child(current, segment);

    current = segment;
  }

  txn.segment_count = NR_TEST_SEGMENT_TREE_SIZE;

  /*
   * Test : Normal operation with sampling
   */
  trace_limit = 2;
  result = nr_segment_tree_finalise(&txn, trace_limit, span_limit, NULL, NULL);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace, should-sample transaction "
      "must populate a result",
      result.trace_json);

  tlib_pass_if_str_equal(
      "Traversing the segments of a should-trace, should-sample transaction "
      "must create expected trace JSON with two segments only",
      result.trace_json,
      "[[0,{},{},[0,9,\"ROOT\",{},[[0,9,\"`0\",{},[[1,8,\"`1\",{},[]]]]]],{}]"
      ",["
      "\"WebTransaction\\/*\",\"0\"]]");

  obj = nro_create_from_json(result.trace_json);
  tlib_pass_if_not_null(
      "Traversing the segments of a should-trace, should-sample transaction "
      "must create valid JSON",
      obj);

  for (i = 0; i < NR_TEST_SEGMENT_TREE_SIZE; i++) {
    nrtime_t expected_duration = nr_time_duration(start_time + ((i + 1) * 1000),
                                                  stop_time - ((i + 1) * 1000));

    test_metric_created_ex(
        "Traversing the segments of a should-trace, should-sample transaction "
        "must create unscoped metrics as needed",
        txn.unscoped_metrics, 0, expected_duration, i == 3 ? 1000 : 2000,
        segment_names[i]);

    test_metric_created_ex(
        "Traversing the segments of a should-trace, should-sample transaction "
        "must create scoped metrics as needed",
        txn.scoped_metrics, 0, expected_duration, i == 3 ? 1000 : 2000,
        segment_names[i]);
  }

  nro_delete(obj);
  nr_txn_final_destroy_fields(&result);
  nrm_table_destroy(&txn.scoped_metrics);
  nrm_table_destroy(&txn.unscoped_metrics);
  nr_string_pool_destroy(&txn.trace_strings);

  nr_segment_destroy_tree(txn.segment_root);
  nr_slab_destroy(&txn.segment_slab);
}

#define NR_TEST_SEGMENT_EXTENDED_TREE_SIZE 3000
static void test_finalise_with_extended_sampling(void) {
  int i;
  nrtxn_t txn = {.abs_start_time = 1000};
  size_t trace_limit = 4;
  size_t span_limit = 0;
  nrtxnfinal_t result;
  nrobj_t* obj;
  nr_segment_t* current;
  char* segment_names[NR_TEST_SEGMENT_EXTENDED_TREE_SIZE];

  nr_segment_t* root;

  txn.segment_slab = nr_slab_create(sizeof(nr_segment_t), 0);
  txn.trace_strings = nr_string_pool_create();
  txn.scoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  txn.unscoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);

  root = nr_slab_next(txn.segment_slab);
  root->txn = &txn;
  root->start_time = 0;
  root->stop_time = 34000;
  root->name = nr_string_add(txn.trace_strings, "WebTransaction/*");

  txn.segment_root = root;
  current = root;

  for (i = 0; i < NR_TEST_SEGMENT_EXTENDED_TREE_SIZE; i++) {
    nr_segment_t* segment = nr_slab_next(txn.segment_slab);

    segment_names[i] = nr_alloca(5 * sizeof(char));
    nr_itoa(segment_names[i], 5, i);

    segment->start_time = i;
    segment->stop_time = i * 10 + 1;
    segment->name = nr_string_add(txn.trace_strings, segment_names[i]);
    segment->txn = &txn;

    nr_segment_add_metric(segment, segment_names[i], false);
    nr_segment_add_metric(segment, segment_names[i], true);

    nr_segment_children_init(&current->children);
    nr_segment_add_child(current, segment);

    current = segment;
  }

  txn.segment_count = NR_TEST_SEGMENT_EXTENDED_TREE_SIZE;

  result = nr_segment_tree_finalise(&txn, trace_limit, span_limit, NULL, NULL);
  tlib_pass_if_not_null(
      "Traversing the segments of a very large should-trace, should-sample "
      "transaction must populate a result",
      result.trace_json);

  tlib_pass_if_str_equal(
      "Traversing the segments of a very large should-trace, should-sample "
      "transaction must create expected trace JSON with the four longest "
      "segments",
      result.trace_json,
      "[[0,{},{},[0,34,\"ROOT\",{},[[0,34,\"`0\",{},[[2,29,\"`1\",{},[[2,29,"
      "\"`"
      "2\",{},[[2,29,\"`3\",{},[]]]]]]]]]],{}],[\"WebTransaction\\/"
      "*\",\"2997\",\"2998\",\"2999\"]]");

  obj = nro_create_from_json(result.trace_json);
  tlib_pass_if_not_null(
      "Traversing the segments of a very large should-trace, should-sample "
      "transaction must create valid JSON",
      obj);

  tlib_pass_if_int_equal(
      "Traversing the segments of a very large should-trace, should-sample "
      "transaction must create unscoped metrics as needed, but subject to "
      "the "
      "overall metric limit",
      NR_METRIC_DEFAULT_LIMIT + 1, nrm_table_size(txn.unscoped_metrics));

  tlib_pass_if_int_equal(
      "Traversing the segments of a very large should-trace, should-sample "
      "transaction must create scoped metrics as needed, but subject to the "
      "overall metric limit",
      NR_METRIC_DEFAULT_LIMIT + 1, nrm_table_size(txn.scoped_metrics));

  nro_delete(obj);
  nr_txn_final_destroy_fields(&result);
  nrm_table_destroy(&txn.scoped_metrics);
  nrm_table_destroy(&txn.unscoped_metrics);
  nr_string_pool_destroy(&txn.trace_strings);

  nr_segment_destroy_tree(txn.segment_root);
  nr_slab_destroy(&txn.segment_slab);
}

static void test_nearest_sampled_ancestor(void) {
  nr_set_t* set;
  nr_segment_t* ancestor = NULL;
  nrtxn_t txn = {0};

  nr_segment_t root = {.name = 0, .txn = &txn};
  nr_segment_t A = {.name = 1, .txn = &txn};
  nr_segment_t B = {.name = 2, .txn = &txn};
  nr_segment_t child = {.name = 3, .txn = &txn};
  txn.segment_root = &root;

  /*
   *         ----------Root-----------
   *                  /
   *            -----A-----
   *                /
   *           ----B----
   *              /
   *          -child-
   */

  set = nr_set_create();
  nr_set_insert(set, (void*)&child);

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&A.children);
  nr_segment_children_init(&B.children);

  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&A, &B);
  nr_segment_add_child(&B, &child);

  /*
   * Test : Bad parameters
   */
  ancestor = nr_segment_tree_get_nearest_sampled_ancestor(NULL, &child);
  tlib_pass_if_null("Passing in a NULL set returns NULL", ancestor);

  ancestor = nr_segment_tree_get_nearest_sampled_ancestor(set, NULL);
  tlib_pass_if_null("Passing in a NULL segment returns NULL", ancestor);

  /*
   * Test : There is no sampled ancestor
   */
  ancestor = nr_segment_tree_get_nearest_sampled_ancestor(set, &child);
  tlib_pass_if_null(
      "Passing in a set without any sampled ancestors returns NULL", ancestor);
  /*
   * Test : There is a sampled ancestor
   */
  nr_segment_add_child(&root, &A);
  nr_set_insert(set, (void*)&root);
  ancestor = nr_segment_tree_get_nearest_sampled_ancestor(set, &child);
  tlib_pass_if_ptr_equal("The returned ancestor should be the root", &root,
                         ancestor);

  nr_set_destroy(&set);
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_deinit(&A.children);
  nr_segment_children_deinit(&B.children);

  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);
  nr_segment_destroy_fields(&child);
}

static void test_nearest_sampled_ancestor_cycle(void) {
  nr_set_t* set;
  nr_segment_t* ancestor = NULL;
  nrtxn_t txn = {0};

  nr_segment_t root = {.name = 0, .txn = &txn};
  nr_segment_t A = {.name = 1, .txn = &txn};
  nr_segment_t B = {.name = 2, .txn = &txn};
  nr_segment_t child = {.name = 3, .txn = &txn};

  txn.segment_root = &root;

  set = nr_set_create();
  nr_set_insert(set, (void*)&child);

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&A.children);
  nr_segment_children_init(&B.children);

  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&A, &B);
  nr_segment_add_child(&B, &child);

  /*
   * Test : There is a cycle in the tree that does not include the target
   *        segment. The target segment is the only one sampled.
   */

  /*
   *         ----------Root-----------
   *                  /      |
   *            -----A-----  |
   *                /        |
   *           ----B----     |
   *              /    |     |
   *          -child-  |     |
   *                   +-->--+
   */
  nr_segment_add_child(&B, &root);
  ancestor = nr_segment_tree_get_nearest_sampled_ancestor(set, &child);
  tlib_pass_if_null(
      "Passing in a tree with a cycle and no sampled ancestors returns NULL",
      ancestor);

  // Test : There is a cycle but the segment has a sampled parent.
  nr_set_insert(set, (void*)&A);
  ancestor = nr_segment_tree_get_nearest_sampled_ancestor(set, &child);
  tlib_pass_if_ptr_equal("The returned ancestor should be A", &A, ancestor);

  nr_set_destroy(&set);
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_deinit(&A.children);
  nr_segment_children_deinit(&B.children);

  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);
  nr_segment_destroy_fields(&child);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 4, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_finalise_bad_params();
  test_finalise_one_only_with_metrics();
  test_finalise();
  test_finalise_total_time();
  test_finalise_total_time_discounted_async();
  test_finalise_total_time_discounted_sync();
  test_finalise_with_sampling();
  test_finalise_with_extended_sampling();
  test_finalise_span_priority();
  test_nearest_sampled_ancestor();
  test_nearest_sampled_ancestor_cycle();
}
