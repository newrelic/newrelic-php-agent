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
                         "\"log.level\":\"null\","
                         "\"timestamp\":0,"
                         "\"trace.id\":\"null\","
                         "\"span.id\":\"null\","
                         "\"entity.guid\":\"null\","
                         "\"entity.name\":\"null\","
                         "\"hostname\":\"null\""
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
  nr_log_event_set_timestamp(log, 12345);
  nr_log_event_set_trace_id(log, "test id 1");
  nr_log_event_set_span_id(log, "test id 2");
  nr_log_event_set_guid(log, "test id 3");
  nr_log_event_set_entity_name(log, "entity name here");
  nr_log_event_set_hostname(log, "host name here");
  json = nr_log_event_to_json(log);
  tlib_pass_if_str_equal("populated log event",
                         "[{"
                         "\"message\":\"this is a test log error message\","
                         "\"log.level\":\"LOG_LEVEL_TEST_ERROR\","
                         "\"timestamp\":12345,"
                         "\"trace.id\":\"test id 1\","
                         "\"span.id\":\"test id 2\","
                         "\"entity.guid\":\"test id 3\","
                         "\"entity.name\":\"entity name here\","
                         "\"hostname\":\"host name here\""
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
                         "\"log.level\":\"null\","
                         "\"timestamp\":0,"
                         "\"trace.id\":\"null\","
                         "\"span.id\":\"null\","
                         "\"entity.guid\":\"null\","
                         "\"entity.name\":\"null\","
                         "\"hostname\":\"null\""
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
  nr_log_event_set_timestamp(log, 12345);
  nr_log_event_set_trace_id(log, "test id 1");
  nr_log_event_set_span_id(log, "test id 2");
  nr_log_event_set_guid(log, "test id 3");
  nr_log_event_set_entity_name(log, "entity name here");
  nr_log_event_set_hostname(log, "host name here");
  tlib_pass_if_bool_equal("full log event", true,
                          nr_log_event_to_json_buffer(log, buf));
  nr_buffer_add(buf, NR_PSTR("\0"));
  tlib_pass_if_str_equal(
      "full log event",
      "[{"
      "\"message\":\"this is a test log error message\","
      "\"log.level\":\"LOG_LEVEL_TEST_ERROR\","
      "\"timestamp\":12345,"
      "\"trace.id\":\"test id 1\","
      "\"span.id\":\"test id 2\","
      "\"entity.guid\":\"test id 3\","
      "\"entity.name\":\"entity name here\","
      "\"hostname\":\"host name here\""
      "}]",
      nr_buffer_cptr(buf));
  nr_log_event_destroy(&log);

  nr_buffer_destroy(&buf);
}

