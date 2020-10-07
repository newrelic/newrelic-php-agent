/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "nr_analytics_events.h"
#include "nr_analytics_events_private.h"
#include "util_random.h"
#include "util_strings.h"

#include "tlib_main.h"

static void add_event_from_json(nr_analytics_events_t* events,
                                const char* event_json,
                                nr_random_t* rrnd) {
  nr_analytics_event_t* event
      = nr_analytics_event_create_from_string(event_json);

  nr_analytics_events_add_event(events, event, rrnd);
  nr_analytics_event_destroy(&event);
}

#define test_json_is_valid(...) \
  test_json_is_valid_fn(__VA_ARGS__, __FILE__, __LINE__)

static void test_json_is_valid_fn(const char* testname,
                                  const char* json,
                                  const char* file,
                                  int line) {
  nrobj_t* obj;

  obj = nro_create_from_json(json);
  test_pass_if_true(testname, 0 != obj, "obj=%p", obj);
  nro_delete(obj);
}

static nr_analytics_event_t* create_sample_event(void) {
  nr_analytics_event_t* event;
  nrobj_t* builtin_fields = nro_new_hash();
  nrobj_t* user_attributes = nro_new_hash();
  nrobj_t* agent_attributes = nro_new_hash();

  nro_set_hash_string(builtin_fields, "type", "Transaction");
  nro_set_hash_string(builtin_fields, "name", "escape/me");
  nro_set_hash_double(builtin_fields, "timestamp", 123456.789000);
  nro_set_hash_double(builtin_fields, "duration", 0.111000);
  nro_set_hash_double(builtin_fields, "webDuration", 0.111000);
  nro_set_hash_double(builtin_fields, "queueDuration", 0.222000);
  nro_set_hash_double(builtin_fields, "externalDuration", 0.333000);
  nro_set_hash_double(builtin_fields, "databaseDuration", 0.444000);
  nro_set_hash_double(builtin_fields, "memcacheDuration", 0.555000);

  nro_set_hash_string(user_attributes, "alpha", "beta");
  nro_set_hash_double(user_attributes, "gamma", 123.456);

  nro_set_hash_long(agent_attributes, "agent_long", 1);

  event = nr_analytics_event_create(builtin_fields, agent_attributes,
                                    user_attributes);

  nro_delete(builtin_fields);
  nro_delete(user_attributes);
  nro_delete(agent_attributes);

  return event;
}

static void test_event_create(void) {
  nr_analytics_event_t* event;
  nrobj_t* builtin_fields = nro_new_hash();
  nrobj_t* user_attributes = nro_new_hash();
  nrobj_t* agent_attributes = nro_new_hash();
  nrobj_t* empty_hash = nro_new_hash();

  nro_set_hash_string(builtin_fields, "type", "Transaction");
  nro_set_hash_string(builtin_fields, "name", "escape/me");
  nro_set_hash_double(builtin_fields, "timestamp", 123456.789000);
  nro_set_hash_double(builtin_fields, "duration", 0.111000);
  nro_set_hash_double(builtin_fields, "webDuration", 0.111000);
  nro_set_hash_double(builtin_fields, "queueDuration", 0.222000);
  nro_set_hash_double(builtin_fields, "externalDuration", 0.333000);
  nro_set_hash_double(builtin_fields, "databaseDuration", 0.444000);
  nro_set_hash_double(builtin_fields, "memcacheDuration", 0.555000);

  nro_set_hash_string(user_attributes, "alpha", "beta");
  nro_set_hash_double(user_attributes, "gamma", 123.456);

  nro_set_hash_long(agent_attributes, "agent_long", 1);

  event = nr_analytics_event_create(builtin_fields, agent_attributes,
                                    user_attributes);
  tlib_pass_if_true("event created",
                    0
                        == nr_strcmp((const char*)event,
                                     "["
                                     "{"
                                     "\"type\":\"Transaction\","
                                     "\"name\":\"escape\\/me\","
                                     "\"timestamp\":123456.78900,"
                                     "\"duration\":0.11100,"
                                     "\"webDuration\":0.11100,"
                                     "\"queueDuration\":0.22200,"
                                     "\"externalDuration\":0.33300,"
                                     "\"databaseDuration\":0.44400,"
                                     "\"memcacheDuration\":0.55500"
                                     "},"
                                     "{"
                                     "\"alpha\":\"beta\","
                                     "\"gamma\":123.45600"
                                     "},"
                                     "{"
                                     "\"agent_long\":1"
                                     "}"
                                     "]"),
                    "event=%s", (char*)event);
  nr_analytics_event_destroy(&event);

  event = nr_analytics_event_create(empty_hash, empty_hash, empty_hash);
  tlib_pass_if_true("empty attributes",
                    0
                        == nr_strcmp((const char*)event,
                                     "["
                                     "{},"
                                     "{},"
                                     "{}"
                                     "]"),
                    "event=%s", (char*)event);
  nr_analytics_event_destroy(&event);

  event = nr_analytics_event_create(0, 0, 0);
  tlib_pass_if_true("null attributes",
                    0
                        == nr_strcmp((const char*)event,
                                     "["
                                     "{},"
                                     "{},"
                                     "{}"
                                     "]"),
                    "event=%s", (char*)event);
  nr_analytics_event_destroy(&event);

  nro_delete(empty_hash);
  nro_delete(builtin_fields);
  nro_delete(user_attributes);
  nro_delete(agent_attributes);
}

