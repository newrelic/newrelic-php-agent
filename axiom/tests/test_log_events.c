/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"
#include <stddef.h>

#include "nr_log_events.h"
#include "nr_log_event_private.h"
#include "util_random.h"
#include "util_strings.h"
#include "util_vector.h"

#include "tlib_main.h"

#define LOG_LEVEL "INFO"
#define LOG_MESSAGE "this is a test log error message"
#define LOG_MESSAGE_0 LOG_MESSAGE " 0"
#define LOG_MESSAGE_1 LOG_MESSAGE " 1"
#define LOG_MESSAGE_2 LOG_MESSAGE " 2"
#define LOG_MESSAGE_3 LOG_MESSAGE " 3"
#define LOG_MESSAGE_4 LOG_MESSAGE " 4"
#define LOG_TIMESTAMP 12345

static nr_log_event_t* create_sample_event(const char* message) {
  nr_log_event_t* e = nr_log_event_create();
  nr_log_event_set_log_level(e, LOG_LEVEL);
  nr_log_event_set_message(e, message);
  nr_log_event_set_timestamp(e, LOG_TIMESTAMP * NR_TIME_DIVISOR_MS);
  nr_log_event_set_priority(e, 0);
  return e;
}

static void test_events_success(void) {
  /* Normal operation */
  nr_log_events_t* events = NULL;
  nr_log_event_t* e = NULL;
  void* test_e;
  nr_vector_t* vector = NULL;
  bool pass;
  char* json;
  const char* expected;

  events = nr_log_events_create(10);
  tlib_fail_if_null("events created", events);

  e = create_sample_event(LOG_MESSAGE_0);
  nr_log_event_set_priority(e, 3);
  nr_log_events_add_event(events, e);
  nr_log_event_destroy(&e);

  e = create_sample_event(LOG_MESSAGE_1);
  nr_log_event_set_priority(e, 2);
  nr_log_events_add_event(events, e);
  nr_log_event_destroy(&e);

  e = create_sample_event(LOG_MESSAGE_2);
  nr_log_event_set_priority(e, 1);
  nr_log_events_add_event(events, e);
  nr_log_event_destroy(&e);

  e = create_sample_event(LOG_MESSAGE_3);
  nr_log_event_set_priority(e, 0);
  nr_log_events_add_event(events, e);
  nr_log_event_destroy(&e);

  // minmax heap will store events like this (showing priorities)
  //         0
  //       3   2
  //      1
  //
  // so when walking the heap and converting to a vector we expect
  // the following order:
  // LOG_MESSAGE_3 (pri 0)
  // LOG_MESSAGE_0 (pri 3)
  // LOG_MESSAGE_1 (pri 2)
  // LOG_MESSAGE_2 (pri 1)

  tlib_pass_if_int_equal("events number seen updated", 4,
                         nr_log_events_number_seen(events));
  tlib_pass_if_int_equal("events number saved updated", 4,
                         nr_log_events_number_saved(events));

  // convert log event pool to a vector and test
  vector = nr_vector_create(10, NULL, NULL);
  nr_log_events_to_vector(events, vector);

#define TEST_EVENTS_GET(MESSAGE, IDX)                              \
  do {                                                             \
    pass = nr_vector_get_element(vector, IDX, &test_e);            \
    tlib_pass_if_true("retrived log element from vector OK", pass, \
                      "expected TRUE");                            \
    json = nr_log_event_to_json((nr_log_event_t*)test_e);          \
    tlib_fail_if_null("no json", json);                            \
    expected = "[{"\
                     "\"message\":\"" MESSAGE "\"," \
                 "\"level\":\"" LOG_LEVEL "\"," \
                 "\"timestamp\":" NR_STR2(LOG_TIMESTAMP) "," \
                 "\"trace.id\":\"null\"," \
                 "\"span.id\":\"null\"," \
                 "\"entity.guid\":\"null\"," \
                 "\"entity.name\":\"null\"," \
                 "\"hostname\":\"null\"" \
              "}]";                                               \
    tlib_pass_if_str_equal("add event", expected, json);           \
    nr_free(json);                                                 \
  } while (0)

  /* Test events are stored in order as explained above for minmax heap*/
  TEST_EVENTS_GET(LOG_MESSAGE_3, 0);
  TEST_EVENTS_GET(LOG_MESSAGE_0, 1);
  TEST_EVENTS_GET(LOG_MESSAGE_1, 2);
  TEST_EVENTS_GET(LOG_MESSAGE_2, 3);

  nr_vector_destroy(&vector);

  /* Getting events should not remove them from the pool */
  tlib_pass_if_int_equal("events number saved preserved", 4,
                         nr_log_events_number_saved(events));

  nr_log_events_destroy(&events);
  tlib_pass_if_null("events destroyed", events);
}

