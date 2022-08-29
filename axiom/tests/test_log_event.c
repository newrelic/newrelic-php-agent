/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_main.h"
#include "nr_log_event.h"
#include "nr_log_event_private.h"
#include "util_memory.h"

static void test_log_event_create_destroy(void) {
  // create a few instances to make sure state stays separate
  // and destroy them to make sure any *alloc-y bugs are
  // caught by valgrind
  nr_log_event_t* ev;
  nr_log_event_t* null_ev = NULL;

  ev = nr_log_event_create();

  tlib_pass_if_not_null("create log events ev1", ev);

  nr_log_event_destroy(&ev);
  nr_log_event_destroy(&null_ev);

  // Test: passing NULL pointer should not cause crash
  nr_log_event_destroy(NULL);
}

static void test_log_event_to_json(void) {
  char* json;
  nr_log_event_t* log;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL log event", nr_log_event_to_json(NULL));

  /*
   * Test : Empty log event.
   */
  log = nr_log_event_create();
  json = nr_log_event_to_json(log);
  tlib_pass_if_str_equal("empty log event",
                         "[{"
                         "\"message\":\"null\","
                         "\"level\":\"null\","
                         "\"timestamp\":0"
                         "}]",
                         json);
  nr_free(json);
  nr_log_event_destroy(&log);

  /*
   * Test : Full (ie every hash has at least one attribute) log event.
   */
  log = nr_log_event_create();
  nr_log_event_set_log_level(log, "LOG_LEVEL_TEST_ERROR");
  nr_log_event_set_message(log, "this is a test log error message");
  nr_log_event_set_timestamp(log, 12345000);
  nr_log_event_set_trace_id(log, "test id 1");
  nr_log_event_set_span_id(log, "test id 2");
  nr_log_event_set_guid(log, "test id 3");
  nr_log_event_set_entity_name(log, "entity name here");
  nr_log_event_set_hostname(log, "host name here");
  json = nr_log_event_to_json(log);
  tlib_pass_if_str_equal("populated log event",
                         "[{"
                         "\"message\":\"this is a test log error message\","
                         "\"level\":\"LOG_LEVEL_TEST_ERROR\","
                         "\"trace.id\":\"test id 1\","
                         "\"span.id\":\"test id 2\","
                         "\"entity.guid\":\"test id 3\","
                         "\"entity.name\":\"entity name here\","
                         "\"hostname\":\"host name here\","
                         "\"timestamp\":12345"
                         "}]",
                         json);
  nr_free(json);
  nr_log_event_destroy(&log);
}

static void test_log_event_to_json_buffer(void) {
  nrbuf_t* buf = nr_buffer_create(0, 0);
  nr_log_event_t* log;

  /*
   * Test : Bad parameters.
   */
  log = nr_log_event_create();
  tlib_pass_if_bool_equal("NULL buffer", false,
                          nr_log_event_to_json_buffer(log, NULL));
  nr_log_event_destroy(&log);

  tlib_pass_if_bool_equal("NULL log event", false,
                          nr_log_event_to_json_buffer(NULL, buf));
  tlib_pass_if_size_t_equal("buffer is untouched after a NULL log event", 0,
                            nr_buffer_len(buf));

  /*
   * Test : Empty log event.
   */
  log = nr_log_event_create();
  tlib_pass_if_bool_equal("empty log event", true,
                          nr_log_event_to_json_buffer(log, buf));
  nr_buffer_add(buf, NR_PSTR("\0"));
  tlib_pass_if_str_equal("empty log event",
                         "[{"
                         "\"message\":\"null\","
                         "\"level\":\"null\","
                         "\"timestamp\":0"
                         "}]",
                         nr_buffer_cptr(buf));
  nr_buffer_reset(buf);
  nr_log_event_destroy(&log);

  /*
   * Test : Full (ie every hash has at least one attribute) log event.
   */
  log = nr_log_event_create();
  nr_log_event_set_log_level(log, "LOG_LEVEL_TEST_ERROR");
  nr_log_event_set_message(log, "this is a test log error message");
  nr_log_event_set_timestamp(log, 12345000);
  nr_log_event_set_trace_id(log, "test id 1");
  nr_log_event_set_span_id(log, "test id 2");
  nr_log_event_set_guid(log, "test id 3");
  nr_log_event_set_entity_name(log, "entity name here");
  nr_log_event_set_hostname(log, "host name here");
  tlib_pass_if_bool_equal("full log event", true,
                          nr_log_event_to_json_buffer(log, buf));
  nr_buffer_add(buf, NR_PSTR("\0"));
  tlib_pass_if_str_equal("full log event",
                         "[{"
                         "\"message\":\"this is a test log error message\","
                         "\"level\":\"LOG_LEVEL_TEST_ERROR\","
                         "\"trace.id\":\"test id 1\","
                         "\"span.id\":\"test id 2\","
                         "\"entity.guid\":\"test id 3\","
                         "\"entity.name\":\"entity name here\","
                         "\"hostname\":\"host name here\","
                         "\"timestamp\":12345"
                         "}]",
                         nr_buffer_cptr(buf));
  nr_log_event_destroy(&log);

  nr_buffer_destroy(&buf);
}