static void test_event_create_bad_params(void) {
  nr_analytics_event_t* event;
  nrobj_t* builtin_fields = nro_new_hash();
  nrobj_t* user_attributes = nro_new_hash();
  nrobj_t* agent_attributes = nro_new_hash();
  nrobj_t* not_hash = nro_new_int(55);

  nro_set_hash_string(builtin_fields, "type", "Transaction");
  nro_set_hash_string(builtin_fields, "name", "escape/me");

  nro_set_hash_string(user_attributes, "alpha", "beta");
  nro_set_hash_double(user_attributes, "gamma", 123.456);

  event
      = nr_analytics_event_create(not_hash, user_attributes, agent_attributes);
  tlib_pass_if_true("builtins not hash", 0 == event, "event=%p", event);

  event = nr_analytics_event_create(builtin_fields, not_hash, agent_attributes);
  tlib_pass_if_true("user attributes not hash", 0 == event, "event=%p", event);

  event = nr_analytics_event_create(builtin_fields, user_attributes, not_hash);
  tlib_pass_if_true("agent attributes not hash", 0 == event, "event=%p", event);
  nr_analytics_event_destroy(&event);

  nro_delete(builtin_fields);
  nro_delete(user_attributes);
  nro_delete(agent_attributes);
  nro_delete(not_hash);
}

static void test_event_destroy(void) {
  nr_analytics_event_t* event;

  /* Don't blow up */
  nr_analytics_event_destroy(0);
  event = 0;
  nr_analytics_event_destroy(&event);

  event = create_sample_event();
  tlib_pass_if_true("tests valid", 0 != event, "event=%p", event);
  nr_analytics_event_destroy(&event);
  tlib_pass_if_true("event zeroed", 0 == event, "event=%p", event);
}

