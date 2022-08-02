/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "nr_log_events.h"
#include "util_random.h"
#include "util_strings.h"

#include "tlib_main.h"

#define LOG_LEVEL "INFO"
#define LOG_MESSAGE "this is a test log error message"
#define LOG_MESSAGE_0 LOG_MESSAGE " 0"
#define LOG_MESSAGE_1 LOG_MESSAGE " 1"
#define LOG_MESSAGE_2 LOG_MESSAGE " 2"
#define LOG_MESSAGE_3 LOG_MESSAGE " 3"
#define LOG_TIMESTAMP 12345

static nr_log_event_t* create_sample_event(const char* message) {
  nr_log_event_t* e = nr_log_event_create();
  nr_log_event_set_log_level(e, LOG_LEVEL);
  nr_log_event_set_message(e, message);
  nr_log_event_set_timestamp(e, LOG_TIMESTAMP * NR_TIME_DIVISOR_MS);
  return e;
}

static void test_events_success(void) {
  /* Normal operation */
  nr_log_events_t* events = NULL;
  nr_log_event_t* e = NULL;
  nr_random_t* rnd = nr_random_create_from_seed(12345);
  char* json;
  char* expected;

  events = nr_log_events_create(10);
  tlib_fail_if_null("events created", events);

  e = create_sample_event(LOG_MESSAGE_0);
  nr_log_events_add_event(events, e, rnd);
  // nr_log_event_destroy(&e);

  e = create_sample_event(LOG_MESSAGE_1);
  nr_log_events_add_event(events, e, rnd);
  // nr_log_event_destroy(&e);

  e = create_sample_event(LOG_MESSAGE_2);
  nr_log_events_add_event(events, e, rnd);
  // nr_log_event_destroy(&e);

  e = create_sample_event(LOG_MESSAGE_3);
  nr_log_events_add_event(events, e, rnd);
  // nr_log_event_destroy(&e);

  tlib_pass_if_int_equal("events number seen updated", 4,
                         nr_log_events_number_seen(events));
  tlib_pass_if_int_equal("events number saved updated", 4,
                         nr_log_events_number_saved(events));

#define TEST_EVENTS_GET(MESSAGE, IDX)                    \
  do {                                                   \
    json = nr_log_events_get_event_json(events, IDX);    \
    tlib_fail_if_null("no json", json);                  \
    expected = "[{" \
                 "\"message\":\"" MESSAGE "\"," \
                 "\"log.level\":\"" LOG_LEVEL "\"," \
                 "\"timestamp\":" NR_STR2(LOG_TIMESTAMP) "," \
                 "\"trace.id\":\"null\"," \
                 "\"span.id\":\"null\"," \
                 "\"entity.guid\":\"null\"," \
                 "\"entity.name\":\"null\"," \
                 "\"hostname\":\"null\"" \
              "}]";                                    \
    tlib_pass_if_str_equal("add event", expected, json); \
    nr_free(json);                                       \
  } while (0)

  /* Test events are stored in order */
  TEST_EVENTS_GET(LOG_MESSAGE_0, 0);
  TEST_EVENTS_GET(LOG_MESSAGE_3, 3);

  /* Getting events should not remove them from the pool */
  tlib_pass_if_int_equal("events number saved preserved", 4,
                         nr_log_events_number_saved(events));

  /* Getting event out of bounds should not crash */
  json = nr_log_events_get_event_json(events, -1);
  tlib_pass_if_null("event from out of bounds (lower)", json);

  json = nr_log_events_get_event_json(events,
                                      nr_log_events_number_saved(events));
  tlib_pass_if_null("event from out of bounds (higher)", json);

  nr_random_destroy(&rnd);
  nr_log_events_destroy(&events);
  tlib_pass_if_null("events destroyed", events);
}

static void test_events_sample(void) {
  /* Adding excessive number of events should cause sampling */
  nr_log_events_t* events = NULL;
  nr_log_event_t* e = NULL;
  nr_random_t* rnd = nr_random_create_from_seed(12345);

  events = nr_log_events_create(2);
  tlib_fail_if_null("events created", events);

  e = create_sample_event(LOG_MESSAGE);
  nr_log_events_add_event(events, e, rnd);
  // nr_log_event_destroy(&e);

  e = create_sample_event(LOG_MESSAGE);
  nr_log_events_add_event(events, e, rnd);
  // nr_log_event_destroy(&e);

  e = create_sample_event(LOG_MESSAGE);
  nr_log_events_add_event(events, e, rnd);
  // nr_log_event_destroy(&e);

  e = create_sample_event(LOG_MESSAGE);
  nr_log_events_add_event(events, e, rnd);
  // nr_log_event_destroy(&e);

  tlib_pass_if_int_equal("events number seen updated", 4,
                         nr_log_events_number_seen(events));
  tlib_pass_if_int_equal("events number saved updated", 2,
                         nr_log_events_number_saved(events));

  nr_random_destroy(&rnd);
  nr_log_events_destroy(&events);
  tlib_pass_if_null("events destroyed", events);
}

static void test_events_null(void) {
  /* Working with null events pool should not cause crash */
  nr_log_events_t* events = NULL;
  nr_log_event_t* e = NULL;
  char* json = NULL;
  nr_random_t* rnd = nr_random_create_from_seed(12345);

  /* Adding to uninitialized events */
  e = create_sample_event(LOG_MESSAGE);
  nr_log_events_add_event(events, e, rnd);
  // nr_log_event_destroy(&e);

  tlib_pass_if_int_equal("events number seen updated", 0,
                         nr_log_events_number_seen(events));
  tlib_pass_if_int_equal("events number saved updated", 0,
                         nr_log_events_number_saved(events));

  /* Retrieving from uninitialized events */
  json = nr_log_events_get_event_json(events, 0);
  tlib_pass_if_null("event from null events", json);

  nr_random_destroy(&rnd);
  nr_log_events_destroy(&events);
  tlib_pass_if_null("events destroyed", events);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_events_success();
  test_events_sample();
  test_events_null();
}