static void test_log_event_to_json_buffer_ex(void) {
  nrbuf_t* buf = nr_buffer_create(0, 0);
  nr_log_event_t* log;

  /*
   * Test : Bad parameters.
   */
  log = nr_log_event_create();
  tlib_pass_if_bool_equal("NULL buffer", false,
                          nr_log_event_to_json_buffer_ex(log, NULL, false));
  nr_log_event_destroy(&log);

  tlib_pass_if_bool_equal("NULL log event", false,
                          nr_log_event_to_json_buffer_ex(NULL, buf, true));
  tlib_pass_if_size_t_equal("buffer is untouched after a NULL log event", 0,
                            nr_buffer_len(buf));

  /*
   * Test : Empty log event.
   */
  log = nr_log_event_create();
  tlib_pass_if_bool_equal("empty log event", true,
                          nr_log_event_to_json_buffer_ex(log, buf, false));
  nr_buffer_add(buf, NR_PSTR("\0"));
  tlib_pass_if_str_equal("empty log event",
                         "[{"
                         "\"message\":\"null\","
                         "\"level\":\"null\","
                         "\"timestamp\":0"
                         "}]",
                         nr_buffer_cptr(buf));
  nr_buffer_reset(buf);
  tlib_pass_if_bool_equal("empty log event", true,
                          nr_log_event_to_json_buffer_ex(log, buf, true));
  nr_buffer_add(buf, NR_PSTR("\0"));
  tlib_pass_if_str_equal("empty log event",
                         "{"
                         "\"message\":\"null\","
                         "\"level\":\"null\","
                         "\"timestamp\":0"
                         "}",
                         nr_buffer_cptr(buf));
  nr_buffer_reset(buf);
  nr_log_event_destroy(&log);

  /*
   * Test : Full (ie every hash has at least one attribute) log event.
   */
  log = nr_log_event_create();
  nr_log_event_set_log_level(log, "LOG_LEVEL_TEST_ERROR");
  nr_log_event_set_message(log, "this is a test log error message");
  nr_log_event_set_timestamp(log, 12345000);
  nr_log_event_set_trace_id(log, "test id 1");
  nr_log_event_set_span_id(log, "test id 2");
  nr_log_event_set_guid(log, "test id 3");
  nr_log_event_set_entity_name(log, "entity name here");
  nr_log_event_set_hostname(log, "host name here");
  tlib_pass_if_bool_equal("full log event", true,
                          nr_log_event_to_json_buffer_ex(log, buf, false));
  nr_buffer_add(buf, NR_PSTR("\0"));
  tlib_pass_if_str_equal("full log event",
                         "[{"
                         "\"message\":\"this is a test log error message\","
                         "\"level\":\"LOG_LEVEL_TEST_ERROR\","
                         "\"trace.id\":\"test id 1\","
                         "\"span.id\":\"test id 2\","
                         "\"entity.guid\":\"test id 3\","
                         "\"entity.name\":\"entity name here\","
                         "\"hostname\":\"host name here\","
                         "\"timestamp\":12345"
                         "}]",
                         nr_buffer_cptr(buf));
  nr_buffer_reset(buf);
  tlib_pass_if_bool_equal("full log event", true,
                          nr_log_event_to_json_buffer_ex(log, buf, true));
  nr_buffer_add(buf, NR_PSTR("\0"));
  tlib_pass_if_str_equal("full log event",
                         "{"
                         "\"message\":\"this is a test log error message\","
                         "\"level\":\"LOG_LEVEL_TEST_ERROR\","
                         "\"trace.id\":\"test id 1\","
                         "\"span.id\":\"test id 2\","
                         "\"entity.guid\":\"test id 3\","
                         "\"entity.name\":\"entity name here\","
                         "\"hostname\":\"host name here\","
                         "\"timestamp\":12345"
                         "}",
                         nr_buffer_cptr(buf));

  nr_log_event_destroy(&log);

  nr_buffer_destroy(&buf);
}