static void test_events_add_event_success(void) {
  nr_analytics_events_t* events;
  nr_analytics_event_t* event;
  const char* json;
  nr_random_t* rnd = nr_random_create_from_seed(12345);

  events = nr_analytics_events_create(10);
  event = create_sample_event();
  nr_analytics_events_add_event(events, event, rnd);
  json = nr_analytics_events_get_event_json(events, 0);
  test_json_is_valid("event added", json);
  tlib_pass_if_true("event added",
                    0
                        == nr_strcmp(json,
                                     "["
                                     "{"
                                     "\"type\":\"Transaction\","
                                     "\"name\":\"escape\\/me\","
                                     "\"timestamp\":123456.78900,"
                                     "\"duration\":0.11100,"
                                     "\"webDuration\":0.11100,"
                                     "\"queueDuration\":0.22200,"
                                     "\"externalDuration\":0.33300,"
                                     "\"databaseDuration\":0.44400,"
                                     "\"memcacheDuration\":0.55500"
                                     "},"
                                     "{"
                                     "\"alpha\":\"beta\","
                                     "\"gamma\":123.45600"
                                     "},"
                                     "{"
                                     "\"agent_long\":1"
                                     "}"
                                     "]"),
                    "json=%s", NRSAFESTR(json));
  nr_analytics_event_destroy(&event);
  nr_analytics_events_destroy(&events);

  events = nr_analytics_events_create(10);
  event = nr_analytics_event_create_from_string("[{},{}]");
  nr_analytics_events_add_event(events, event, rnd);
  json = nr_analytics_events_get_event_json(events, 0);
  test_json_is_valid("empty event added", json);
  tlib_pass_if_true("empty event added", 0 == nr_strcmp(json, "[{},{}]"),
                    "json=%s", NRSAFESTR(json));
  nr_analytics_event_destroy(&event);
  nr_analytics_events_destroy(&events);

  events = nr_analytics_events_create(10);
  event = nr_analytics_event_create_from_string("[{},{\"x\":123,\"y\":\"z\"}]");
  nr_analytics_events_add_event(events, event, rnd);
  json = nr_analytics_events_get_event_json(events, 0);
  test_json_is_valid("only user params", json);
  tlib_pass_if_true("only user params",
                    0 == nr_strcmp(json, "[{},{\"x\":123,\"y\":\"z\"}]"),
                    "json=%s", NRSAFESTR(json));
  nr_analytics_event_destroy(&event);
  nr_analytics_events_destroy(&events);

  nr_random_destroy(&rnd);
}

static void test_events_create_bad_param(void) {
  nr_analytics_events_t* events;

  events = nr_analytics_events_create(0);
  tlib_pass_if_true("zero max_events", 0 == events, "events=%p", events);

  events = nr_analytics_events_create(-1);
  tlib_pass_if_true("negative max_events", 0 == events, "events=%p", events);

  events = nr_analytics_events_create(100 * 1000 * 1000);
  tlib_pass_if_true("crazy large max_events", 0 == events, "events=%p", events);
}

static void test_events_add_event_failure(void) {
  nr_analytics_events_t* events;
  nr_analytics_event_t* event;
  nr_random_t* rnd = nr_random_create_from_seed(12345);

  /* NULL params, don't crash */
  event = create_sample_event();
  events = nr_analytics_events_create(10);
  nr_analytics_events_add_event(0, 0, rnd);
  nr_analytics_events_add_event(events, 0, rnd);
  nr_analytics_events_add_event(0, event, rnd);
  nr_analytics_events_destroy(&events);
  nr_analytics_event_destroy(&event);
  nr_random_destroy(&rnd);
}

static void test_max_observed(void) {
  int i;
  int max = 2;
  nr_analytics_events_t* events = nr_analytics_events_create(max);
  nr_random_t* rnd = nr_random_create_from_seed(12345);
  const char* event_json = "[{\"a\":1},{\"b\":2}]";

  for (i = 0; i < max + 1; i++) {
    add_event_from_json(events, event_json, rnd);
  }

  tlib_pass_if_int_equal("max observed", max,
                         nr_analytics_events_number_saved(events));
  tlib_pass_if_int_equal("max observed", max + 1,
                         nr_analytics_events_number_seen(events));

  for (i = 0; i < max; i++) {
    tlib_pass_if_str_equal("max observed", event_json,
                           nr_analytics_events_get_event_json(events, 0));
  }

  nr_analytics_events_destroy(&events);
  nr_random_destroy(&rnd);
}

