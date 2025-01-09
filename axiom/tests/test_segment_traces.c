/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>

#include "nr_limits.h"
#include "nr_segment.h"
#include "nr_segment_private.h"
#include "nr_segment_traces.h"
#include "nr_span_event_private.h"
#include "util_memory.h"
#include "util_minmax_heap.h"
#include "util_set.h"

#include "tlib_main.h"

#define test_buffer_contents(...) \
  test_buffer_contents_fn(__VA_ARGS__, __FILE__, __LINE__)

#define SPAN_EVENT_COMPARE(evt, expect_name, expect_category, expect_parent, \
                           expect_start, expect_duration)                    \
  tlib_pass_if_not_null(expect_name, evt);                                   \
  tlib_pass_if_str_equal("name", nr_span_event_get_name(evt), expect_name);  \
  tlib_pass_if_time_equal("start", expect_start / NR_TIME_DIVISOR_MS,        \
                          nr_span_event_get_timestamp(evt));                 \
  tlib_pass_if_double_equal("duration", expect_duration / NR_TIME_DIVISOR_D, \
                            nr_span_event_get_duration(evt));                \
                                                                             \
  if (expect_parent) {                                                       \
    tlib_pass_if_str_equal("parent", nr_span_event_get_guid(expect_parent),  \
                           nr_span_event_get_parent_id(evt));                \
  } else {                                                                   \
    tlib_pass_if_null("parent", nr_span_event_get_parent_id(evt));           \
  }                                                                          \
                                                                             \
  switch (expect_category) {                                                 \
    case NR_SPAN_GENERIC:                                                    \
      tlib_pass_if_str_equal("category", "generic",                          \
                             nr_span_event_get_category(evt));               \
      break;                                                                 \
    case NR_SPAN_DATASTORE:                                                  \
      tlib_pass_if_str_equal("category", "datastore",                        \
                             nr_span_event_get_category(evt));               \
      break;                                                                 \
    case NR_SPAN_HTTP:                                                       \
      tlib_pass_if_str_equal("category", "http",                             \
                             nr_span_event_get_category(evt));               \
      break;                                                                 \
    case NR_SPAN_MESSAGE:                                                    \
      tlib_pass_if_str_equal("category", "message",                          \
                             nr_span_event_get_category(evt));               \
      break;                                                                 \
    default:                                                                 \
      tlib_pass_if_true("invalid category", false, "category=%s",            \
                        nr_span_event_get_category(evt));                    \
  }

#define SPAN_EVENT_COMPARE_DATASTORE(span_event, expected_host,                \
                                     expected_db_name, expected_statement,     \
                                     expected_address)                         \
  tlib_pass_if_str_equal("host", expected_host,                                \
                         nr_span_event_get_datastore(                          \
                             span_event, NR_SPAN_DATASTORE_PEER_HOSTNAME));    \
  tlib_pass_if_str_equal("address", expected_address,                          \
                         nr_span_event_get_datastore(                          \
                             span_event, NR_SPAN_DATASTORE_PEER_ADDRESS));     \
  tlib_pass_if_str_equal(                                                      \
      "database name", expected_db_name,                                       \
      nr_span_event_get_datastore(span_event, NR_SPAN_DATASTORE_DB_INSTANCE)); \
  tlib_pass_if_str_equal("Statement", expected_statement,                      \
                         nr_span_event_get_datastore(                          \
                             span_event, NR_SPAN_DATASTORE_DB_STATEMENT));

#define SPAN_EVENT_COMPARE_EXTERNAL(span_event, expected_url, expected_method, \
                                    expected_component, expected_status)       \
  tlib_pass_if_str_equal(                                                      \
      "url", expected_url,                                                     \
      nr_span_event_get_external(span_event, NR_SPAN_EXTERNAL_URL));           \
  tlib_pass_if_str_equal(                                                      \
      "method", expected_method,                                               \
      nr_span_event_get_external(span_event, NR_SPAN_EXTERNAL_METHOD));        \
  tlib_pass_if_str_equal(                                                      \
      "component", expected_component,                                         \
      nr_span_event_get_external(span_event, NR_SPAN_EXTERNAL_COMPONENT));     \
  tlib_pass_if_int_equal("status", expected_status,                            \
                         nr_span_event_get_external_status(span_event));

#define SPAN_EVENT_COMPARE_MESSAGE(span_event, expected_destination_name,    \
                                   expected_messaging_system,                \
                                   expected_server_address)                  \
  tlib_pass_if_str_equal("messaging.destination.name",                       \
                         expected_destination_name,                          \
                         nr_span_event_get_message(                          \
                             span_event, NR_SPAN_MESSAGE_DESTINATION_NAME)); \
  tlib_pass_if_str_equal("messaging.system", expected_messaging_system,      \
                         nr_span_event_get_message(                          \
                             span_event, NR_SPAN_MESSAGE_MESSAGING_SYSTEM)); \
  tlib_pass_if_str_equal(                                                    \
      "server.address", expected_server_address,                             \
      nr_span_event_get_message(span_event, NR_SPAN_MESSAGE_SERVER_ADDRESS));

static void nr_vector_span_event_dtor(void* element, void* userdata NRUNUSED) {
  nr_span_event_destroy((nr_span_event_t**)&element);
}

static void mock_txn(nrtxn_t* txn, nr_segment_t* root) {
  txn->segment_root = root;
  txn->trace_strings = nr_string_pool_create();

  // This is only required until we have Trace Observer configuration plumbed
  // through in a way that nr_txn_should_create_span_events() can be updated.
  txn->distributed_trace = nr_distributed_trace_create();
  nr_distributed_trace_set_sampled(txn->distributed_trace, true);
  txn->options.distributed_tracing_enabled = true;
  txn->options.span_events_enabled = true;
}

static void cleanup_mock_txn(nrtxn_t* txn) {
  nr_distributed_trace_destroy(&txn->distributed_trace);
  nr_string_pool_destroy(&txn->trace_strings);
}

static void test_buffer_contents_fn(const char* testname,
                                    nrbuf_t* buf,
                                    const char* expected,
                                    const char* file,
                                    int line) {
  const char* cs;
  nrobj_t* obj;

  nr_buffer_add(buf, "", 1);
  cs = (const char*)nr_buffer_cptr(buf);

  test_pass_if_true(testname, 0 == nr_strcmp(cs, expected), "cs=%s expected=%s",
                    NRSAFESTR(cs), NRSAFESTR(expected));

  if (nr_strcmp(cs, expected)) {
    printf("got:      %s\n", NRSAFESTR(cs));
    printf("expected: %s\n", NRSAFESTR(expected));
  }

  obj = nro_create_from_json(cs);
  test_pass_if_true(testname, 0 != obj, "obj=%p", obj);
  nro_delete(obj);

  nr_buffer_reset(buf);
}