static void test_log_event_guid(void) {
  nr_log_event_t* event = nr_log_event_create();

  // Test : should not set a null guid
  nr_log_event_set_guid(event, NULL);
  tlib_pass_if_null("NULL guid", event->entity_guid);

  // Test : should set the guid to an empty string
  nr_log_event_set_guid(event, "");
  tlib_pass_if_str_equal("empty string guid", "", event->entity_guid);

  // Test : should set the guid
  nr_log_event_set_guid(event, "wombat");
  tlib_pass_if_str_equal("set the guid", "wombat", event->entity_guid);

  // Test : One more set
  nr_log_event_set_guid(event, "Kangaroo");
  tlib_pass_if_str_equal("set a new guid", "Kangaroo", event->entity_guid);

  nr_log_event_destroy(&event);
}

static void test_log_event_trace_id(void) {
  nr_log_event_t* event = nr_log_event_create();

  // Test : that is does not blow up when we give the setter a NULL pointer
  nr_log_event_set_trace_id(event, NULL);
  nr_log_event_set_trace_id(NULL, "wallaby");
  tlib_pass_if_null("the trace should still be NULL", event->trace_id);

  // Test : setting the trace id back and forth behaves as expected
  nr_log_event_set_trace_id(event, "Florance");
  tlib_pass_if_str_equal("should be the trace ID we set 1", "Florance",
                         event->trace_id);
  nr_log_event_set_trace_id(event, "Wallaby");
  tlib_pass_if_str_equal("should be the trace ID we set 2", "Wallaby",
                         event->trace_id);

  nr_log_event_destroy(&event);
}

static void test_log_event_entity_name(void) {
  nr_log_event_t* event = nr_log_event_create();

  // Test : that is does not blow up when we give the setter a NULL pointer
  nr_log_event_set_entity_name(event, NULL);
  nr_log_event_set_entity_name(NULL, "wallaby");
  tlib_pass_if_null("the entity_name should still be NULL", event->entity_name);

  // Test : setting the entity_name back and forth behaves as expected
  nr_log_event_set_entity_name(event, "Florance");
  tlib_pass_if_str_equal("should be the entity name we set 1", "Florance",
                         event->entity_name);
  nr_log_event_set_entity_name(event, "Wallaby");
  tlib_pass_if_str_equal("should be the entity name we set 2", "Wallaby",
                         event->entity_name);

  nr_log_event_destroy(&event);
}

static void test_log_event_message(void) {
  nr_log_event_t* event = nr_log_event_create();

  /*
   * Test : Bad parameters.
   */
  nr_log_event_set_message(event, NULL);
  tlib_pass_if_null("NULL message", event->message);
  nr_log_event_set_message(NULL, "test message");
  tlib_pass_if_null("NULL event", event->message);

  /*
   * Test : Valid message.
   */
  nr_log_event_set_message(event, "test message");
  tlib_pass_if_str_equal("Valid message set", "test message", event->message);
  nr_log_event_set_message(event, "another test message");
  tlib_pass_if_str_equal("Another valid message set", "another test message",
                         event->message);

  nr_log_event_destroy(&event);
}