static void test_reservoir_replacement(void) {
  int i;
  int max = 100;
  nr_analytics_events_t* events = nr_analytics_events_create(max);
  int count1;
  int count2;
  int seen;
  int saved;
  nr_random_t* rnd = nr_random_create_from_seed(12345);

  /*
   * This test is non-deterministic: there is some (low) probability that it
   * will fail.  First fill up the events with X.  Then add a large equal
   * number of X and Y.  Eventually expect roughly 50% of each.
   */

  for (i = 0; i < max; i++) {
    add_event_from_json(events, "[{\"X\":1},{}]", rnd);
    seen = nr_analytics_events_number_seen(events);
    saved = nr_analytics_events_number_saved(events);
    tlib_pass_if_true("number seen", i + 1 == seen, "i=%d seen=%d", i, seen);
    tlib_pass_if_true("number saved", i + 1 == saved, "i=%d saved=%d", i,
                      saved);
  }

  for (i = 0; i < 10 * max; i++) {
    add_event_from_json(events, "[{\"X\":1},{}]", rnd);
    add_event_from_json(events, "[{\"Y\":2},{}]", rnd);
    seen = nr_analytics_events_number_seen(events);
    saved = nr_analytics_events_number_saved(events);
    tlib_pass_if_true("number seen", max + (2 * (i + 1)) == seen,
                      "max=%d i=%d seen=%d", max, i, seen);
    tlib_pass_if_true("number saved", max == saved, "max=%d saved=%d", max,
                      saved);
  }

  count1 = 0;
  count2 = 0;
  for (i = 0; i < max; i++) {
    const char* json = nr_analytics_events_get_event_json(events, i);

    if (-1 != nr_stridx(json, "1")) {
      count1++;
    } else {
      count2++;
    }
  }
  tlib_pass_if_true("test is valid", max == count1 + count2,
                    "max=%d count1=%d count2=%d", max, count1, count2);
  tlib_pass_if_true("approx equal counts", count1 > (max / 4),
                    "max=%d count1=%d", max, count1);
  tlib_pass_if_true("approx equal counts", count2 > (max / 4),
                    "max=%d count2=%d", max, count2);

  nr_analytics_events_destroy(&events);
  nr_random_destroy(&rnd);
}

static void test_events_destroy_bad_params(void) {
  nr_analytics_events_t* null_events = 0;

  /* Don't blow up! */
  nr_analytics_events_destroy(0);
  nr_analytics_events_destroy(&null_events);
}

static void test_number_seen_bad_param(void) {
  tlib_pass_if_int_equal("null events", nr_analytics_events_number_seen(NULL),
                         0);
}

static void test_number_saved_bad_param(void) {
  tlib_pass_if_int_equal("null events", nr_analytics_events_number_saved(NULL),
                         0);
}

static void test_event_int_long(void) {
  nr_analytics_events_t* events = nr_analytics_events_create(10);
  const char* json;
  nr_random_t* rnd = nr_random_create_from_seed(12345);

  add_event_from_json(
      events, "[{\"my_int\":123,\"my_long\":9223372036854775807},{}]", rnd);

  json = nr_analytics_events_get_event_json(events, 0);

  test_json_is_valid("event added", json);
  tlib_pass_if_str_equal(
      "event added", json,
      "[{\"my_int\":123,\"my_long\":9223372036854775807},{}]");

  nr_analytics_events_destroy(&events);
  nr_random_destroy(&rnd);
}

static void test_analytics_events_get_event_json(void) {
  nr_analytics_events_t* events = nr_analytics_events_create(100);
  nr_random_t* rnd = nr_random_create_from_seed(12345);

  add_event_from_json(events, "[{\"a\":1},{\"b\":2}]", rnd);

  tlib_pass_if_null("null events", nr_analytics_events_get_event_json(NULL, 0));
  tlib_pass_if_null("negative index",
                    nr_analytics_events_get_event_json(events, -1));
  tlib_pass_if_null("high index",
                    nr_analytics_events_get_event_json(events, 1));

  tlib_pass_if_str_equal("success",
                         nr_analytics_events_get_event_json(events, 0),
                         "[{\"a\":1},{\"b\":2}]");

  nr_analytics_events_destroy(&events);
  nr_random_destroy(&rnd);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_event_create();
  test_event_create_bad_params();
  test_event_destroy();
  test_events_add_event_success();
  test_events_create_bad_param();
  test_events_add_event_failure();
  test_max_observed();
  test_reservoir_replacement();
  test_events_destroy_bad_params();
  test_number_seen_bad_param();
  test_number_saved_bad_param();
  test_event_int_long();
  test_analytics_events_get_event_json();
}