static void test_events_sample(void) {
  /* Adding excessive number of events should cause sampling */
  nr_log_events_t* events = NULL;
  nr_log_event_t* e = NULL;
  bool event_dropped;
  bool pass;
  void* test_e;
  nr_vector_t* vector;

  events = nr_log_events_create(2);
  tlib_fail_if_null("events created", events);

  e = create_sample_event(LOG_MESSAGE_0);
  nr_log_event_set_priority(e, 100);
  event_dropped = nr_log_events_add_event(events, e);
  nr_log_event_destroy(&e);
  tlib_pass_if_false("1st event not dropped", event_dropped,
                     "nr_log_events_add_event: got [%d], want [%d]",
                     event_dropped, false);

  e = create_sample_event(LOG_MESSAGE_1);
  nr_log_event_set_priority(e, 1);
  event_dropped = nr_log_events_add_event(events, e);
  nr_log_event_destroy(&e);
  tlib_pass_if_false("2nd event not dropped", event_dropped,
                     "nr_log_events_add_event: got [%d], want [%d]",
                     event_dropped, false);

  e = create_sample_event(LOG_MESSAGE_2);
  nr_log_event_set_priority(e, 2);
  event_dropped = nr_log_events_add_event(events, e);
  nr_log_event_destroy(&e);
  tlib_pass_if_true("3nd event, sampling", event_dropped,
                    "nr_log_events_add_event: got [%d], want [%d]",
                    event_dropped, true);

  e = create_sample_event(LOG_MESSAGE_3);
  nr_log_event_set_priority(e, 50);
  event_dropped = nr_log_events_add_event(events, e);
  nr_log_event_destroy(&e);
  tlib_pass_if_true("4th event, sampling", event_dropped,
                    "nr_log_events_add_event: got [%d], want [%d]",
                    event_dropped, true);

  tlib_pass_if_int_equal("events number seen updated", 4,
                         nr_log_events_number_seen(events));
  tlib_pass_if_int_equal("events number saved updated", 2,
                         nr_log_events_number_saved(events));

  // verify two highest priority events were retained
  // convert log event pool to a vector and test
  vector = nr_vector_create(10, NULL, NULL);
  nr_log_events_to_vector(events, vector);
  pass = nr_vector_get_element(vector, 0, &test_e);
  tlib_pass_if_true("retrived log element from vector OK", pass,
                    "expected TRUE");
  tlib_pass_if_str_equal("LOG_MESSAGE_3 log event retained", LOG_MESSAGE_3,
                         ((nr_log_event_t*)test_e)->message);
  tlib_pass_if_int_equal("Priority 50 log event retained", 50,
                         ((nr_log_event_t*)test_e)->priority);
  pass = nr_vector_get_element(vector, 1, &test_e);
  tlib_pass_if_true("retrived log element from vector OK", pass,
                    "expected TRUE");
  tlib_pass_if_str_equal("LOG_MESSAGE_0 log event retained", LOG_MESSAGE_0,
                         ((nr_log_event_t*)test_e)->message);
  tlib_pass_if_int_equal("Priority 100 log event retained", 100,
                         ((nr_log_event_t*)test_e)->priority);

  nr_vector_destroy(&vector);

  // Test: Add another priority 50 log event and it should remove oldest
  e = create_sample_event(LOG_MESSAGE_4);
  nr_log_event_set_priority(e, 50);
  nr_log_event_set_timestamp(e, (LOG_TIMESTAMP + 100) * NR_TIME_DIVISOR_MS);
  event_dropped = nr_log_events_add_event(events, e);
  nr_log_event_destroy(&e);
  tlib_pass_if_true("final event caused drop", event_dropped,
                    "nr_log_events_add_event: got [%d], want [%d]",
                    event_dropped, true);

  // verify two highest priority events were retained
  // convert log event pool to a vector and test
  vector = nr_vector_create(10, NULL, NULL);
  nr_log_events_to_vector(events, vector);
  pass = nr_vector_get_element(vector, 0, &test_e);
  tlib_pass_if_true("retrived log element from vector OK", pass,
                    "expected TRUE");
  tlib_pass_if_str_equal("LOG_MESSAGE_4 log event retained", LOG_MESSAGE_4,
                         ((nr_log_event_t*)test_e)->message);
  tlib_pass_if_int_equal("Priority 50 log event retained", 50,
                         ((nr_log_event_t*)test_e)->priority);
  pass = nr_vector_get_element(vector, 1, &test_e);
  tlib_pass_if_true("retrived log element from vector OK", pass,
                    "expected TRUE");
  tlib_pass_if_str_equal("LOG_MESSAGE_0 log event retained", LOG_MESSAGE_0,
                         ((nr_log_event_t*)test_e)->message);
  tlib_pass_if_int_equal("Priority 100 log event retained", 100,
                         ((nr_log_event_t*)test_e)->priority);

  nr_vector_destroy(&vector);

  nr_log_events_destroy(&events);
  tlib_pass_if_null("events destroyed", events);
}