static void test_log_event_log_level(void) {
  nr_log_event_t* event = nr_log_event_create();

  /*
   * Test : Bad parameters.
   */
  nr_log_event_set_log_level(event, NULL);
  tlib_pass_if_null("NULL log_level", event->log_level);
  nr_log_event_set_log_level(NULL, "test log_level");
  tlib_pass_if_null("NULL event", event->log_level);

  /*
   * Test : Valid log_level.
   */
  nr_log_event_set_log_level(event, "test log_level");
  tlib_pass_if_str_equal("Valid log_level set", "test log_level",
                         event->log_level);
  nr_log_event_set_log_level(event, "another test log_level");
  tlib_pass_if_str_equal("Another valid log_level set",
                         "another test log_level", event->log_level);

  nr_log_event_destroy(&event);
}

static void test_log_event_hostname(void) {
  nr_log_event_t* event = nr_log_event_create();

  /*
   * Test : Bad parameters.
   */
  nr_log_event_set_hostname(event, NULL);
  tlib_pass_if_null("NULL hostname", event->hostname);
  nr_log_event_set_hostname(NULL, "test hostname");
  tlib_pass_if_null("NULL event", event->hostname);

  /*
   * Test : Valid hostname.
   */
  nr_log_event_set_hostname(event, "test hostname");
  tlib_pass_if_str_equal("Valid hostname set", "test hostname",
                         event->hostname);
  nr_log_event_set_hostname(event, "another test hostname");
  tlib_pass_if_str_equal("Another valid hostname set", "another test hostname",
                         event->hostname);

  nr_log_event_destroy(&event);
}

static void test_log_event_timestamp(void) {
  nr_log_event_t* event = nr_log_event_create();

  // Test : Get timestamp with a NULL event
  tlib_pass_if_time_equal("NULL event should give zero", 0, event->timestamp);

  // Test : Set the timestamp a couple times
  nr_log_event_set_timestamp(event, 553483260);
  tlib_pass_if_time_equal("Get timestamp should equal 553483260",
                          553483260 / NR_TIME_DIVISOR_MS, event->timestamp);
  nr_log_event_set_timestamp(event, 853483260);
  tlib_pass_if_time_equal("Get timestamp should equal 853483260",
                          853483260 / NR_TIME_DIVISOR_MS, event->timestamp);

  nr_log_event_destroy(&event);
}

static void test_log_event_span_id(void) {
  nr_log_event_t* event = nr_log_event_create();

  // Test : that is does not blow up when we give the setter a NULL pointer
  nr_log_event_set_span_id(event, NULL);
  nr_log_event_set_span_id(NULL, "wallaby");
  tlib_pass_if_null("the span should still be NULL", event->span_id);

  // Test : setting the span id back and forth behaves as expected
  nr_log_event_set_span_id(event, "Florance");
  tlib_pass_if_str_equal("should be the span ID we set 1", "Florance",
                         event->span_id);
  nr_log_event_set_span_id(event, "Wallaby");
  tlib_pass_if_str_equal("should be the span ID we set 2", "Wallaby",
                         event->span_id);

  nr_log_event_destroy(&event);
}

static void test_log_event_priority(void) {
  nr_log_event_t* event = nr_log_event_create();

  // Test : Get priority with a NULL event
  tlib_pass_if_int_equal("NULL event should give zero", 0, event->priority);

  // Test : Set the priority a couple times
  nr_log_event_set_priority(event, 12345);
  tlib_pass_if_int_equal("Get priority should equal 12345", 12345,
                         event->priority);
  nr_log_event_set_priority(event, 0xFFFF);
  tlib_pass_if_time_equal("Get priority should equal 0xFFFF", 0xFFFF,
                          event->priority);

  nr_log_event_destroy(&event);

  // setting priority on NULL event ptr should not crash
  nr_log_event_set_priority(NULL, 0xFFFF);
}