//static void test_log_event_guid(void) {
//  nr_log_event_t* event = nr_log_event_create();
//
//  // Test : should not set a null guid
//  nr_log_event_set_guid(event, NULL);
//  tlib_pass_if_null("NULL guid", nr_log_event_get_guid(event));
//
//  // Test : should set the guid to an empty string
//  nr_log_event_set_guid(event, "");
//  tlib_pass_if_str_equal("empty string guid", "",
//                         nr_log_event_get_guid(event));
//
//  // Test : should set the guid
//  nr_log_event_set_guid(event, "wombat");
//  tlib_pass_if_str_equal("set the guid", "wombat",
//                         nr_log_event_get_guid(event));
//
//  // Test : One more set
//  nr_log_event_set_guid(event, "Kangaroo");
//  tlib_pass_if_str_equal("set a new guid", "Kangaroo",
//                         nr_log_event_get_guid(event));
//
//  nr_log_event_destroy(&event);
//}
//
//static void test_log_event_parent(void) {
//  nr_log_event_t* event = nr_log_event_create();
//
//  // Test : that it does not blow up when we give a NULL pointer
//  nr_log_event_set_parent_id(event, NULL);
//  nr_log_event_set_parent_id(NULL, "wombat");
//  tlib_pass_if_null("the parent should still be NULL",
//                    nr_log_event_get_parent_id(event));
//
//  // Test : the getter should return NULL when a NULL event is passed in
//  tlib_pass_if_null("NULL event -> NULL parent",
//                    nr_log_event_get_parent_id(NULL));
//
//  // Test : that the parent is set correctly.
//  nr_log_event_set_parent_id(event, "wombat");
//  tlib_pass_if_str_equal("the parent guid should be the one we set earlier",
//                         "wombat", nr_log_event_get_parent_id(event));
//
//  nr_log_event_destroy(&event);
//}
//
//static void test_log_event_transaction_id(void) {
//  nr_log_event_t* event = nr_log_event_create();
//
//  // Test : that is does not blow up when we give the setter a NULL pointer
//  nr_log_event_set_transaction_id(event, NULL);
//  nr_log_event_set_transaction_id(NULL, "wallaby");
//  tlib_pass_if_null("the transaction should still be NULL",
//                    nr_log_event_get_transaction_id(event));
//
//  // Test : the getter should not blow up when we send it an event with a NULL
//  // transactionID
//  tlib_pass_if_null("NULL event -> NULL transaction ID",
//                    nr_log_event_get_transaction_id(event));
//
//  // Test : setting the transaction id back and forth behaves as expected
//  nr_log_event_set_transaction_id(event, "Florance");
//  tlib_pass_if_str_equal("should be the transaction ID we set 1", "Florance",
//                         nr_log_event_get_transaction_id(event));
//  nr_log_event_set_transaction_id(event, "Wallaby");
//  tlib_pass_if_str_equal("should be the transaction ID we set 2", "Wallaby",
//                         nr_log_event_get_transaction_id(event));
//
//  nr_log_event_destroy(&event);
//}
//
//static void test_log_event_name(void) {
//  nr_log_event_t* event = nr_log_event_create();
//
//  // Test : that is does not blow up when we give the setter a NULL pointer
//  nr_log_event_set_name(event, NULL);
//  nr_log_event_set_name(NULL, "wallaby");
//  tlib_pass_if_null("the name should still be NULL",
//                    nr_log_event_get_name(event));
//
//  // Test : the getter should not blow up when we send it an event with a NULL
//  // name.
//  tlib_pass_if_null("NULL event -> NULL name", nr_log_event_get_name(event));
//
//  // Test : setting the name back and forth behaves as expected
//  nr_log_event_set_name(event, "Florance");
//  tlib_pass_if_str_equal("should be the name we set 1", "Florance",
//                         nr_log_event_get_name(event));
//  nr_log_event_set_name(event, "Wallaby");
//  tlib_pass_if_str_equal("should be the name we set 2", "Wallaby",
//                         nr_log_event_get_name(event));
//
//  nr_log_event_destroy(&event);
//}
//
//static void test_log_event_transaction_name(void) {
//  nr_log_event_t* event = nr_log_event_create();
//
//  /*
//   * Test : Bad parameters.
//   */
//  nr_log_event_set_transaction_name(event, NULL);
//  tlib_pass_if_null("NULL name", nr_log_event_get_transaction_name(event));
//  nr_log_event_set_transaction_name(NULL, "transaction.name");
//  tlib_pass_if_null("NULL event", nr_log_event_get_transaction_name(event));
//
//  /*
//   * Test : Valid transaction.name.
//   */
//  nr_log_event_set_transaction_name(event, "transaction.name");
//  tlib_pass_if_str_equal("Valid transaction name set", "transaction.name",
//                         nr_log_event_get_transaction_name(event));
//  nr_log_event_set_transaction_name(event, "another transaction.name");
//  tlib_pass_if_str_equal("Another valid transaction name set",
//                         "another transaction.name",
//                         nr_log_event_get_transaction_name(event));
//
//  nr_log_event_destroy(&event);
//}
//
//static void test_log_event_category(void) {
//  nr_log_event_t* event = nr_log_event_create();
//
//  // Test : the default is NULL
//  tlib_pass_if_str_equal("The default category", "generic",
//                         nr_log_event_get_category(event));
//
//  // Test : A null event returns NULL
//  tlib_pass_if_null("The default category", nr_log_event_get_category(NULL));
//
//  // Test : passing a NULL event should not blow up
//  nr_log_event_set_category(NULL, NR_LOG_HTTP);
//
//  // Test : setting the category back and forth
//  nr_log_event_set_category(event, NR_LOG_DATASTORE);
//  tlib_pass_if_str_equal("Category should be the one we set - datastore",
//                         "datastore", nr_log_event_get_category(event));
//
//  nr_log_event_set_category(event, NR_LOG_HTTP);
//  tlib_pass_if_str_equal("Category should be the one we set - http", "http",
//                         nr_log_event_get_category(event));
//
//  nr_log_event_destroy(&event);
//}
//
//static void test_log_event_timestamp(void) {
//  nr_log_event_t* event = nr_log_event_create();
//
//  // Test : Get timestamp with a NULL event
//  tlib_pass_if_time_equal("NULL event should give zero", 0,
//                          nr_log_event_get_timestamp(NULL));
//
//  // Test : Set the timestamp a couple times
//  nr_log_event_set_timestamp(event, 553483260);
//  tlib_pass_if_time_equal("Get timestamp should equal 553483260",
//                          553483260 / NR_TIME_DIVISOR_MS,
//                          nr_log_event_get_timestamp(event));
//  nr_log_event_set_timestamp(event, 853483260);
//  tlib_pass_if_time_equal("Get timestamp should equal 853483260",
//                          853483260 / NR_TIME_DIVISOR_MS,
//                          nr_log_event_get_timestamp(event));
//
//  nr_log_event_destroy(&event);
//}
//
//static void test_log_event_duration(void) {
//  nr_log_event_t* event = nr_log_event_create();
//
//  // Test : get duration with a NULL event should return zero
//  tlib_pass_if_time_equal("NULL event should give zero duration", 0,
//                          nr_log_event_get_duration(NULL));
//
//  // Test : Set duration a couple times
//  nr_log_event_set_duration(event, 1 * NR_TIME_DIVISOR_D);
//  tlib_pass_if_time_equal("Get duration should be one", 1,
//                          nr_log_event_get_duration(event));
//  nr_log_event_set_duration(event, 341 * NR_TIME_DIVISOR_D);
//  tlib_pass_if_time_equal("Get duration should be one", 341,
//                          nr_log_event_get_duration(event));
//
//  nr_log_event_destroy(&event);
//}
//
//static void test_log_event_datastore_string_get_and_set(void) {
//  nr_log_event_t* event = nr_log_event_create();
//
//  // Test : that is does not blow up when we give the setter a NULL pointer
//  nr_log_event_set_datastore(NULL, NR_LOG_DATASTORE_COMPONENT, "wallaby");
//  tlib_pass_if_null(
//      "the component should still be NULL",
//      nr_log_event_get_datastore(event, NR_LOG_DATASTORE_COMPONENT));
//  nr_log_event_set_datastore(event, NR_LOG_DATASTORE_COMPONENT, NULL);
//  tlib_pass_if_null(
//      "given a NULL value we should get a NULL",
//      nr_log_event_get_datastore(event, NR_LOG_DATASTORE_COMPONENT));
//
//  // Test : the getter should not blow up when we send it an event with a NULL
//  // component
//  tlib_pass_if_null(
//      "NULL event -> NULL component",
//      nr_log_event_get_datastore(NULL, NR_LOG_DATASTORE_COMPONENT));
//
//  // Test : setting the component back and forth behaves as expected
//  nr_log_event_set_datastore(event, NR_LOG_DATASTORE_COMPONENT, "chicken");
//  tlib_pass_if_str_equal(
//      "should be the component we set 1", "chicken",
//      nr_log_event_get_datastore(event, NR_LOG_DATASTORE_COMPONENT));
//  nr_log_event_set_datastore(event, NR_LOG_DATASTORE_COMPONENT, "oracle");
//  tlib_pass_if_str_equal(
//      "should be the component we set 2", "oracle",
//      nr_log_event_get_datastore(event, NR_LOG_DATASTORE_COMPONENT));
//
//  // Test : setting and getting db_statement
//  tlib_pass_if_null(
//      "the db_statement should still be NULL",
//      nr_log_event_get_datastore(event, NR_LOG_DATASTORE_DB_STATEMENT));
//  nr_log_event_set_datastore(event, NR_LOG_DATASTORE_DB_STATEMENT,
//                              "SELECT * FROM BOBBY;");
//  tlib_pass_if_str_equal(
//      "set db_statement to BOBBY", "SELECT * FROM BOBBY;",
//      nr_log_event_get_datastore(event, NR_LOG_DATASTORE_DB_STATEMENT));
//  nr_log_event_set_datastore(event, NR_LOG_DATASTORE_DB_STATEMENT,
//                              "SELECT * FROM transactions;");
//  tlib_pass_if_str_equal(
//      "set db_statement to transactions", "SELECT * FROM transactions;",
//      nr_log_event_get_datastore(event, NR_LOG_DATASTORE_DB_STATEMENT));
//
//  // Test : setting and getting db_instance
//  tlib_pass_if_null(
//      "the db_statement should still be NULL",
//      nr_log_event_get_datastore(event, NR_LOG_DATASTORE_DB_INSTANCE));
//  nr_log_event_set_datastore(event, NR_LOG_DATASTORE_DB_INSTANCE,
//                              "I'm a box somewhere");
//  tlib_pass_if_str_equal(
//      "set db_statement to somewhere", "I'm a box somewhere",
//      nr_log_event_get_datastore(event, NR_LOG_DATASTORE_DB_INSTANCE));
//  nr_log_event_set_datastore(event, NR_LOG_DATASTORE_DB_INSTANCE,
//                              "some instance");
//  tlib_pass_if_str_equal(
//      "set db_statement to some instance", "some instance",
//      nr_log_event_get_datastore(event, NR_LOG_DATASTORE_DB_INSTANCE));
//
//  // Test : setting and getting peer_addresss
//  tlib_pass_if_null(
//      "the db_statement should still be NULL",
//      nr_log_event_get_datastore(event, NR_LOG_DATASTORE_PEER_ADDRESS));
//  nr_log_event_set_datastore(event, NR_LOG_DATASTORE_PEER_ADDRESS,
//                              "an address");
//  tlib_pass_if_str_equal(
//      "set db_statement to an address", "an address",
//      nr_log_event_get_datastore(event, NR_LOG_DATASTORE_PEER_ADDRESS));
//  nr_log_event_set_datastore(event, NR_LOG_DATASTORE_PEER_ADDRESS, "turkey");
//  tlib_pass_if_str_equal(
//      "set db_statement to turkey", "turkey",
//      nr_log_event_get_datastore(event, NR_LOG_DATASTORE_PEER_ADDRESS));
//
//  // Test : setting and getting peer_hostname
//  tlib_pass_if_null(
//      "the db_statement should still be NULL",
//      nr_log_event_get_datastore(event, NR_LOG_DATASTORE_PEER_HOSTNAME));
//  nr_log_event_set_datastore(event, NR_LOG_DATASTORE_PEER_HOSTNAME, "wombat");
//  tlib_pass_if_str_equal(
//      "set db_statement to wombat", "wombat",
//      nr_log_event_get_datastore(event, NR_LOG_DATASTORE_PEER_HOSTNAME));
//  nr_log_event_set_datastore(event, NR_LOG_DATASTORE_PEER_HOSTNAME, "rabbit");
//  tlib_pass_if_str_equal(
//      "set db_statement to rabbit", "rabbit",
//      nr_log_event_get_datastore(event, NR_LOG_DATASTORE_PEER_HOSTNAME));
//
//  nr_log_event_destroy(&event);
//}
//
//static void test_log_events_extern_get_and_set(void) {
//  nr_log_event_t* log = nr_log_event_create();
//
//  // Test : That nothing blows up if nulls are given.
//  nr_log_event_set_external(NULL, NR_LOG_EXTERNAL_URL, "no log");
//  tlib_pass_if_null("The URL should still be NULL",
//                    nr_log_event_get_external(log, NR_LOG_EXTERNAL_URL));
//  nr_log_event_set_external(log, NR_LOG_EXTERNAL_COMPONENT, NULL);
//  tlib_pass_if_str_equal(
//      "When set external is given a NULL target value it should stay NULL",
//      NULL, nr_log_event_get_external(log, NR_LOG_EXTERNAL_COMPONENT));
//  tlib_pass_if_null("NULL event -> NULL Method",
//                    nr_log_event_get_external(log, NR_LOG_EXTERNAL_METHOD));
//  nr_log_event_set_external_status(NULL, 200);
//  tlib_pass_if_int_equal("NULL event", 0,
//                         nr_log_event_get_external_status(log));
//
//  // Test : setting the component back and forth behaves as expected.
//  nr_log_event_set_external(log, NR_LOG_EXTERNAL_COMPONENT, "curl");
//  tlib_pass_if_str_equal(
//      "The component should be curl",
//      nr_log_event_get_external(log, NR_LOG_EXTERNAL_COMPONENT), "curl");
//  nr_log_event_set_external(log, NR_LOG_EXTERNAL_COMPONENT, "Guzzle 6");
//  tlib_pass_if_str_equal(
//      "The component should be Guzzle",
//      nr_log_event_get_external(log, NR_LOG_EXTERNAL_COMPONENT), "Guzzle 6");
//
//  // Test : setting and getting the method and URL
//  nr_log_event_set_external(log, NR_LOG_EXTERNAL_METHOD, "GET");
//  tlib_pass_if_str_equal(
//      "The method should be GET",
//      nr_log_event_get_external(log, NR_LOG_EXTERNAL_METHOD), "GET");
//  nr_log_event_set_external(log, NR_LOG_EXTERNAL_URL, "wombats.com");
//  tlib_pass_if_str_equal("The method should be wombats.com",
//                         nr_log_event_get_external(log, NR_LOG_EXTERNAL_URL),
//                         "wombats.com");
//
//  // Test : setting and getting the status multiple times
//  nr_log_event_set_external_status(log, 200);
//  tlib_pass_if_int_equal("The status should be 200", 200,
//                         nr_log_event_get_external_status(log));
//
//  nr_log_event_set_external_status(log, 400);
//  tlib_pass_if_int_equal("The status should be 400", 400,
//                         nr_log_event_get_external_status(log));
//  nr_log_event_destroy(&log);
//}
//
//static void test_log_event_error(void) {
//  nr_log_event_t* event = nr_log_event_create();
//
//  /*
//   * Test : Bad parameters.
//   */
//  nr_log_event_set_error_message(NULL, "error message");
//  tlib_pass_if_null("NULL log event -> NULL error message",
//                    nr_log_event_get_error_message(event));
//
//  nr_log_event_set_error_message(event, NULL);
//  tlib_pass_if_null("NULL error message -> no error message set",
//                    nr_log_event_get_error_message(event));
//
//  nr_log_event_set_error_class(NULL, "error class");
//  tlib_pass_if_null("NULL log event -> NULL error class",
//                    nr_log_event_get_error_class(event));
//
//  nr_log_event_set_error_class(event, NULL);
//  tlib_pass_if_null("NULL error class -> no error class set",
//                    nr_log_event_get_error_class(event));
//
//  /*
//   * Test : error.message.
//   */
//  nr_log_event_set_error_message(event, "error message 1");
//  tlib_pass_if_str_equal("test error message set once", "error message 1",
//                         nr_log_event_get_error_message(event));
//
//  nr_log_event_set_error_message(event, "error message 2");
//  tlib_pass_if_str_equal("test error message set again", "error message 2",
//                         nr_log_event_get_error_message(event));
//
//  /*
//   * Test : error.class.
//   */
//  nr_log_event_set_error_class(event, "error class 1");
//  tlib_pass_if_str_equal("test error class set once", "error class 1",
//                         nr_log_event_get_error_class(event));
//
//  nr_log_event_set_error_class(event, "error class 2");
//  tlib_pass_if_str_equal("test error class set again", "error class 2",
//                         nr_log_event_get_error_class(event));
//
//  nr_log_event_destroy(&event);
//}
//
//static void test_log_event_set_attribute_user(void) {
//  nr_log_event_t* log = nr_log_event_create();
//  nrobj_t* value = nro_new_string("value");
//  nr_status_t err;
//
//  /*
//   * Invalid arguments, this shouldn't blow up.
//   */
//
//  nr_log_event_set_attribute_user(NULL, "key", value);
//  nr_log_event_set_attribute_user(log, NULL, value);
//  nr_log_event_set_attribute_user(log, "key", NULL);
//
//  /*
//   * Add an attribute and test for it.
//   */
//  nr_log_event_set_attribute_user(log, "key", value);
//
//  tlib_pass_if_size_t_equal("Adding a log attribute saves it in attributes", 1,
//                            nro_getsize(log->user_attributes));
//  tlib_pass_if_bool_equal(
//      "Adding a log attribute saves it in attributes", "value",
//      nro_get_hash_string(log->user_attributes, "key", &err));
//  tlib_pass_if_true("Adding a log attribute saves it in attributes",
//                    NR_SUCCESS == err, "Expected NR_SUCCESS");
//
//  nro_delete(value);
//  nr_log_event_destroy(&log);
//}
//
//static void test_log_event_txn_parent_attributes(void) {
//  nr_log_event_t* event = nr_log_event_create();
//
//  /*
//   * Test : Bad parameters.
//   */
//  nr_log_event_set_parent_attribute(NULL, NR_LOG_PARENT_TYPE, "parent.type");
//  tlib_pass_if_null("test NULL event", nr_log_event_get_error_class(event));
//  nr_log_event_set_parent_attribute(event, NR_LOG_PARENT_TYPE, NULL);
//  tlib_pass_if_null("test NULL parent.type", nr_log_event_get_parent_attribute(
//                                                 event, NR_LOG_PARENT_TYPE));
//
//  nr_log_event_set_parent_transport_duration(NULL, 100000);
//  tlib_pass_if_time_equal("test NULL event", 0.0,
//                          nr_log_event_get_parent_transport_duration(event));
//
//  /*
//   * Test : parent.type.
//   */
//  nr_log_event_set_parent_attribute(event, NR_LOG_PARENT_TYPE, "parent.type");
//  tlib_pass_if_str_equal(
//      "test parent.type", "parent.type",
//      nr_log_event_get_parent_attribute(event, NR_LOG_PARENT_TYPE));
//
//  /*
//   * Test : parent.app.
//   */
//  nr_log_event_set_parent_attribute(event, NR_LOG_PARENT_APP, "parent.app");
//  tlib_pass_if_str_equal(
//      "test parent.app", "parent.app",
//      nr_log_event_get_parent_attribute(event, NR_LOG_PARENT_APP));
//
//  /*
//   * Test : parent.account.
//   */
//  nr_log_event_set_parent_attribute(event, NR_LOG_PARENT_ACCOUNT,
//                                     "parent.account");
//  tlib_pass_if_str_equal(
//      "test parent.account", "parent.account",
//      nr_log_event_get_parent_attribute(event, NR_LOG_PARENT_ACCOUNT));
//
//  /*
//   * Test : parent.transportType.
//   */
//  nr_log_event_set_parent_attribute(event, NR_LOG_PARENT_TRANSPORT_TYPE,
//                                     "parent.transportType");
//  tlib_pass_if_str_equal(
//      "test parent.transportType", "parent.transportType",
//      nr_log_event_get_parent_attribute(event, NR_LOG_PARENT_TRANSPORT_TYPE));
//
//  /*
//   * Test : parent.transportDuration.
//   */
//  nr_log_event_set_parent_transport_duration(event, 553483260);
//  tlib_pass_if_time_equal("test parent.transportDuration",
//                          553483260 / NR_TIME_DIVISOR,
//                          nr_log_event_get_parent_transport_duration(event));
//
//  nr_log_event_destroy(&event);
//}
//
//static void test_log_event_set_attribute_agent(void) {
//  nr_log_event_t* log = nr_log_event_create();
//  nrobj_t* value = nro_new_string("value");
//  nr_status_t err;
//
//  /*
//   * Invalid arguments, this shouldn't blow up.
//   */
//  nr_log_event_set_attribute_agent(NULL, "errorMessage", value);
//  nr_log_event_set_attribute_agent(log, NULL, value);
//  nr_log_event_set_attribute_agent(log, "errorMessage", NULL);
//
//  /*
//   * Add an attribute and test for it.
//   */
//  nr_log_event_set_attribute_agent(log, "errorMessage", value);
//
//  tlib_pass_if_size_t_equal(
//      "Adding a log attribute saves it in agent attributes", 1,
//      nro_getsize(log->agent_attributes));
//  tlib_pass_if_bool_equal(
//      "Adding a log attribute saves it in agent attributes", "value",
//      nro_get_hash_string(log->agent_attributes, "errorMessage", &err));
//  tlib_pass_if_true("Adding a log attribute saves it in agent attributes",
//                    NR_SUCCESS == err, "Expected NR_SUCCESS");
//
//  nro_delete(value);
//  nr_log_event_destroy(&log);
//}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 1, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_log_event_create_destroy();
  test_log_event_to_json();
  test_log_event_to_json_buffer();
  //test_log_event_guid();
  //test_log_event_parent();
  //test_log_event_transaction_id();
  //test_log_event_name();
  //test_log_event_transaction_name();
  //test_log_event_category();
  //test_log_event_timestamp();
  //test_log_event_duration();
  //test_log_event_datastore_string_get_and_set();
  //test_log_events_extern_get_and_set();
  //test_log_event_error();
  //test_log_event_set_attribute_user();
  //test_log_event_txn_parent_attributes();
  //test_log_event_set_attribute_agent();
}
