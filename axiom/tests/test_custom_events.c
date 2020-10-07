/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"
#include "nr_custom_events.h"
#include "util_strings.h"

#include "tlib_main.h"

static void test_custom_events_add_event(void) {
  nrtime_t now = 123 * NR_TIME_DIVISOR;
  const char* type = "my_event_type";
  nrobj_t* params;
  nr_analytics_events_t* custom_events;
  const char* json;
  nr_random_t* rnd = NULL;

  params = nro_create_from_json(
      "{"
      "\"exclude_me\":\"heyo\","
      "\"my_string\":\"zip\","
      "\"my_int\":123,"
      "\"my_long\":9223372036854775807,"
      "\"my_double\":44.55,"
      "\"improper_value\":[123]"
      "}");

  /*
   * Test: NULL params, don't blow up!
   */
  custom_events = nr_analytics_events_create(100);
  nr_custom_events_add_event(NULL, NULL, NULL, now, rnd);
  nr_custom_events_add_event(NULL, type, params, now, rnd);
  nr_custom_events_add_event(custom_events, NULL, params, now, rnd);
  nr_custom_events_add_event(custom_events, type, NULL, now, rnd);
  json = nr_analytics_events_get_event_json(custom_events, 0);
  tlib_pass_if_null("bad params", json);
  nr_analytics_events_destroy(&custom_events);

  custom_events = nr_analytics_events_create(100);
  nr_custom_events_add_event(custom_events, type, params, now, rnd);
  json = nr_analytics_events_get_event_json(custom_events, 0);
  tlib_pass_if_str_equal(
      "success", json,
      "["
      "{\"type\":\"my_event_type\",\"timestamp\":123.00000},"
      "{\"my_double\":44.55000,\"my_long\":9223372036854775807,"
      "\"my_int\":123,\"my_string\":\"zip\",\"exclude_me\":\"heyo\"},"
      "{}"
      "]");
  nr_analytics_events_destroy(&custom_events);
  nro_delete(params);
}

static void test_type_too_large(void) {
  nrtime_t now = 123 * NR_TIME_DIVISOR;
  const char* type
      = "012345678901234567890123456789012345678901234567890123456789"
        "012345678901234567890123456789012345678901234567890123456789"
        "012345678901234567890123456789012345678901234567890123456789"
        "012345678901234567890123456789012345678901234567890123456789"
        "012345678901234567890123456789012345678901234567890123456789"
        "012345678901234567890123456789012345678901234567890123456789"
        "012345678901234567890123456789012345678901234567890123456789";
  nrobj_t* params;
  nr_analytics_events_t* custom_events;
  const char* json;
  nr_random_t* rnd = NULL;

  params = nro_create_from_json(
      "{"
      "\"exclude_me\":\"heyo\","
      "\"my_string\":\"zip\","
      "\"my_int\":123,"
      "\"my_long\":9223372036854775807,"
      "\"my_double\":44.55,"
      "\"improper_value\":[123]"
      "}");

  custom_events = nr_analytics_events_create(100);
  nr_custom_events_add_event(custom_events, type, params, now, rnd);
  json = nr_analytics_events_get_event_json(custom_events, 0);
  tlib_pass_if_null("type name too long", json);
  nr_analytics_events_destroy(&custom_events);
  nro_delete(params);
}

static void test_type_invalid_characters(void) {
  nrtime_t now = 123 * NR_TIME_DIVISOR;
  nrobj_t* params;
  nr_analytics_events_t* custom_events;
  const char* json;
  nr_random_t* rnd = NULL;

  params = nro_create_from_json(
      "{"
      "\"exclude_me\":\"heyo\","
      "\"my_string\":\"zip\","
      "\"my_int\":123,"
      "\"my_long\":9223372036854775807,"
      "\"my_double\":44.55,"
      "\"improper_value\":[123]"
      "}");

  custom_events = nr_analytics_events_create(100);
  nr_custom_events_add_event(custom_events, "alpha!", params, now, rnd);
  nr_custom_events_add_event(custom_events, "", params, now, rnd);
  nr_custom_events_add_event(custom_events, "!alpha", params, now, rnd);
  nr_custom_events_add_event(custom_events, "!!!!!!", params, now, rnd);
  json = nr_analytics_events_get_event_json(custom_events, 0);
  tlib_pass_if_null("invalid type characters", json);
  nr_analytics_events_destroy(&custom_events);
  nro_delete(params);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 4, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_custom_events_add_event();
  test_type_too_large();
  test_type_invalid_characters();
}