static void test_log_event_clone(void) {
  nr_log_event_t* orig;
  nr_log_event_t* clone;

  // Test : Clone a NULL ptr should return NULL and not crash
  clone = nr_log_event_clone(NULL);
  tlib_pass_if_null("cloning a NULL log event ptr should return NULL", clone);

  // Test: Clone a event with NULL string members should copy as NULL and not
  // creash
  orig = nr_log_event_create();
  clone = nr_log_event_clone(orig);
  tlib_pass_if_null(
      "cloning a log event with NULL entity_guid should remain NULL",
      clone->entity_guid);
  tlib_pass_if_null(
      "cloning a log event with NULL entity_name should remain NULL",
      clone->entity_name);
  tlib_pass_if_null("cloning a log event with NULL hostname should remain NULL",
                    clone->hostname);
  tlib_pass_if_null(
      "cloning a log event with NULL log_level should remain NULL",
      clone->log_level);
  tlib_pass_if_null("cloning a log event with NULL message should remain NULL",
                    clone->message);
  tlib_pass_if_null("cloning a log event with NULL span_id should remain NULL",
                    clone->span_id);
  tlib_pass_if_null("cloning a log event with NULL trace_id should remain NULL",
                    clone->trace_id);
  tlib_pass_if_int_equal("cloning a log event with 0 priority should give 0", 0,
                         clone->priority);
  tlib_pass_if_int_equal("cloning a log event with 0 timestamp should give 0",
                         0, clone->timestamp);

  // Free original event first to test if freeing clone double frees or other
  // mem probs
  nr_log_event_destroy(&orig);
  nr_log_event_destroy(&clone);

  // Test: Clone an event with all string members present even after orig freed
  orig = nr_log_event_create();
  nr_log_event_set_entity_name(orig, "ENTITY_NAME");
  nr_log_event_set_guid(orig, "ENTITY_GUID");
  nr_log_event_set_hostname(orig, "HOSTNAME");
  nr_log_event_set_log_level(orig, "LOGLEVEL");
  nr_log_event_set_message(orig, "MESSAGE");
  nr_log_event_set_span_id(orig, "SPAN_ID");
  nr_log_event_set_trace_id(orig, "TRACE_ID");
  nr_log_event_set_timestamp(orig, 553483260);
  nr_log_event_set_priority(orig, 0x1234);
  clone = nr_log_event_clone(orig);
  nr_log_event_destroy(&orig);
  tlib_pass_if_str_equal(
      "cloning a log event should create correct entity_guid", "ENTITY_GUID",
      clone->entity_guid);
  tlib_pass_if_str_equal(
      "cloning a log event should create correct entity_name", "ENTITY_NAME",
      clone->entity_name);
  tlib_pass_if_str_equal("cloning a log event should create correct hostname",
                         "HOSTNAME", clone->hostname);
  tlib_pass_if_str_equal("cloning a log event should create correct log_level",
                         "LOGLEVEL", clone->log_level);
  tlib_pass_if_str_equal("cloning a log event should create correct message",
                         "MESSAGE", clone->message);
  tlib_pass_if_str_equal("cloning a log event should create correct span_id",
                         "SPAN_ID", clone->span_id);
  tlib_pass_if_str_equal("cloning a log event should create correct trace_id",
                         "TRACE_ID", clone->trace_id);
  tlib_pass_if_int_equal("cloning a log event should create correct priority",
                         0x1234, clone->priority);
  tlib_pass_if_int_equal("cloning a log event should create correct timestamp",
                         553483260 / NR_TIME_DIVISOR_MS, clone->timestamp);
  nr_log_event_destroy(&clone);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 1, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_log_event_create_destroy();
  test_log_event_to_json();
  test_log_event_to_json_buffer();
  test_log_event_to_json_buffer_ex();
  test_log_event_guid();
  test_log_event_trace_id();
  test_log_event_entity_name();
  test_log_event_log_level();
  test_log_event_message();
  test_log_event_hostname();
  test_log_event_timestamp();
  test_log_event_priority();
  test_log_event_span_id();
  test_log_event_clone();
}