static void test_log_event_comparator(void) {
  nr_log_event_t log0 = {.message = "LOG0", .priority = 30, .timestamp = 0};
  nr_log_event_t log1 = {.message = "LOG1", .priority = 20, .timestamp = 0};
  nr_log_event_t log2 = {.message = "LOG2", .priority = 10, .timestamp = 0};
  nr_log_event_t log3 = {.message = "LOG3", .priority = 0, .timestamp = 0};
  nr_log_event_t log4 = {.message = "LOG4", .priority = 0, .timestamp = 100};

  nr_vector_t* events = nr_vector_create(12, NULL, NULL);

  /*
   * The comparator function is tested by using it to sort a vector of
   * log events. In this way, all necessary test cases are covered.
   *
   * The comparator first compares a log events's priority, which is a bit
   * field with bits set according to NR_SEGEMENT_PRIORITY_* flags. The
   * priority with the higher numerical value is considered higher.
   *
   * If the priorities of two events are the same, the comparator
   * compares the events age duration. The younger event is considered
   * higher.
   *
   * The table below shows the final ordering and the respective values
   * that are considered by the comparator function.
   *
   * Position | Message    | Priority  | Timestamp
   * ---------+------------+-----------+----------
   * 4        | "LOG0"     | 30        | 0
   * 3        | "LOG1"     | 20        | 0
   * 2        | "LOG2"     | 10        | 0
   * 1        | "LOG4"     | 0         | 100
   * 0        | "LOG3"     | 0         | 0
   */

  nr_vector_push_back(events, &log0);
  nr_vector_push_back(events, &log4);
  nr_vector_push_back(events, &log1);
  nr_vector_push_back(events, &log2);
  nr_vector_push_back(events, &log3);

  nr_vector_sort(events, nr_log_event_wrapped_priority_comparator, NULL);

  tlib_pass_if_ptr_equal("1. log 0", nr_vector_get(events, 4), &log0);
  tlib_pass_if_ptr_equal("2. log 1", nr_vector_get(events, 3), &log1);
  tlib_pass_if_ptr_equal("3. log 2", nr_vector_get(events, 2), &log2);
  tlib_pass_if_ptr_equal("4. log 4", nr_vector_get(events, 1), &log4);
  tlib_pass_if_ptr_equal("5. log 3", nr_vector_get(events, 0), &log3);

  nr_vector_destroy(&events);
}

static void test_events_null(void) {
  /* Working with null events pool should not cause crash */
  nr_log_events_t* events = NULL;
  nr_log_event_t* e = NULL;
  // const char* json = NULL;

  /* Adding to uninitialized events */
  e = create_sample_event(LOG_MESSAGE);
  nr_log_events_add_event(events, e);
  nr_log_event_destroy(&e);

  tlib_pass_if_int_equal("events number seen updated", 0,
                         nr_log_events_number_seen(events));
  tlib_pass_if_int_equal("events number saved updated", 0,
                         nr_log_events_number_saved(events));

  /* Retrieving from uninitialized events */
#if 0   // MSF
  json = nr_log_events_get_event_json(events, 0);
  tlib_pass_if_null("event from null events", json);
#endif  // MSF
  nr_log_events_destroy(&events);
  tlib_pass_if_null("events destroyed", events);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_events_success();
  test_events_sample();
  test_events_null();
  test_log_event_comparator();
}