static void test_json_print_bad_parameters(void) {
  bool rv;
  nrbuf_t* buf;
  nrpool_t* segment_names;

  nrtxn_t txn = {.abs_start_time = 1000};
  nr_segment_t root = {.type = NR_SEGMENT_CUSTOM,
                       .txn = &txn,
                       .start_time = 0,
                       .stop_time = 9000};
  buf = nr_buffer_create(4096, 4096);
  segment_names = nr_string_pool_create();

  /*
   * Test : Bad parameters
   */
  rv = nr_segment_traces_json_print_segments(NULL, NULL, NULL, NULL, NULL, NULL,
                                             NULL);
  tlib_pass_if_bool_equal(
      "Return value must be false when input params are NULL", false, rv);

  rv = nr_segment_traces_json_print_segments(NULL, NULL, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal("Return value must be false when input buff is NULL",
                          false, rv);

  rv = nr_segment_traces_json_print_segments(buf, NULL, NULL, NULL, NULL, &root,
                                             segment_names);
  tlib_pass_if_bool_equal("Return value must be false when input txn is NULL",
                          false, rv);

  rv = nr_segment_traces_json_print_segments(buf, NULL, NULL, NULL, &txn, &root,
                                             NULL);
  tlib_pass_if_bool_equal("Return value must be -1 when input pool is NULL",
                          false, rv);

  /* Clean up */
  nr_string_pool_destroy(&segment_names);
  nr_buffer_destroy(&buf);
}

static void test_json_print_segments_root_only(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;
  nr_span_event_t* evt_root;

  nrtxn_t txn = {.abs_start_time = 1000};
  nr_segment_t root = {.type = NR_SEGMENT_CUSTOM,
                       .txn = &txn,
                       .start_time = 0,
                       .stop_time = 9000};

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(9, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);

  /* Create a single mock segment */
  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal(
      "Printing JSON for a single root segment must succeed", true, rv);
  test_buffer_contents("success", buf, "[0,9,\"`0\",{},[]]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 1);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);

  /* Clean up */
  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_segment_destroy_fields(&root);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_bad_segments(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_child;

  nrtxn_t txn = {.abs_start_time = 1000};
  nr_segment_t root = {.type = NR_SEGMENT_CUSTOM,
                       .txn = &txn,
                       .start_time = 0,
                       .stop_time = 9000};
  nr_segment_t child = {.type = NR_SEGMENT_CUSTOM,
                        .txn = &txn,
                        .start_time = 1000,
                        .stop_time = 1000};

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(9, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);

  /* Create a collection of mock segments */

  /*    ------root-------
   *       --child--
   *
   */

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&child.children);

  nr_segment_add_child(&root, &child);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  child.name = nr_string_add(txn.trace_strings, "Mongo/alpha");

  /*
   * Test : Segment stop before segment start
   */
  child.start_time = 4000;
  child.stop_time = 2000;
  rv = nr_segment_traces_json_print_segments(buf, span_events, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal(
      "Printing JSON for a segment that has out of order start and stop must "
      "fail",
      false, rv);

  tlib_pass_if_uint_equal("not all span events created",
                          nr_vector_size(span_events), 1);

  nr_buffer_reset(buf);
  nr_vector_destroy(&span_events);
  span_events = nr_vector_create(9, nr_vector_span_event_dtor, NULL);

  /*
   * Test : Segment with unknown name
   */
  child.start_time = 1000;
  child.stop_time = 3000;
  child.name = 0;
  rv = nr_segment_traces_json_print_segments(buf, span_events, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal(
      "Printing JSON for a segment with an unknown name must succeed", true,
      rv);
  test_buffer_contents("unknown name", buf,
                       "[0,9,\"`0\",{},[[1,3,\"`1\",{},[]]]]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 2);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_child = (nr_span_event_t*)nr_vector_get(span_events, 1);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  SPAN_EVENT_COMPARE(evt_child, "<unknown>", NR_SPAN_GENERIC, evt_root, 2000,
                     2000);

  /* Clean up */
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_deinit(&child.children);
  nr_segment_destroy_fields(&child);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segment_with_data(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;
  nrobj_t* value;

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_child;

  nrtxn_t txn = {.abs_start_time = 1000};
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t child = {.txn = &txn, .start_time = 1000, .stop_time = 3000};

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(9, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.segment_count = 2;

  /* Create a collection of mock segments */

  /*    ------root-------
   *       --child--
   *
   */

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&child.children);

  nr_segment_add_child(&root, &child);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  child.name = nr_string_add(txn.trace_strings, "External/domain.com/all");
  child.attributes = nr_attributes_create(NULL);
  value = nro_new_string("domain.com");
  nr_segment_attributes_user_add(&child, NR_ATTRIBUTE_DESTINATION_TXN_TRACE,
                                 "uri", value);
  nro_delete(value);

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal("Printing JSON for a segment with data must succeed",
                          true, rv);
  test_buffer_contents("node with data", buf,
                       "[0,9,\"`0\",{},"
                       "[[1,3,\"`1\",{\"uri\":\"domain.com\"},[]]]]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 2);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_child = (nr_span_event_t*)nr_vector_get(span_events, 1);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  SPAN_EVENT_COMPARE(evt_child, "External/domain.com/all", NR_SPAN_GENERIC,
                     evt_root, 2000, 2000);

  /* Clean up */
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_deinit(&child.children);
  nr_segment_destroy_fields(&child);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_two_nodes(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_child;

  nrtxn_t txn = {.abs_start_time = 1000};
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t child = {.txn = &txn, .start_time = 1000, .stop_time = 3000};

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(9, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.segment_count = 2;

  /* Create a collection of mock segments */

  /*    ------root-------
   *       --child--
   *
   */

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&child.children);

  nr_segment_add_child(&root, &child);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  child.name = nr_string_add(txn.trace_strings, "Mongo/alpha");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal("Printing JSON for a root+child pair must succeed",
                          true, rv);
  test_buffer_contents("success", buf, "[0,9,\"`0\",{},[[1,3,\"`1\",{},[]]]]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 2);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_child = (nr_span_event_t*)nr_vector_get(span_events, 1);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  SPAN_EVENT_COMPARE(evt_child, "Mongo/alpha", NR_SPAN_GENERIC, evt_root, 2000,
                     2000);

  /* Clean up */
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_deinit(&child.children);
  nr_segment_destroy_fields(&child);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_hanoi(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;

  nrtxn_t txn = {.abs_start_time = 1000};

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_a;
  nr_span_event_t* evt_b;
  nr_span_event_t* evt_c;

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t A = {.txn = &txn, .start_time = 1000, .stop_time = 6000};
  nr_segment_t B = {.txn = &txn, .start_time = 2000, .stop_time = 5000};
  nr_segment_t C = {.txn = &txn, .start_time = 3000, .stop_time = 4000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(9, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  txn.segment_count = 4;
  mock_txn(&txn, &root);

  /* Create a collection of mock segments */

  /*    ------root-------
   *       ----A----
   *       ----B----
   *         --C--
   */

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&A.children);
  nr_segment_children_init(&B.children);

  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&A, &B);
  nr_segment_add_child(&B, &C);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");
  B.name = nr_string_add(txn.trace_strings, "B");
  C.name = nr_string_add(txn.trace_strings, "C");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal(
      "Printing JSON for a cascade of four segments must succeed", true, rv);
  test_buffer_contents("towers of hanoi", buf,
                       "[0,9,\"`0\",{},[[1,6,\"`1\",{},[[2,5,\"`2\",{},[[3,4,"
                       "\"`3\",{},[]]]]]]]]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 4);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_a = (nr_span_event_t*)nr_vector_get(span_events, 1);
  evt_b = (nr_span_event_t*)nr_vector_get(span_events, 2);
  evt_c = (nr_span_event_t*)nr_vector_get(span_events, 3);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  SPAN_EVENT_COMPARE(evt_a, "A", NR_SPAN_GENERIC, evt_root, 2000, 5000);
  SPAN_EVENT_COMPARE(evt_b, "B", NR_SPAN_GENERIC, evt_a, 3000, 3000);
  SPAN_EVENT_COMPARE(evt_c, "C", NR_SPAN_GENERIC, evt_b, 4000, 1000);

  /* Clean up */
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_deinit(&A.children);
  nr_segment_children_deinit(&B.children);

  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);
  nr_segment_destroy_fields(&C);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_three_siblings(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;

  nrtxn_t txn = {.abs_start_time = 1000};

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_a;
  nr_span_event_t* evt_b;
  nr_span_event_t* evt_c;

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t A = {.txn = &txn, .start_time = 1000, .stop_time = 2000};
  nr_segment_t B = {.txn = &txn, .start_time = 3000, .stop_time = 4000};
  nr_segment_t C = {.txn = &txn, .start_time = 5000, .stop_time = 6000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(9, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.segment_count = 4;

  /* Create a collection of mock segments */

  /*
   *      -- root --
   *  --A--  --B--  --C--
   */

  nr_segment_children_init(&root.children);

  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&root, &B);
  nr_segment_add_child(&root, &C);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");
  B.name = nr_string_add(txn.trace_strings, "B");
  C.name = nr_string_add(txn.trace_strings, "C");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal(
      "Printing JSON for a rooted set of triplets must succeed", true, rv);
  test_buffer_contents("sequential nodes", buf,
                       "[0,9,\"`0\",{},[[1,2,\"`1\",{},[]],[3,4,\"`2\",{},[]],["
                       "5,6,\"`3\",{},[]]]]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 4);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_a = (nr_span_event_t*)nr_vector_get(span_events, 1);
  evt_b = (nr_span_event_t*)nr_vector_get(span_events, 2);
  evt_c = (nr_span_event_t*)nr_vector_get(span_events, 3);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  SPAN_EVENT_COMPARE(evt_a, "A", NR_SPAN_GENERIC, evt_root, 2000, 1000);
  SPAN_EVENT_COMPARE(evt_b, "B", NR_SPAN_GENERIC, evt_root, 4000, 1000);
  SPAN_EVENT_COMPARE(evt_c, "C", NR_SPAN_GENERIC, evt_root, 6000, 1000);

  /* Clean up */
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);
  nr_segment_destroy_fields(&C);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_invalid_typed_attributes(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_a;
  nr_span_event_t* evt_b;
  nr_span_event_t* evt_c;

  nrtxn_t txn = {0};

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 11000};
  nr_segment_t A = {.txn = &txn, .start_time = 1000, .stop_time = 6000};
  nr_segment_t B = {.txn = &txn, .start_time = 6000, .stop_time = 8000};
  nr_segment_t C = {.txn = &txn, .start_time = 9000, .stop_time = 10000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(9, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.abs_start_time = 1000;
  txn.segment_count = 3;

  /* Create a collection of mock segments */
  nr_segment_children_init(&root.children);

  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&root, &B);
  nr_segment_add_child(&root, &C);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");
  B.name = nr_string_add(txn.trace_strings, "B");
  C.name = nr_string_add(txn.trace_strings, "C");

  A.type = NR_SEGMENT_EXTERNAL;
  B.type = NR_SEGMENT_DATASTORE;
  C.type = NR_SEGMENT_MESSAGE;

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal("success", true, rv);
  test_buffer_contents("segment attributes", buf,
                       "[0,11,\"`0\",{},"
                       "[[1,6,\"`1\",{},[]],"
                       "[6,8,\"`2\",{},[]],"
                       "[9,10,\"`3\",{},[]]]]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 4);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_a = (nr_span_event_t*)nr_vector_get(span_events, 1);
  evt_b = (nr_span_event_t*)nr_vector_get(span_events, 2);
  evt_c = (nr_span_event_t*)nr_vector_get(span_events, 3);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     11000);
  SPAN_EVENT_COMPARE(evt_a, "A", NR_SPAN_HTTP, evt_root, 2000, 5000);
  SPAN_EVENT_COMPARE_EXTERNAL(evt_a, NULL, NULL, NULL, 0);
  SPAN_EVENT_COMPARE(evt_b, "B", NR_SPAN_DATASTORE, evt_root, 7000, 2000);
  SPAN_EVENT_COMPARE_DATASTORE(evt_b, NULL, NULL, NULL, NULL);
  SPAN_EVENT_COMPARE(evt_c, "C", NR_SPAN_MESSAGE, evt_root, 10000, 1000);
  SPAN_EVENT_COMPARE_MESSAGE(evt_c, NULL, NULL, NULL);

  /* Clean up */
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);
  nr_segment_destroy_fields(&C);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_datastore_params(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;

  nrtxn_t txn = {0};

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_a;

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t A = {.txn = &txn, .start_time = 1000, .stop_time = 6000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(9, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.abs_start_time = 1000;
  txn.segment_count = 2;

  /* Create a collection of mock segments */

  /*    ------root-------
   *     ------A------
   */

  nr_segment_children_init(&root.children);

  nr_segment_add_child(&root, &A);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");

  A.type = NR_SEGMENT_DATASTORE;
  A.attributes = NULL;
  A.typed_attributes = nr_zalloc(sizeof(nr_segment_typed_attributes_t));
  A.typed_attributes->datastore.sql_obfuscated = nr_strdup("SELECT");
  A.typed_attributes->datastore.instance.host = nr_strdup("localhost");
  A.typed_attributes->datastore.instance.database_name = nr_strdup("db");
  A.typed_attributes->datastore.instance.port_path_or_id = nr_strdup("3308");
  A.typed_attributes->datastore.backtrace_json = nr_strdup("[\"a\",\"b\"]");
  A.typed_attributes->datastore.explain_plan_json = nr_strdup("[\"c\",\"d\"]");
  A.typed_attributes->datastore.input_query_json = nr_strdup("[\"e\",\"f\"]");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal("success", true, rv);
  test_buffer_contents("datastore params", buf,
                       "[0,9,\"`0\",{},[[1,6,\"`1\",{"
                       "\"host\":\"localhost\","
                       "\"database_name\":\"db\","
                       "\"port_path_or_id\":\"3308\","
                       "\"backtrace\":[\"a\",\"b\"],"
                       "\"explain_plan\":[\"c\",\"d\"],"
                       "\"sql_obfuscated\":\"SELECT\","
                       "\"input_query\":[\"e\",\"f\"]"
                       "},[]]]]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 2);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_a = (nr_span_event_t*)nr_vector_get(span_events, 1);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  SPAN_EVENT_COMPARE(evt_a, "A", NR_SPAN_DATASTORE, evt_root, 2000, 5000);
  SPAN_EVENT_COMPARE_DATASTORE(evt_a, "localhost", "db", "SELECT",
                               "localhost:3308");

  /* Clean up */
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_destroy_fields(&A);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_external_async_user_attrs(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;
  nrobj_t* value;

  nrtxn_t txn = {0};

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_a;

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t A = {.txn = &txn, .start_time = 1000, .stop_time = 6000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(9, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.abs_start_time = 1000;
  txn.segment_count = 2;

  /* Create a collection of mock segments */

  /*    ------root-------
   *     ------A------
   */

  nr_segment_children_init(&root.children);

  nr_segment_add_child(&root, &A);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");

  A.type = NR_SEGMENT_EXTERNAL;
  A.attributes = nr_attributes_create(NULL);
  value = nro_new_string("bar");
  nr_segment_attributes_user_add(&A, NR_ATTRIBUTE_DESTINATION_TXN_TRACE, "foo",
                                 value);
  nro_delete(value);
  A.async_context = nr_string_add(txn.trace_strings, "async");
  A.typed_attributes = nr_zalloc(sizeof(nr_segment_typed_attributes_t));
  A.typed_attributes->external.uri = nr_strdup("example.com");
  A.typed_attributes->external.library = nr_strdup("curl");
  A.typed_attributes->external.procedure = nr_strdup("GET");
  A.typed_attributes->external.transaction_guid = nr_strdup("guid");
  A.typed_attributes->external.status = 200;

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal("success", true, rv);
  test_buffer_contents("datastore params", buf,
                       "[0,9,\"`0\",{},[[1,6,\"`1\",{"
                       "\"uri\":\"example.com\","
                       "\"library\":\"curl\","
                       "\"procedure\":\"GET\","
                       "\"transaction_guid\":\"guid\","
                       "\"status\":200,"
                       "\"async_context\":\"`2\","
                       "\"foo\":\"bar\""
                       "},[]]]]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 2);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_a = (nr_span_event_t*)nr_vector_get(span_events, 1);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  SPAN_EVENT_COMPARE(evt_a, "A", NR_SPAN_HTTP, evt_root, 2000, 5000);
  SPAN_EVENT_COMPARE_EXTERNAL(evt_a, "example.com", "GET", "curl", 200);

  /* Clean up */
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_destroy_fields(&A);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_message_attributes(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;

  nrtxn_t txn = {0};

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_a;

  // clang-format off
    nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
    nr_segment_t A = {.txn = &txn, .start_time = 1000, .stop_time = 6000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(9, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.abs_start_time = 1000;
  txn.segment_count = 2;

  /* Create a collection of mock segments */

  /*    ------root-------
   *     ------A------
   */

  nr_segment_children_init(&root.children);

  nr_segment_add_child(&root, &A);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");

  A.type = NR_SEGMENT_MESSAGE;
  A.attributes = NULL;
  A.typed_attributes = nr_zalloc(sizeof(nr_segment_typed_attributes_t));
  A.typed_attributes->message.destination_name = nr_strdup("queue_name");
  A.typed_attributes->message.messaging_system = nr_strdup("aws_sqs");
  A.typed_attributes->message.server_address = nr_strdup("localhost");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal("success", true, rv);
  test_buffer_contents("message attributes", buf,
                       "[0,9,\"`0\",{},[[1,6,\"`1\",{"
                       "\"destination_name\":\"queue_name\","
                       "\"messaging_system\":\"aws_sqs\","
                       "\"server_address\":\"localhost\""
                       "},[]]]]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 2);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_a = (nr_span_event_t*)nr_vector_get(span_events, 1);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  SPAN_EVENT_COMPARE(evt_a, "A", NR_SPAN_MESSAGE, evt_root, 2000, 5000);
  SPAN_EVENT_COMPARE_MESSAGE(evt_a, "queue_name", "aws_sqs", "localhost");

  /* Clean up */
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_destroy_fields(&A);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_datastore_external_message(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;

  nrtxn_t txn = {.abs_start_time = 1000};

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_a;
  nr_span_event_t* evt_b;
  nr_span_event_t* evt_c;
  nr_span_event_t* evt_d;

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t A = {.txn = &txn, .start_time = 1000, .stop_time = 6000};
  nr_segment_t B = {.txn = &txn, .start_time = 2000, .stop_time = 3000};
  nr_segment_t C = {.txn = &txn, .start_time = 4000, .stop_time = 5000};
  nr_segment_t D = {.txn = &txn, .start_time = 5000, .stop_time = 6000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(9, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.segment_count = 4;

  /* Create a collection of mock segments */

  /*    ------root-------
   *     ------A------
   *    --B-- --C-- --D--
   */

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&A.children);

  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&A, &B);
  nr_segment_add_child(&A, &C);
  nr_segment_add_child(&A, &D);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");
  B.name = nr_string_add(txn.trace_strings, "B");
  C.name = nr_string_add(txn.trace_strings, "C");
  D.name = nr_string_add(txn.trace_strings, "D");

  B.type = NR_SEGMENT_DATASTORE;
  B.attributes = NULL;
  B.typed_attributes = nr_zalloc(sizeof(nr_segment_typed_attributes_t));
  B.typed_attributes->datastore.sql_obfuscated = nr_strdup("SELECT");
  B.typed_attributes->datastore.instance.host = nr_strdup("localhost");
  B.typed_attributes->datastore.instance.database_name = nr_strdup("db");
  B.typed_attributes->datastore.instance.port_path_or_id = nr_strdup("3308");

  C.type = NR_SEGMENT_EXTERNAL;
  C.attributes = NULL;
  C.typed_attributes = nr_zalloc(sizeof(nr_segment_typed_attributes_t));
  C.typed_attributes->external.uri = nr_strdup("example.com");
  C.typed_attributes->external.library = nr_strdup("curl");
  C.typed_attributes->external.procedure = nr_strdup("GET");
  C.typed_attributes->external.transaction_guid = nr_strdup("guid");
  C.typed_attributes->external.status = 200;

  D.type = NR_SEGMENT_MESSAGE;
  D.attributes = NULL;
  D.typed_attributes = nr_zalloc(sizeof(nr_segment_typed_attributes_t));
  D.typed_attributes->message.destination_name = nr_strdup("queue_name");
  D.typed_attributes->message.messaging_system = nr_strdup("aws_sqs");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal("success", true, rv);
  test_buffer_contents("two kids", buf,
                       "[0,9,\"`0\",{},[[1,6,\"`1\",{},["
                       "[2,3,\"`2\",{"
                       "\"host\":\"localhost\","
                       "\"database_name\":\"db\","
                       "\"port_path_or_id\":\"3308\","
                       "\"sql_obfuscated\":\"SELECT\"},[]],"
                       "[4,5,\"`3\",{"
                       "\"uri\":\"example.com\","
                       "\"library\":\"curl\","
                       "\"procedure\":\"GET\","
                       "\"transaction_guid\":\"guid\","
                       "\"status\":200},[]],"
                       "[5,6,\"`4\","
                       "{\"destination_name\":\"queue_name\","
                       "\"messaging_system\":\"aws_sqs\"},[]]]]]]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 5);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_a = (nr_span_event_t*)nr_vector_get(span_events, 1);
  evt_b = (nr_span_event_t*)nr_vector_get(span_events, 2);
  evt_c = (nr_span_event_t*)nr_vector_get(span_events, 3);
  evt_d = (nr_span_event_t*)nr_vector_get(span_events, 4);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  SPAN_EVENT_COMPARE(evt_a, "A", NR_SPAN_GENERIC, evt_root, 2000, 5000);
  SPAN_EVENT_COMPARE(evt_b, "B", NR_SPAN_DATASTORE, evt_a, 3000, 1000);
  SPAN_EVENT_COMPARE_DATASTORE(evt_b, "localhost", "db", "SELECT",
                               "localhost:3308");
  SPAN_EVENT_COMPARE(evt_c, "C", NR_SPAN_HTTP, evt_a, 5000, 1000);
  SPAN_EVENT_COMPARE_EXTERNAL(evt_c, "example.com", "GET", "curl", 200);
  SPAN_EVENT_COMPARE(evt_d, "D", NR_SPAN_MESSAGE, evt_a, 6000, 1000);
  SPAN_EVENT_COMPARE_MESSAGE(evt_d, "queue_name", "aws_sqs", NULL);

  /* Clean up */
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_deinit(&A.children);

  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);
  nr_segment_destroy_fields(&C);
  nr_segment_destroy_fields(&D);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_two_generations(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;

  nrtxn_t txn = {.abs_start_time = 1000};

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_a;
  nr_span_event_t* evt_b;
  nr_span_event_t* evt_c;

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t A = {.txn = &txn, .start_time = 1000, .stop_time = 6000};
  nr_segment_t B = {.txn = &txn, .start_time = 2000, .stop_time = 3000};
  nr_segment_t C = {.txn = &txn, .start_time = 4000, .stop_time = 5000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(9, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.segment_count = 4;

  /* Create a collection of mock segments */

  /*    ------root-------
   *     ------A------
   *      --B-- --C--
   */

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&A.children);

  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&A, &B);
  nr_segment_add_child(&A, &C);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");
  B.name = nr_string_add(txn.trace_strings, "B");
  C.name = nr_string_add(txn.trace_strings, "C");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal("success", true, rv);
  test_buffer_contents("two kids", buf,
                       "[0,9,\"`0\",{},[[1,6,\"`1\",{},[[2,3,\"`2\",{},[]],[4,"
                       "5,\"`3\",{},[]]]]]]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 4);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_a = (nr_span_event_t*)nr_vector_get(span_events, 1);
  evt_b = (nr_span_event_t*)nr_vector_get(span_events, 2);
  evt_c = (nr_span_event_t*)nr_vector_get(span_events, 3);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  SPAN_EVENT_COMPARE(evt_a, "A", NR_SPAN_GENERIC, evt_root, 2000, 5000);
  SPAN_EVENT_COMPARE(evt_b, "B", NR_SPAN_GENERIC, evt_a, 3000, 1000);
  SPAN_EVENT_COMPARE(evt_c, "C", NR_SPAN_GENERIC, evt_a, 5000, 1000);

  /* Clean up */
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_deinit(&A.children);

  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);
  nr_segment_destroy_fields(&C);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_async_basic(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;

  nrtxn_t txn = {.abs_start_time = 1000};

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_main;
  nr_span_event_t* evt_loop;

  /*
   * Basic test: main context lasts the same timespan as ROOT, and spawns one
   * child context for part of its run time.
   *
   * These diagrams all follow the same pattern: time is shown in seconds on
   * the first row, followed by the ROOT node, and then individual contexts
   * with their nodes.  The "main" context indicates that no async_context
   * will be attached to nodes in that context.
   *
   * time (s)             0    1    2    3    4    5    6    7    8    9    10
   *                           |------------------- ROOT -------------------|
   * main                      |------------------- main -------------------|
   * async                          |- loop --|
   */

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t main_segment = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t loop_segment = {.txn = &txn, .start_time = 1000, .stop_time = 3000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(9, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.segment_count = 3;

  /* Create a collection of mock segments */
  nr_segment_children_init(&root.children);
  nr_segment_children_init(&main_segment.children);

  nr_segment_add_child(&root, &main_segment);
  nr_segment_add_child(&main_segment, &loop_segment);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  main_segment.name = nr_string_add(txn.trace_strings, "main");

  loop_segment.name = nr_string_add(txn.trace_strings, "loop");
  loop_segment.async_context = nr_string_add(txn.trace_strings, "async");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal(
      "Printing JSON for a basic async scenario must succeed", true, rv);
  test_buffer_contents("basic", buf,
                       "["
                       "0,9,\"`0\",{},"
                       "["
                       "["
                       "0,9,\"`1\",{},"
                       "["
                       "[1,3,\"`2\",{\"async_context\":\"`3\"},[]]"
                       "]"
                       "]"
                       "]"
                       "]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 3);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_main = (nr_span_event_t*)nr_vector_get(span_events, 1);
  evt_loop = (nr_span_event_t*)nr_vector_get(span_events, 2);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  SPAN_EVENT_COMPARE(evt_main, "main", NR_SPAN_GENERIC, evt_root, 1000, 9000);
  SPAN_EVENT_COMPARE(evt_loop, "loop", NR_SPAN_GENERIC, evt_main, 2000, 2000);

  /* Clean up */
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_deinit(&main_segment.children);

  nr_segment_destroy_fields(&main_segment);
  nr_segment_destroy_fields(&loop_segment);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_async_multi_child(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;

  nrtxn_t txn = {.abs_start_time = 1000};

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_main;
  nr_span_event_t* evt_a_a;
  nr_span_event_t* evt_b;
  nr_span_event_t* evt_a_b;

  /*
   * Multiple children test: main context lasts the same timespan as ROOT, and
   * spawns one child context with three nodes for part of its run time, one
   * of which has a duplicated name.
   *
   * time (s)             0    1    2    3    4    5    6    7    8    9    10
   *                           |------------------- ROOT -------------------|
   * main                      |------------------- main -------------------|
   * async                          |--- a_a ---|--- b ---|    | a_b  |
   */

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t main_segment = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t a_a = {.txn = &txn, .start_time = 1000, .stop_time = 3000};
  nr_segment_t b = {.txn = &txn, .start_time = 3000, .stop_time = 5000};
  nr_segment_t a_b = {.txn = &txn, .start_time = 6000, .stop_time = 7000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(9, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.segment_count = 5;

  /* Create a collection of mock segments */
  nr_segment_children_init(&root.children);
  nr_segment_children_init(&main_segment.children);

  nr_segment_add_child(&root, &main_segment);
  nr_segment_add_child(&main_segment, &a_a);
  nr_segment_add_child(&main_segment, &b);
  nr_segment_add_child(&main_segment, &a_b);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  main_segment.name = nr_string_add(txn.trace_strings, "main");

  a_a.name = nr_string_add(txn.trace_strings, "a");
  a_a.async_context = nr_string_add(txn.trace_strings, "async");

  b.name = nr_string_add(txn.trace_strings, "b");
  b.async_context = nr_string_add(txn.trace_strings, "async");

  a_b.name = nr_string_add(txn.trace_strings, "a");
  a_b.async_context = nr_string_add(txn.trace_strings, "async");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal("success", true, rv);
  test_buffer_contents(
      "Printing JSON for a three-child async scenario must succeed", buf,
      "["
      "0,9,\"`0\",{},"
      "["
      "["
      "0,9,\"`1\",{},"
      "["
      "[1,3,\"`2\",{\"async_context\":\"`3\"},[]],"
      "[3,5,\"`4\",{\"async_context\":\"`3\"},[]],"
      "[6,7,\"`2\",{\"async_context\":\"`3\"},[]]"
      "]"
      "]"
      "]"
      "]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 5);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_main = (nr_span_event_t*)nr_vector_get(span_events, 1);
  evt_a_a = (nr_span_event_t*)nr_vector_get(span_events, 2);
  evt_b = (nr_span_event_t*)nr_vector_get(span_events, 3);
  evt_a_b = (nr_span_event_t*)nr_vector_get(span_events, 4);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  SPAN_EVENT_COMPARE(evt_main, "main", NR_SPAN_GENERIC, evt_root, 1000, 9000);
  SPAN_EVENT_COMPARE(evt_a_a, "a", NR_SPAN_GENERIC, evt_main, 2000, 2000);
  SPAN_EVENT_COMPARE(evt_b, "b", NR_SPAN_GENERIC, evt_main, 4000, 2000);
  SPAN_EVENT_COMPARE(evt_a_b, "a", NR_SPAN_GENERIC, evt_main, 7000, 1000);

  /* Clean up */
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_deinit(&main_segment.children);

  nr_segment_destroy_fields(&main_segment);
  nr_segment_destroy_fields(&a_a);
  nr_segment_destroy_fields(&b);
  nr_segment_destroy_fields(&a_b);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_async_multi_context(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;

  nrtxn_t txn = {.abs_start_time = 1000};

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_main;
  nr_span_event_t* evt_a_a;
  nr_span_event_t* evt_b;
  nr_span_event_t* evt_a_b;
  nr_span_event_t* evt_c;
  nr_span_event_t* evt_d;

  /*
   * Multiple contexts test: main context lasts the same timespan as ROOT, and
   * spawns three child contexts with a mixture of nodes.
   *
   * time (s)             0    1    2    3    4    5    6    7    8    9    10
   *                           |------------------- ROOT -------------------|
   * main                      |------------------- main -------------------|
   * alpha                          |--- a_a --|--- b --|   | a_b |
   * beta                                |--- c ---|
   * gamma                                                             | d  |
   */

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t main_segment = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t a_a = {.txn = &txn, .start_time = 1000, .stop_time = 3000};
  nr_segment_t b = {.txn = &txn, .start_time = 3000, .stop_time = 5000};
  nr_segment_t a_b = {.txn = &txn, .start_time = 6000, .stop_time = 7000};
  nr_segment_t c = {.txn = &txn, .start_time = 2000, .stop_time = 4000};
  nr_segment_t d = {.txn = &txn, .start_time = 8000, .stop_time = 9000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(9, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.segment_count = 7;

  /* Create a collection of mock segments */
  nr_segment_children_init(&root.children);
  nr_segment_children_init(&main_segment.children);

  nr_segment_add_child(&root, &main_segment);
  nr_segment_add_child(&main_segment, &a_a);
  nr_segment_add_child(&main_segment, &b);
  nr_segment_add_child(&main_segment, &a_b);
  nr_segment_add_child(&main_segment, &c);
  nr_segment_add_child(&main_segment, &d);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  main_segment.name = nr_string_add(txn.trace_strings, "main");

  a_a.name = nr_string_add(txn.trace_strings, "a");
  a_a.async_context = nr_string_add(txn.trace_strings, "alpha");

  b.name = nr_string_add(txn.trace_strings, "b");
  b.async_context = nr_string_add(txn.trace_strings, "alpha");

  a_b.name = nr_string_add(txn.trace_strings, "a");
  a_b.async_context = nr_string_add(txn.trace_strings, "alpha");

  c.name = nr_string_add(txn.trace_strings, "c");
  c.async_context = nr_string_add(txn.trace_strings, "beta");

  d.name = nr_string_add(txn.trace_strings, "d");
  d.async_context = nr_string_add(txn.trace_strings, "gamma");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal("success", true, rv);
  test_buffer_contents("multiple contexts", buf,
                       "["
                       "0,9,\"`0\",{},"
                       "["
                       "["
                       "0,9,\"`1\",{},"
                       "["
                       "[1,3,\"`2\",{\"async_context\":\"`3\"},[]],"
                       "[3,5,\"`4\",{\"async_context\":\"`3\"},[]],"
                       "[6,7,\"`2\",{\"async_context\":\"`3\"},[]],"
                       "[2,4,\"`5\",{\"async_context\":\"`6\"},[]],"
                       "[8,9,\"`7\",{\"async_context\":\"`8\"},[]]"
                       "]"
                       "]"
                       "]"
                       "]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 7);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_main = (nr_span_event_t*)nr_vector_get(span_events, 1);
  evt_a_a = (nr_span_event_t*)nr_vector_get(span_events, 2);
  evt_b = (nr_span_event_t*)nr_vector_get(span_events, 3);
  evt_a_b = (nr_span_event_t*)nr_vector_get(span_events, 4);
  evt_c = (nr_span_event_t*)nr_vector_get(span_events, 5);
  evt_d = (nr_span_event_t*)nr_vector_get(span_events, 6);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  SPAN_EVENT_COMPARE(evt_main, "main", NR_SPAN_GENERIC, evt_root, 1000, 9000);
  SPAN_EVENT_COMPARE(evt_a_a, "a", NR_SPAN_GENERIC, evt_main, 2000, 2000);
  SPAN_EVENT_COMPARE(evt_b, "b", NR_SPAN_GENERIC, evt_main, 4000, 2000);
  SPAN_EVENT_COMPARE(evt_a_b, "a", NR_SPAN_GENERIC, evt_main, 7000, 1000);
  SPAN_EVENT_COMPARE(evt_c, "c", NR_SPAN_GENERIC, evt_main, 3000, 2000);
  SPAN_EVENT_COMPARE(evt_d, "d", NR_SPAN_GENERIC, evt_main, 9000, 1000);

  /* Clean up */
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_deinit(&main_segment.children);
  nr_segment_destroy_fields(&main_segment);

  nr_segment_destroy_fields(&a_a);
  nr_segment_destroy_fields(&b);
  nr_segment_destroy_fields(&a_b);
  nr_segment_destroy_fields(&c);
  nr_segment_destroy_fields(&d);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_async_context_nesting(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;

  nrtxn_t txn = {.abs_start_time = 1000};

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_main;
  nr_span_event_t* evt_a;
  nr_span_event_t* evt_b;
  nr_span_event_t* evt_c;
  nr_span_event_t* evt_d;
  nr_span_event_t* evt_e;
  nr_span_event_t* evt_f;
  nr_span_event_t* evt_g;

  /*
   * Context nesting test: contexts spawned from different main thread
   * contexts.
   *
   * time (s)             0    1    2    3    4    5    6    7    8    9    10
   *                           |------------------- ROOT -------------------|
   * main                      |------------------- main -------------------|
   *                                |--- a ---|----- b ------|
   *                                                    | c  |
   * alpha                               |---------- d ---------------------|
   *                                               |--- e ---|
   * beta                                          |--- f ---|
   * gamma                                                    | g |
   */

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t main_segment = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t a = {.txn = &txn, .start_time = 1000, .stop_time = 3000};
  nr_segment_t b = {.txn = &txn, .start_time = 3000, .stop_time = 6000};
  nr_segment_t g = {.txn = &txn, .start_time = 6200, .stop_time = 7000};

  /* b begets f and c, in that order */
  nr_segment_t f = {.txn = &txn, .start_time = 4000, .stop_time = 6000};
  nr_segment_t c = {.txn = &txn, .start_time = 5000, .stop_time = 6000};

  /* a begets d */
  nr_segment_t d = {.txn = &txn, .start_time = 2000, .stop_time = 9000};

  /* d begets e */
  nr_segment_t e = {.txn = &txn, .start_time = 4000, .stop_time = 6000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(9, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.segment_count = 9;

  /* Create a collection of mock segments */
  nr_segment_children_init(&root.children);
  nr_segment_children_init(&main_segment.children);
  nr_segment_children_init(&a.children);
  nr_segment_children_init(&b.children);
  nr_segment_children_init(&d.children);

  nr_segment_add_child(&root, &main_segment);
  nr_segment_add_child(&main_segment, &a);
  nr_segment_add_child(&main_segment, &b);

  nr_segment_add_child(&main_segment, &g);

  nr_segment_add_child(&a, &d);
  nr_segment_add_child(&d, &e);

  nr_segment_add_child(&b, &f);
  nr_segment_add_child(&b, &c);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  main_segment.name = nr_string_add(txn.trace_strings, "main");

  a.name = nr_string_add(txn.trace_strings, "a");
  b.name = nr_string_add(txn.trace_strings, "b");
  c.name = nr_string_add(txn.trace_strings, "c");
  d.name = nr_string_add(txn.trace_strings, "d");
  d.async_context = nr_string_add(txn.trace_strings, "alpha");

  e.name = nr_string_add(txn.trace_strings, "e");
  e.async_context = nr_string_add(txn.trace_strings, "alpha");

  f.name = nr_string_add(txn.trace_strings, "f");
  f.async_context = nr_string_add(txn.trace_strings, "beta");

  g.name = nr_string_add(txn.trace_strings, "g");
  g.async_context = nr_string_add(txn.trace_strings, "gamma");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal("success", true, rv);
  test_buffer_contents("context nesting", buf,
                       "["
                       "0,9,\"`0\",{},"
                       "["
                       "["
                       "0,9,\"`1\",{},"
                       "["
                       "[1,3,\"`2\",{},"
                       "["
                       "[2,9,\"`3\",{\"async_context\":\"`4\"},"
                       "["
                       "[4,6,\"`5\",{\"async_context\":\"`4\"},[]]"
                       "]"
                       "]"
                       "]"
                       "],"
                       "[3,6,\"`6\",{},"
                       "["
                       "[4,6,\"`7\",{\"async_context\":\"`8\"},[]],"
                       "[5,6,\"`9\",{},[]]"
                       "]"
                       "],"
                       "[6,7,\"`10\",{\"async_context\":\"`11\"},[]]"
                       "]"
                       "]"
                       "]"
                       "]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 9);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_main = (nr_span_event_t*)nr_vector_get(span_events, 1);
  evt_a = (nr_span_event_t*)nr_vector_get(span_events, 2);
  evt_d = (nr_span_event_t*)nr_vector_get(span_events, 3);
  evt_e = (nr_span_event_t*)nr_vector_get(span_events, 4);
  evt_b = (nr_span_event_t*)nr_vector_get(span_events, 5);
  evt_f = (nr_span_event_t*)nr_vector_get(span_events, 6);
  evt_c = (nr_span_event_t*)nr_vector_get(span_events, 7);
  evt_g = (nr_span_event_t*)nr_vector_get(span_events, 8);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  SPAN_EVENT_COMPARE(evt_main, "main", NR_SPAN_GENERIC, evt_root, 1000, 9000);
  SPAN_EVENT_COMPARE(evt_a, "a", NR_SPAN_GENERIC, evt_main, 2000, 2000);
  SPAN_EVENT_COMPARE(evt_b, "b", NR_SPAN_GENERIC, evt_main, 4000, 3000);
  SPAN_EVENT_COMPARE(evt_c, "c", NR_SPAN_GENERIC, evt_b, 6000, 1000);
  SPAN_EVENT_COMPARE(evt_d, "d", NR_SPAN_GENERIC, evt_a, 3000, 7000);
  SPAN_EVENT_COMPARE(evt_e, "e", NR_SPAN_GENERIC, evt_d, 5000, 2000);
  SPAN_EVENT_COMPARE(evt_f, "f", NR_SPAN_GENERIC, evt_b, 5000, 2000);
  SPAN_EVENT_COMPARE(evt_g, "g", NR_SPAN_GENERIC, evt_main, 7200, 800);

  /* Clean up */
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_deinit(&main_segment.children);
  nr_segment_children_deinit(&a.children);
  nr_segment_children_deinit(&b.children);
  nr_segment_children_deinit(&d.children);

  nr_segment_destroy_fields(&main_segment);
  nr_segment_destroy_fields(&a);
  nr_segment_destroy_fields(&b);
  nr_segment_destroy_fields(&c);
  nr_segment_destroy_fields(&d);
  nr_segment_destroy_fields(&e);
  nr_segment_destroy_fields(&f);
  nr_segment_destroy_fields(&g);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_async_with_data(void) {
  bool rv;
  nrbuf_t* buf;
  nrpool_t* segment_names;
  nrobj_t* value;

  nrtxn_t txn = {.abs_start_time = 1000};

  /*
   * Data hash testing: ensure that we never overwrite a data hash, and also
   * ensure that we never modify it.
   *
   * time (s)             0    1    2    3    4    5    6    7    8    9    10
   *                           |------------------- ROOT -------------------|
   * main                      |------------------- main -------------------|
   * async                          |- loop --|
   */

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t main_segment = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t loop = {.txn = &txn, .start_time = 1000, .stop_time = 3000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.segment_count = 3;

  /* Create a collection of mock segments */
  nr_segment_children_init(&root.children);
  nr_segment_children_init(&main_segment.children);

  nr_segment_add_child(&root, &main_segment);
  nr_segment_add_child(&main_segment, &loop);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  main_segment.name = nr_string_add(txn.trace_strings, "main");

  loop.name = nr_string_add(txn.trace_strings, "loop");
  loop.async_context = nr_string_add(txn.trace_strings, "async");

  loop.attributes = nr_attributes_create(NULL);
  main_segment.attributes = nr_attributes_create(NULL);

  value = nro_new_string("bar");
  nr_segment_attributes_user_add(
      &main_segment, NR_ATTRIBUTE_DESTINATION_TXN_TRACE, "foo", value);
  nr_segment_attributes_user_add(&loop, NR_ATTRIBUTE_DESTINATION_TXN_TRACE,
                                 "foo", value);
  nro_delete(value);

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, NULL, NULL, NULL, &txn, &root,
                                             segment_names);
  tlib_pass_if_bool_equal("success", true, rv);
  test_buffer_contents(
      "basic", buf,
      "["
      "0,9,\"`0\",{},"
      "["
      "["
      "0,9,\"`1\",{\"foo\":\"bar\"},"
      "["
      "[1,3,\"`2\",{\"async_context\":\"`3\",\"foo\":\"bar\"},[]]"
      "]"
      "]"
      "]"
      "]");

  /* Clean up */
  nr_segment_children_deinit(&root.children);
  nr_segment_children_deinit(&main_segment.children);

  nr_segment_destroy_fields(&root);
  nr_segment_destroy_fields(&main_segment);
  nr_segment_destroy_fields(&loop);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
}

static void test_json_print_segments_with_sampling(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;

  nrtxn_t txn = {.abs_start_time = 1000};
  nr_set_t* set;

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_b;

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t A = {.txn = &txn, .start_time = 1000, .stop_time = 6000};
  nr_segment_t B = {.txn = &txn, .start_time = 2000, .stop_time = 5000};
  nr_segment_t C = {.txn = &txn, .start_time = 3000, .stop_time = 4000};
  // clang-format on

  set = nr_set_create();
  nr_set_insert(set, (void*)&root);
  nr_set_insert(set, (void*)&B);
  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(8, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.segment_count = 4;

  /* Create a collection of mock segments */

  /*    ------root-------
   *       ----A----
   *       ----B----
   *         --C--
   */

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&A.children);
  nr_segment_children_init(&B.children);

  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&A, &B);
  nr_segment_add_child(&B, &C);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");
  B.name = nr_string_add(txn.trace_strings, "B");
  C.name = nr_string_add(txn.trace_strings, "C");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, set, set, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal(
      "Printing JSON for a sampled tree of segments must succeed", true, rv);
  test_buffer_contents("Free samples", buf,
                       "[0,9,\"`0\",{},[[2,5,\"`1\",{},[]]]]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 2);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_b = (nr_span_event_t*)nr_vector_get(span_events, 1);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  SPAN_EVENT_COMPARE(evt_b, "B", NR_SPAN_GENERIC, evt_root, 3000, 3000);

  /* Clean up */
  nr_set_destroy(&set);
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_deinit(&A.children);
  nr_segment_children_deinit(&B.children);

  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);
  nr_segment_destroy_fields(&C);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_with_sampling_cousin_parent(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;

  nrtxn_t txn = {.abs_start_time = 1000};
  nr_set_t* set;

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_c;
  nr_span_event_t* evt_d;
  nr_span_event_t* evt_f;
  nr_span_event_t* evt_g;
  nr_span_event_t* evt_i;

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 14000};
  nr_segment_t A = {.txn = &txn, .start_time = 1000, .stop_time = 6000};
  nr_segment_t B = {.txn = &txn, .start_time = 2000, .stop_time = 5000};
  nr_segment_t C = {.txn = &txn, .start_time = 1000, .stop_time = 5000};
  nr_segment_t D = {.txn = &txn, .start_time = 1000, .stop_time = 6000};
  nr_segment_t E = {.txn = &txn, .start_time = 1000, .stop_time = 4000};
  nr_segment_t F = {.txn = &txn, .start_time = 4000, .stop_time = 6000};
  nr_segment_t G = {.txn = &txn, .start_time = 5000, .stop_time = 5500};
  nr_segment_t H = {.txn = &txn, .start_time = 1000, .stop_time = 13000};
  nr_segment_t I = {.txn = &txn, .start_time = 1000, .stop_time = 3000};
  nr_segment_t J = {.txn = &txn, .start_time = 3000, .stop_time = 13000};
  nr_segment_t K = {.txn = &txn, .start_time = 2000, .stop_time = 11000};
  // clang-format on

  set = nr_set_create();
  nr_set_insert(set, (void*)&root);
  nr_set_insert(set, (void*)&C);
  nr_set_insert(set, (void*)&D);
  nr_set_insert(set, (void*)&F);
  nr_set_insert(set, (void*)&G);
  nr_set_insert(set, (void*)&I);

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(8, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.segment_count = 11;

  // clang-format off
  /*
   * The mock tree looks like this:
   *
   *
   *            --------------------*(0,14)root---------------------
   *               /                   |                          \
   *         --(1,6)A--           --*(1,6)D--            --------(1,13)H--------
   *          /        \           /        \            /        |         \
   *      -(2,5)B- -*(1,5)C-   -(1,4)E- -*(4,6)F-   -*(1,3)I-  -(3,13)J- -(2,11)K-
   *                   |                   /            |
   *                   |               -*(5,5)G-        ^
   *                   |                                |
   *                   +---------------->---------------+
   *
   *  Key:
   *  Sampled - *
   *
   *  One would think that root would be I's parent. Because of prefix
   *  traversal, C is I's parent. This is expected because the provided
   *  tree was invalid.
   */
  // clang-format on

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&A.children);
  nr_segment_children_init(&B.children);
  nr_segment_children_init(&C.children);
  nr_segment_children_init(&D.children);
  nr_segment_children_init(&E.children);
  nr_segment_children_init(&F.children);
  nr_segment_children_init(&G.children);
  nr_segment_children_init(&H.children);
  nr_segment_children_init(&I.children);
  nr_segment_children_init(&J.children);
  nr_segment_children_init(&K.children);

  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&A, &B);
  nr_segment_add_child(&B, &C);
  nr_segment_add_child(&root, &D);
  nr_segment_add_child(&D, &E);
  nr_segment_add_child(&D, &F);
  nr_segment_add_child(&F, &G);
  nr_segment_add_child(&D, &F);
  nr_segment_add_child(&root, &H);
  nr_segment_add_child(&H, &I);
  nr_segment_add_child(&H, &J);
  nr_segment_add_child(&H, &K);
  nr_segment_add_child(&C, &I);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");
  B.name = nr_string_add(txn.trace_strings, "B");
  C.name = nr_string_add(txn.trace_strings, "C");
  D.name = nr_string_add(txn.trace_strings, "D");
  E.name = nr_string_add(txn.trace_strings, "E");
  F.name = nr_string_add(txn.trace_strings, "F");
  G.name = nr_string_add(txn.trace_strings, "G");
  H.name = nr_string_add(txn.trace_strings, "H");
  I.name = nr_string_add(txn.trace_strings, "I");
  J.name = nr_string_add(txn.trace_strings, "J");
  K.name = nr_string_add(txn.trace_strings, "K");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, set, set, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal(
      "Printing JSON for a sampled cousin parent tree of segments must "
      "succeed",
      true, rv);
  test_buffer_contents("Cousin Parent", buf,
                       "[0,14,\"`0\",{},[[1,5,\"`1\",{},[[1,3,\"`2\",{},[]]]],["
                       "1,6,\"`3\",{},[[4,6,\"`4\",{},[[5,5,\"`5\",{},[]]]]]]]"
                       "]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 6);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_c = (nr_span_event_t*)nr_vector_get(span_events, 1);
  evt_i = (nr_span_event_t*)nr_vector_get(span_events, 2);
  evt_d = (nr_span_event_t*)nr_vector_get(span_events, 3);
  evt_f = (nr_span_event_t*)nr_vector_get(span_events, 4);
  evt_g = (nr_span_event_t*)nr_vector_get(span_events, 5);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     14000);
  SPAN_EVENT_COMPARE(evt_c, "C", NR_SPAN_GENERIC, evt_root, 2000, 4000);
  SPAN_EVENT_COMPARE(evt_i, "I", NR_SPAN_GENERIC, evt_c, 2000, 2000);
  SPAN_EVENT_COMPARE(evt_d, "D", NR_SPAN_GENERIC, evt_root, 2000, 5000);
  SPAN_EVENT_COMPARE(evt_f, "F", NR_SPAN_GENERIC, evt_d, 5000, 2000);
  SPAN_EVENT_COMPARE(evt_g, "G", NR_SPAN_GENERIC, evt_f, 6000, 500);

  /* Clean up */
  nr_set_destroy(&set);
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_deinit(&A.children);
  nr_segment_children_deinit(&B.children);
  nr_segment_children_deinit(&C.children);
  nr_segment_children_deinit(&D.children);
  nr_segment_children_deinit(&E.children);
  nr_segment_children_deinit(&F.children);
  nr_segment_children_deinit(&G.children);
  nr_segment_children_deinit(&H.children);
  nr_segment_children_deinit(&I.children);
  nr_segment_children_deinit(&J.children);
  nr_segment_children_deinit(&K.children);

  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);
  nr_segment_destroy_fields(&C);
  nr_segment_destroy_fields(&D);
  nr_segment_destroy_fields(&E);
  nr_segment_destroy_fields(&F);
  nr_segment_destroy_fields(&G);
  nr_segment_destroy_fields(&H);
  nr_segment_destroy_fields(&I);
  nr_segment_destroy_fields(&J);
  nr_segment_destroy_fields(&K);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_with_sampling_inner_loop(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;

  nrtxn_t txn = {.abs_start_time = 1000};
  nr_set_t* trace_set;
  nr_set_t* span_set;

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_a;
  nr_span_event_t* evt_d;
  nr_span_event_t* evt_f;
  nr_span_event_t* evt_g;

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t A = {.txn = &txn, .start_time = 1000, .stop_time = 6000};
  nr_segment_t B = {.txn = &txn, .start_time = 2000, .stop_time = 5000};
  nr_segment_t C = {.txn = &txn, .start_time = 3000, .stop_time = 4000};
  nr_segment_t D = {.txn = &txn, .start_time = 1000, .stop_time = 6000};
  nr_segment_t E = {.txn = &txn, .start_time = 1000, .stop_time = 4000};
  nr_segment_t F = {.txn = &txn, .start_time = 4000, .stop_time = 6000};
  nr_segment_t G = {.txn = &txn, .start_time = 5000, .stop_time = 5500};
  // clang-format on

  trace_set = nr_set_create();
  nr_set_insert(trace_set, (void*)&root);
  nr_set_insert(trace_set, (void*)&C);
  nr_set_insert(trace_set, (void*)&E);
  nr_set_insert(trace_set, (void*)&G);

  span_set = nr_set_create();
  nr_set_insert(span_set, (void*)&root);
  nr_set_insert(span_set, (void*)&A);
  nr_set_insert(span_set, (void*)&D);
  nr_set_insert(span_set, (void*)&F);
  nr_set_insert(span_set, (void*)&G);

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(8, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.segment_count = 7;

  // clang-format off
  /*
   * The mock tree looks like this:
   *
   *
   *   +--------->---------+
   *   |                   |
   *   |          ----+*(0,9)root------
   *   |           /                  \
   *   |      -+(1,6)A--           -+(1,6)D--
   *   ^      /        \           /        \
   *   |  -(2,5)B- -*(3,4)C-  -*(1,4)E-  -+(4,6)F-
   *   |                         |         /
   *   |                         |     +*(5,5)G-
   *   +-----------<-------------+
   *
   *  Key:
   *  Sampled trace - *
   *  Sampled spans - +
   *
   */
  // clang-format on

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&A.children);
  nr_segment_children_init(&B.children);
  nr_segment_children_init(&C.children);
  nr_segment_children_init(&D.children);
  nr_segment_children_init(&E.children);
  nr_segment_children_init(&F.children);
  nr_segment_children_init(&G.children);

  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&A, &B);
  nr_segment_add_child(&B, &C);
  nr_segment_add_child(&root, &D);
  nr_segment_add_child(&D, &E);
  nr_segment_add_child(&D, &F);
  nr_segment_add_child(&F, &G);
  nr_segment_add_child(&D, &F);
  nr_segment_add_child(&E, &root);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");
  B.name = nr_string_add(txn.trace_strings, "B");
  C.name = nr_string_add(txn.trace_strings, "C");
  D.name = nr_string_add(txn.trace_strings, "D");
  E.name = nr_string_add(txn.trace_strings, "E");
  F.name = nr_string_add(txn.trace_strings, "F");
  G.name = nr_string_add(txn.trace_strings, "G");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(
      buf, span_events, trace_set, span_set, &txn, &root, segment_names);
  tlib_pass_if_bool_equal(
      "Printing JSON for a sampled tree of segments must succeed", true, rv);
  test_buffer_contents("Inner Loop", buf,
                       "[0,9,\"`0\",{},[[3,4,\"`1\",{},[]],[1,4,\"`2\",{},[]],["
                       "5,5,\"`3\",{},[]]]]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 5);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_a = (nr_span_event_t*)nr_vector_get(span_events, 1);
  evt_d = (nr_span_event_t*)nr_vector_get(span_events, 2);
  evt_f = (nr_span_event_t*)nr_vector_get(span_events, 3);
  evt_g = (nr_span_event_t*)nr_vector_get(span_events, 4);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  SPAN_EVENT_COMPARE(evt_a, "A", NR_SPAN_GENERIC, evt_root, 2000, 5000);
  SPAN_EVENT_COMPARE(evt_d, "D", NR_SPAN_GENERIC, evt_root, 2000, 5000);
  SPAN_EVENT_COMPARE(evt_f, "F", NR_SPAN_GENERIC, evt_d, 5000, 2000);
  SPAN_EVENT_COMPARE(evt_g, "G", NR_SPAN_GENERIC, evt_f, 6000, 500);

  /* Clean up */
  nr_set_destroy(&trace_set);
  nr_set_destroy(&span_set);
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_deinit(&A.children);
  nr_segment_children_deinit(&B.children);
  nr_segment_children_deinit(&C.children);
  nr_segment_children_deinit(&D.children);
  nr_segment_children_deinit(&E.children);
  nr_segment_children_deinit(&F.children);
  nr_segment_children_deinit(&G.children);

  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);
  nr_segment_destroy_fields(&C);
  nr_segment_destroy_fields(&D);
  nr_segment_destroy_fields(&E);
  nr_segment_destroy_fields(&F);
  nr_segment_destroy_fields(&G);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_with_sampling_genghis_khan(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;

  nrtxn_t txn = {.abs_start_time = 1000};
  nr_set_t* set;

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_a;
  nr_span_event_t* evt_c;
  nr_span_event_t* evt_e;
  nr_span_event_t* evt_f;
  nr_span_event_t* evt_g;
  nr_span_event_t* evt_h;
  nr_span_event_t* evt_i;

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t A = {.txn = &txn, .start_time = 1000, .stop_time = 6000};
  nr_segment_t B = {.txn = &txn, .start_time = 2000, .stop_time = 5000};
  nr_segment_t C = {.txn = &txn, .start_time = 3000, .stop_time = 4000};
  nr_segment_t D = {.txn = &txn, .start_time = 1000, .stop_time = 7000};
  nr_segment_t E = {.txn = &txn, .start_time = 1000, .stop_time = 4000};
  nr_segment_t F = {.txn = &txn, .start_time = 4000, .stop_time = 6000};
  nr_segment_t G = {.txn = &txn, .start_time = 0,    .stop_time = 8000};
  nr_segment_t H = {.txn = &txn, .start_time = 2000, .stop_time = 3000};
  nr_segment_t I = {.txn = &txn, .start_time = 0,    .stop_time = 6000};
  // clang-format on

  set = nr_set_create();
  nr_set_insert(set, (void*)&root);
  nr_set_insert(set, (void*)&A);
  nr_set_insert(set, (void*)&C);
  nr_set_insert(set, (void*)&E);
  nr_set_insert(set, (void*)&F);
  nr_set_insert(set, (void*)&G);
  nr_set_insert(set, (void*)&H);
  nr_set_insert(set, (void*)&I);

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(8, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.segment_count = 9;

  // clang-format off
  /*
   * The mock tree looks like this:
   *    -----------------------------------*(0,9)root------------------------------------
   *     /         |         |        |         |         |         |         |        \
   * -*(1,6)A- -(2,5)B- -*(3,4)C- -(1,7)D- -*(1,4)E- -*(4,6)F- -*(0,8)G- -*(2,3)H- -*(0,6)I-
   *
   *  Key:
   *  Sampled - *
   *
   */
  // clang-format on

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&A.children);
  nr_segment_children_init(&B.children);
  nr_segment_children_init(&C.children);
  nr_segment_children_init(&D.children);
  nr_segment_children_init(&E.children);
  nr_segment_children_init(&F.children);
  nr_segment_children_init(&G.children);
  nr_segment_children_init(&H.children);
  nr_segment_children_init(&I.children);

  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&root, &B);
  nr_segment_add_child(&root, &C);
  nr_segment_add_child(&root, &D);
  nr_segment_add_child(&root, &E);
  nr_segment_add_child(&root, &F);
  nr_segment_add_child(&root, &G);
  nr_segment_add_child(&root, &H);
  nr_segment_add_child(&root, &I);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");
  B.name = nr_string_add(txn.trace_strings, "B");
  C.name = nr_string_add(txn.trace_strings, "C");
  D.name = nr_string_add(txn.trace_strings, "D");
  E.name = nr_string_add(txn.trace_strings, "E");
  F.name = nr_string_add(txn.trace_strings, "F");
  G.name = nr_string_add(txn.trace_strings, "G");
  H.name = nr_string_add(txn.trace_strings, "H");
  I.name = nr_string_add(txn.trace_strings, "I");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, set, set, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal(
      "Printing JSON for a genghis khan sampled tree of segments must "
      "succeed",
      true, rv);
  test_buffer_contents("genghis khan", buf,
                       "[0,9,\"`0\",{},[[1,6,\"`1\",{},[]],[3,4,\"`2\",{},[]],["
                       "1,4,\"`3\",{},[]],[4,6,\"`4\",{},[]],[0,8,\"`5\",{},[]]"
                       ",[2,3,\"`6\",{},[]],[0,6,\"`7\",{},[]]]]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 8);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_a = (nr_span_event_t*)nr_vector_get(span_events, 1);
  evt_c = (nr_span_event_t*)nr_vector_get(span_events, 2);
  evt_e = (nr_span_event_t*)nr_vector_get(span_events, 3);
  evt_f = (nr_span_event_t*)nr_vector_get(span_events, 4);
  evt_g = (nr_span_event_t*)nr_vector_get(span_events, 5);
  evt_h = (nr_span_event_t*)nr_vector_get(span_events, 6);
  evt_i = (nr_span_event_t*)nr_vector_get(span_events, 7);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  SPAN_EVENT_COMPARE(evt_a, "A", NR_SPAN_GENERIC, evt_root, 2000, 5000);
  SPAN_EVENT_COMPARE(evt_c, "C", NR_SPAN_GENERIC, evt_root, 4000, 1000);
  SPAN_EVENT_COMPARE(evt_e, "E", NR_SPAN_GENERIC, evt_root, 2000, 3000);
  SPAN_EVENT_COMPARE(evt_f, "F", NR_SPAN_GENERIC, evt_root, 5000, 2000);
  SPAN_EVENT_COMPARE(evt_g, "G", NR_SPAN_GENERIC, evt_root, 1000, 8000);
  SPAN_EVENT_COMPARE(evt_h, "H", NR_SPAN_GENERIC, evt_root, 3000, 1000);
  SPAN_EVENT_COMPARE(evt_i, "I", NR_SPAN_GENERIC, evt_root, 1000, 6000);

  /* Clean up */
  nr_set_destroy(&set);
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_deinit(&A.children);
  nr_segment_children_deinit(&B.children);
  nr_segment_children_deinit(&C.children);
  nr_segment_children_deinit(&D.children);
  nr_segment_children_deinit(&E.children);
  nr_segment_children_deinit(&F.children);
  nr_segment_children_deinit(&G.children);
  nr_segment_children_deinit(&H.children);
  nr_segment_children_deinit(&I.children);

  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);
  nr_segment_destroy_fields(&C);
  nr_segment_destroy_fields(&D);
  nr_segment_destroy_fields(&E);
  nr_segment_destroy_fields(&F);
  nr_segment_destroy_fields(&G);
  nr_segment_destroy_fields(&H);
  nr_segment_destroy_fields(&I);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_json_print_segments_extremely_short(void) {
  bool rv;
  nrbuf_t* buf;
  nr_vector_t* span_events;
  nrpool_t* segment_names;

  nrtxn_t txn = {.abs_start_time = 1000};

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_a;
  nr_span_event_t* evt_c;

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t A = {.txn = &txn, .start_time = 1000, .stop_time = 6000};
  nr_segment_t B = {.txn = &txn, .start_time = 2000, .stop_time = 2000};
  nr_segment_t C = {.txn = &txn, .start_time = 3000, .stop_time = 4000};
  // clang-format on

  buf = nr_buffer_create(4096, 4096);
  span_events = nr_vector_create(9, nr_vector_span_event_dtor, NULL);
  segment_names = nr_string_pool_create();

  /* Mock up the transaction */
  mock_txn(&txn, &root);
  txn.segment_count = 4;

  /* Create a collection of mock segments */

  /*    ------root-------
   *       ----A----
   *        ---B--- (zero duration)
   *         --C--
   */

  nr_segment_children_init(&root.children);
  nr_segment_children_init(&A.children);
  nr_segment_children_init(&B.children);

  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&A, &B);
  nr_segment_add_child(&B, &C);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");
  B.name = nr_string_add(txn.trace_strings, "B");
  C.name = nr_string_add(txn.trace_strings, "C");

  /*
   * Test : Normal operation
   */
  rv = nr_segment_traces_json_print_segments(buf, span_events, NULL, NULL, &txn,
                                             &root, segment_names);
  tlib_pass_if_bool_equal(
      "A segment with zero duration must not appear in the transaction trace",
      true, rv);
  test_buffer_contents("segment B omitted", buf,
                       "[0,9,\"`0\",{},[[1,6,\"`1\",{},[[3,4,\"`2\",{},"
                       "[]]]]]]");

  tlib_pass_if_uint_equal("span event size", nr_vector_size(span_events), 3);

  evt_root = (nr_span_event_t*)nr_vector_get(span_events, 0);
  evt_a = (nr_span_event_t*)nr_vector_get(span_events, 1);
  evt_c = (nr_span_event_t*)nr_vector_get(span_events, 2);

  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  SPAN_EVENT_COMPARE(evt_a, "A", NR_SPAN_GENERIC, evt_root, 2000, 5000);
  SPAN_EVENT_COMPARE(evt_c, "C", NR_SPAN_GENERIC, evt_a, 4000, 1000);

  /* Clean up */
  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);

  nr_segment_children_deinit(&A.children);
  nr_segment_children_deinit(&B.children);

  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);
  nr_segment_destroy_fields(&C);

  cleanup_mock_txn(&txn);
  nr_string_pool_destroy(&segment_names);

  nr_buffer_destroy(&buf);
  nr_vector_destroy(&span_events);
}

static void test_trace_create_data_bad_parameters(void) {
  nrtxn_t txn = {.abs_start_time = 1000};
  uintptr_t i;
  nr_segment_tree_sampling_metadata_t metadata = {.trace_set = NULL};
  nrtxnfinal_t result = {.trace_json = NULL};

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  // clang-format on

  nrobj_t* agent_attributes = nro_create_from_json("[\"agent_attributes\"]");
  nrobj_t* user_attributes = nro_create_from_json("[\"user_attributes\"]");
  nrobj_t* intrinsics = nro_create_from_json("[\"intrinsics\"]");

  metadata.out = &result;
  metadata.trace_set = nr_set_create();

  /*
   * Test : Bad parameters
   */
  nr_segment_traces_create_data(NULL, 2 * NR_TIME_DIVISOR, &metadata,
                                agent_attributes, user_attributes, intrinsics,
                                true, false);
  tlib_pass_if_null(
      "A NULL transaction pointer must not succeed in creating a trace",
      metadata.out->trace_json);

  nr_segment_traces_create_data(&txn, 2 * NR_TIME_DIVISOR, &metadata,
                                agent_attributes, user_attributes, intrinsics,
                                true, false);
  tlib_pass_if_null(
      "A zero-sized transaction must not succeed in creating a trace",
      metadata.out->trace_json);

  txn.segment_count = 1;
  txn.segment_root = &root;

  nr_segment_traces_create_data(&txn, 0, &metadata, agent_attributes,
                                user_attributes, intrinsics, true, false);
  tlib_pass_if_null(
      "A zero-duration transaction must not succeed in creating a trace",
      metadata.out->trace_json);

  /* Insert initial values. */
  for (i = 0; i < NR_MAX_SEGMENTS + 1; i++) {
    nr_set_insert(metadata.trace_set, (const void*)i);
  }

  nr_segment_traces_create_data(&txn, 2 * NR_TIME_DIVISOR, &metadata,
                                agent_attributes, user_attributes, intrinsics,
                                true, false);
  tlib_pass_if_null(
      "A transaction with more than NR_MAX_SEGMENTS segments must not "
      "succeed "
      "in creating "
      "a trace",
      metadata.out->trace_json);

  nr_segment_traces_create_data(&txn, 2 * NR_TIME_DIVISOR, NULL,
                                agent_attributes, user_attributes, intrinsics,
                                true, false);

  nr_set_destroy(&metadata.trace_set);
  nro_delete(agent_attributes);
  nro_delete(user_attributes);
  nro_delete(intrinsics);
}

static void test_trace_create_trace_spans(void) {
  nrtxn_t txn = {.abs_start_time = 1000};
  nr_segment_tree_sampling_metadata_t metadata = {0};
  nrtxnfinal_t result = {0};

  nrobj_t* agent_attributes = nro_create_from_json("[\"agent_attributes\"]");
  nrobj_t* user_attributes = nro_create_from_json("[\"user_attributes\"]");
  nrobj_t* intrinsics = nro_create_from_json("[\"intrinsics\"]");

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t A = {.txn = &txn, .start_time = 1000, .stop_time = 2000};
  // clang-format on

  metadata.out = &result;

  /* Mock up a transaction */
  mock_txn(&txn, &root);
  txn.segment_count = 2;
  txn.name = nr_strdup("WebTransaction/*");

  /* Mock up a tree of segments */
  /* Create a collection of mock segments */
  nr_segment_children_init(&root.children);
  nr_segment_add_child(&root, &A);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");

  /*
   * Test : Create none of span events and traces
   */
  nr_segment_traces_create_data(&txn, 2 * NR_TIME_DIVISOR, &metadata,
                                agent_attributes, user_attributes, intrinsics,
                                false, false);

  tlib_pass_if_null("Trace must not be created", metadata.out->trace_json);
  tlib_pass_if_null("Span events must not be created",
                    metadata.out->span_events);

  nr_realfree((void**)&metadata.out->trace_json);
  nr_vector_destroy(&metadata.out->span_events);

  /*
   * Test : Create both span events and traces
   */
  nr_segment_traces_create_data(&txn, 2 * NR_TIME_DIVISOR, &metadata,
                                agent_attributes, user_attributes, intrinsics,
                                true, true);

  tlib_pass_if_not_null("Both traces and span events must be created",
                        metadata.out->trace_json);
  tlib_pass_if_not_null("Both traces and span events must be created",
                        metadata.out->span_events);

  nr_realfree((void**)&metadata.out->trace_json);
  nr_vector_destroy(&metadata.out->span_events);

  /*
   * Test : Create only traces
   */
  nr_segment_traces_create_data(&txn, 2 * NR_TIME_DIVISOR, &metadata,
                                agent_attributes, user_attributes, intrinsics,
                                true, false);

  tlib_pass_if_not_null("Create only traces", metadata.out->trace_json);
  tlib_pass_if_null("Create only traces", metadata.out->span_events);

  nr_realfree((void**)&metadata.out->trace_json);
  nr_vector_destroy(&metadata.out->span_events);

  /*
   * Test : Create only span events
   */
  nr_segment_traces_create_data(&txn, 2 * NR_TIME_DIVISOR, &metadata,
                                agent_attributes, user_attributes, intrinsics,
                                false, true);

  tlib_pass_if_null("Create only span events", metadata.out->trace_json);
  tlib_pass_if_not_null("Create only span events", metadata.out->span_events);

  nr_realfree((void**)&metadata.out->trace_json);
  nr_vector_destroy(&metadata.out->span_events);

  /* Clean up */
  nr_free(txn.name);
  cleanup_mock_txn(&txn);

  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);
  nr_segment_destroy_fields(&A);

  nro_delete(agent_attributes);
  nro_delete(user_attributes);
  nro_delete(intrinsics);
}

static void test_trace_create_data(void) {
  nrtxn_t txn = {.abs_start_time = 1000};
  nr_segment_tree_sampling_metadata_t metadata = {.trace_set = NULL};
  nrtxnfinal_t result = {.trace_json = NULL};

  nrobj_t* agent_attributes = nro_create_from_json("[\"agent_attributes\"]");
  nrobj_t* user_attributes = nro_create_from_json("[\"user_attributes\"]");
  nrobj_t* intrinsics = nro_create_from_json("[\"intrinsics\"]");
  nrobj_t* obj;

  nr_span_event_t* evt_root;
  nr_span_event_t* evt_a;
  nr_span_event_t* evt_b;

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t A = {.txn = &txn, .start_time = 1000, .stop_time = 2000};
  nr_segment_t B = {.txn = &txn, .start_time = 3000, .stop_time = 4000};
  // clang-format on

  metadata.out = &result;

  /* Mock up a transaction */
  mock_txn(&txn, &root);
  txn.segment_count = 3;
  txn.name = nr_strdup("WebTransaction/*");

  /* Mock up a tree of segments */
  /* Create a collection of mock segments */

  /*    ------root-------
   *      --A-- --B--
   */

  nr_segment_children_init(&root.children);
  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&root, &B);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");
  B.name = nr_string_add(txn.trace_strings, "B");

  /*
   * Test : Normal operation
   */
  nr_segment_traces_create_data(&txn, 2 * NR_TIME_DIVISOR, &metadata,
                                agent_attributes, user_attributes, intrinsics,
                                true, true);

  tlib_pass_if_str_equal(
      "A multi-node transaction must succeed in creating a trace",
      metadata.out->trace_json,
      "[[0,{},{},[0,2000,\"ROOT\",{},[[0,9,\"`0\",{},[[1,2,"
      "\"`1\",{},[]],[3,4,\"`2\",{},[]]]]]],"
      "{\"agentAttributes\":[\"agent_attributes\"],"
      "\"userAttributes\":[\"user_attributes\"],"
      "\"intrinsics\":[\"intrinsics\"]}],"
      "[\"WebTransaction\\/*\",\"A\",\"B\"]]");

  obj = nro_create_from_json(metadata.out->trace_json);
  tlib_pass_if_not_null(
      "A multi-node transaction must succeed in creating valid json", obj);

  tlib_pass_if_uint_equal("span event size",
                          nr_vector_size(metadata.out->span_events), 3);

  evt_root = (nr_span_event_t*)nr_vector_get(metadata.out->span_events, 0);
  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  evt_a = (nr_span_event_t*)nr_vector_get(metadata.out->span_events, 1);
  SPAN_EVENT_COMPARE(evt_a, "A", NR_SPAN_GENERIC, evt_root, 2000, 1000);
  evt_b = (nr_span_event_t*)nr_vector_get(metadata.out->span_events, 2);
  SPAN_EVENT_COMPARE(evt_b, "B", NR_SPAN_GENERIC, evt_root, 4000, 1000);

  /* Clean up */
  nro_delete(obj);
  nr_free(metadata.out->trace_json);
  nr_vector_destroy(&metadata.out->span_events);
  nr_free(txn.name);

  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);
  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);

  cleanup_mock_txn(&txn);

  nro_delete(agent_attributes);
  nro_delete(user_attributes);
  nro_delete(intrinsics);
}

static void test_trace_create_data_with_sampling(void) {
  nrtxn_t txn = {.abs_start_time = 1000};
  nr_segment_tree_sampling_metadata_t metadata = {.trace_set = NULL};
  nrtxnfinal_t result = {.trace_json = NULL};

  nrobj_t* agent_attributes = nro_create_from_json("[\"agent_attributes\"]");
  nrobj_t* user_attributes = nro_create_from_json("[\"user_attributes\"]");
  nrobj_t* intrinsics = nro_create_from_json("[\"intrinsics\"]");
  nrobj_t* obj;
  nr_span_event_t* evt_root;
  nr_span_event_t* evt_b;

  // clang-format off
  nr_segment_t root = {.txn = &txn, .start_time = 0, .stop_time = 9000};
  nr_segment_t A = {.txn = &txn, .start_time = 1000, .stop_time = 2000};
  nr_segment_t B = {.txn = &txn, .start_time = 3000, .stop_time = 4000};
  // clang-format on

  metadata.out = &result;
  metadata.trace_set = nr_set_create();
  nr_set_insert(metadata.trace_set, (void*)&root);
  nr_set_insert(metadata.trace_set, (void*)&A);
  metadata.span_set = nr_set_create();
  nr_set_insert(metadata.span_set, (void*)&root);
  nr_set_insert(metadata.span_set, (void*)&B);

  /* Mock up a transaction */
  mock_txn(&txn, &root);
  txn.segment_count = 3;
  txn.name = nr_strdup("WebTransaction/*");

  /* Mock up a tree of segments */
  /* Create a collection of mock segments */

  /*    -----+*root-------
   *      --*A-- --+B--
   *
   *  Key:
   *  Sampled trace - *
   *  Sampled spans - +
   */

  nr_segment_children_init(&root.children);
  nr_segment_add_child(&root, &A);
  nr_segment_add_child(&root, &B);

  root.name = nr_string_add(txn.trace_strings, "WebTransaction/*");
  A.name = nr_string_add(txn.trace_strings, "A");
  B.name = nr_string_add(txn.trace_strings, "B");

  /*
   * Test : Normal operation
   */
  nr_segment_traces_create_data(&txn, 2 * NR_TIME_DIVISOR, &metadata,
                                agent_attributes, user_attributes, intrinsics,
                                true, true);

  tlib_pass_if_str_equal(
      "A transaction with sampling must succeed in creating a trace",
      metadata.out->trace_json,
      "[[0,{},{},[0,2000,\"ROOT\",{},[[0,9,\"`0\",{},[[1,2,"
      "\"`1\",{},[]]]]]],"
      "{\"agentAttributes\":[\"agent_attributes\"],"
      "\"userAttributes\":[\"user_attributes\"],"
      "\"intrinsics\":[\"intrinsics\"]}],"
      "[\"WebTransaction\\/*\",\"A\"]]");

  obj = nro_create_from_json(metadata.out->trace_json);
  tlib_pass_if_not_null(
      "A transaction with sampling must succeed in creating valid json", obj);

  tlib_pass_if_uint_equal("span event size",
                          nr_vector_size(metadata.out->span_events), 2);

  evt_root = (nr_span_event_t*)nr_vector_get(metadata.out->span_events, 0);
  SPAN_EVENT_COMPARE(evt_root, "WebTransaction/*", NR_SPAN_GENERIC, NULL, 1000,
                     9000);
  evt_b = (nr_span_event_t*)nr_vector_get(metadata.out->span_events, 1);
  SPAN_EVENT_COMPARE(evt_b, "B", NR_SPAN_GENERIC, evt_root, 4000, 1000);

  /* Clean up */
  nro_delete(obj);
  nr_free(metadata.out->trace_json);
  nr_vector_destroy(&metadata.out->span_events);
  nr_free(txn.name);

  nr_segment_children_deinit(&root.children);
  nr_segment_destroy_fields(&root);
  nr_segment_destroy_fields(&A);
  nr_segment_destroy_fields(&B);

  cleanup_mock_txn(&txn);

  nro_delete(agent_attributes);
  nro_delete(user_attributes);
  nro_delete(intrinsics);
  nr_set_destroy(&metadata.span_set);
  nr_set_destroy(&metadata.trace_set);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_json_print_bad_parameters();
  test_json_print_segments_root_only();
  test_json_print_segments_bad_segments();

  test_json_print_segment_with_data();
  test_json_print_segments_two_nodes();
  test_json_print_segments_hanoi();
  test_json_print_segments_three_siblings();
  test_json_print_segments_two_generations();
  test_json_print_segments_datastore_external_message();
  test_json_print_segments_datastore_params();
  test_json_print_segments_external_async_user_attrs();
  test_json_print_segments_message_attributes();

  test_json_print_segments_async_basic();
  test_json_print_segments_async_multi_child();
  test_json_print_segments_async_multi_context();
  test_json_print_segments_async_context_nesting();
  test_json_print_segments_async_with_data();

  test_json_print_segments_with_sampling();
  test_json_print_segments_with_sampling_cousin_parent();
  test_json_print_segments_with_sampling_inner_loop();
  test_json_print_segments_with_sampling_genghis_khan();
  test_json_print_segments_invalid_typed_attributes();

  test_json_print_segments_extremely_short();

  test_trace_create_data_bad_parameters();
  test_trace_create_data();
  test_trace_create_data_with_sampling();

  test_trace_create_trace_spans();
}
