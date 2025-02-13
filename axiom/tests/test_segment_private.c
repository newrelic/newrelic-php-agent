/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>

#include "nr_segment_private.h"
#include "nr_segment.h"
#include "util_memory.h"

#include "tlib_main.h"

static void test_bad_parameters(void) {
  nr_segment_children_init(NULL);
  nr_segment_children_get_prev(NULL, NULL);
  nr_segment_children_get_next(NULL, NULL);
  nr_segment_children_add(NULL, NULL);
  nr_segment_children_remove(NULL, NULL);
  nr_segment_children_deinit(NULL);
  nr_segment_destroy_typed_attributes(NR_SEGMENT_CUSTOM, NULL);
  nr_segment_destroy_fields(NULL);
  nr_segment_datastore_destroy_fields(NULL);
  nr_segment_external_destroy_fields(NULL);
  nr_segment_message_destroy_fields(NULL);
  nr_segment_metric_destroy_fields(NULL);
  nr_segment_error_destroy_fields(NULL);
}

static void test_create_add_destroy(void) {
  nr_segment_children_t children;
  nr_segment_t embryo;
  nr_segment_t first_born;
  nr_segment_t second_born;
  nr_segment_t neighbor_kid;

  /*
   * Test : Bad parameters.
   */
  nr_segment_children_init(NULL);

  /* Cannot operate on the children when the array is not initialized */
  nr_segment_children_get_prev(&children, NULL);

  /*
   * Test : Normal operation.
   */
  nr_segment_children_init(&children);
  tlib_pass_if_null("An empty array cannot have a prev child",
                    nr_segment_children_get_prev(&children, &embryo));
  tlib_pass_if_null("An empty array cannot have a next child",
                    nr_segment_children_get_next(&children, &embryo));

  nr_segment_children_add(&children, &first_born);
  tlib_pass_if_ptr_equal("A first child must be successfully added",
                         nr_segment_children_get(&children, 0), &first_born);
  tlib_pass_if_null("An only child cannot have a prev child",
                    nr_segment_children_get_prev(&children, &first_born));
  tlib_pass_if_null("An only child cannot have a next child",
                    nr_segment_children_get_next(&children, &first_born));

  nr_segment_children_add(&children, &second_born);
  tlib_pass_if_ptr_equal("A second child must be successfully added",
                         nr_segment_children_get(&children, 1), &second_born);
  tlib_pass_if_ptr_equal("A second child must be inserted after the first",
                         nr_segment_children_get_prev(&children, &second_born),
                         &first_born);
  tlib_pass_if_ptr_equal("A first child must be inserted before the second",
                         nr_segment_children_get_next(&children, &first_born),
                         &second_born);
  tlib_pass_if_ptr_equal("Children not in the family must not have a next",
                         nr_segment_children_get_next(&children, &neighbor_kid),
                         NULL);
  tlib_pass_if_ptr_equal("Children not in the family must not have a prev",
                         nr_segment_children_get_prev(&children, &neighbor_kid),
                         NULL);

  nr_segment_children_deinit(&children);
}

#define NR_EXTENDED_FAMILY_SIZE 100
static void test_create_add_destroy_extended(void) {
  int i = 0;
  nr_segment_children_t children;
  nr_segment_t child;

  nr_segment_children_init(&children);

  for (i = 0; i < NR_EXTENDED_FAMILY_SIZE; i++) {
    nr_segment_children_add(&children, &child);
    tlib_pass_if_ptr_equal("A child must be successfully added",
                           nr_segment_children_get(&children, i), &child);
    tlib_pass_if_int_equal("The number of used locations must be incremented",
                           nr_segment_children_size(&children), i + 1);
  }
  nr_segment_children_deinit(&children);
}

static void test_remove(void) {
  nr_segment_children_t children;
  nr_segment_t first_born;
  nr_segment_t second_born;
  nr_segment_t third_born;
  nr_segment_t fourth_born;
  nr_segment_t fifth_born;

  const size_t total_children = 5;

  nr_segment_children_init(&children);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_false(
      "Cannot remove a segment from an uninitialized array of children",
      nr_segment_children_remove(&children, &first_born), "Expected false");

  /*
   * Test : Normal operation.
   */

  /* Build a mock array of children. */
  nr_segment_children_init(&children);
  nr_segment_children_add(&children, &first_born);
  nr_segment_children_add(&children, &second_born);
  nr_segment_children_add(&children, &third_born);
  nr_segment_children_add(&children, &fourth_born);
  nr_segment_children_add(&children, &fifth_born);

  /* Briefly affirm the array is well-formed */
  tlib_pass_if_uint_equal(
      "Adding five children must yield an expected used value",
      nr_segment_children_size(&children), total_children);

  /* Affirm successful removal of the first child */
  tlib_pass_if_true(
      "Removing an existing segment from an array of children must be "
      "successful",
      nr_segment_children_remove(&children, &first_born), "Expected true");
  tlib_pass_if_uint_equal(
      "Removing an existing segment from an array of children must "
      "reduce the number of used locations",
      nr_segment_children_size(&children), total_children - 1);
  tlib_pass_if_ptr_equal(
      "Removing the first born means the second born must have a new prev",
      nr_segment_children_get_prev(&children, &second_born), &fifth_born);
  tlib_pass_if_ptr_equal(
      "Removing the first born means the second born must still have a next",
      nr_segment_children_get_next(&children, &second_born), &third_born);

  /* Affirm successful removal of a child in the middle */
  tlib_pass_if_true(
      "Removing an existing segment from an array of children must be "
      "successful",
      nr_segment_children_remove(&children, &third_born), "Expected true");
  tlib_pass_if_uint_equal(
      "Removing an existing segment from an array of children must "
      "reduce the number of used locations",
      nr_segment_children_size(&children), total_children - 2);
  tlib_pass_if_ptr_equal(
      "Removing the third born means the fourth is after the second",
      nr_segment_children_get_next(&children, &second_born), &fourth_born);

  /* Affirm successful removal of a last child */
  tlib_pass_if_true(
      "Removing an existing segment from an array of children must be "
      "successful",
      nr_segment_children_remove(&children, &fifth_born), "Expected true");
  tlib_pass_if_uint_equal(
      "Removing an existing segment from an array of children must "
      "reduce the number of used locations",
      nr_segment_children_size(&children), total_children - 3);
  tlib_pass_if_ptr_equal(
      "Removing the fifth born means the previous last element has a new next",
      nr_segment_children_get_next(&children, &fourth_born), &second_born);

  /* Clean up the mocked array of children */
  nr_segment_children_deinit(&children);
}

static void test_set_custom(void) {
  nr_segment_t s = {0};
  nr_segment_t t = {.type = NR_SEGMENT_DATASTORE};

  nr_segment_datastore_t d = {.component = "component",
                              .sql = "sql",
                              .sql_obfuscated = "sql_obfuscated",
                              .input_query_json = "input_query_json",
                              .backtrace_json = "backtrace_json",
                              .explain_plan_json = "explain_plan_json"};
  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_false("Setting a NULL segment to custom must not be successful",
                     nr_segment_set_custom(NULL), "Expected false");

  /*
   * Test : Normal operation.
   */

  tlib_pass_if_true("Setting an untyped segment to custom must be successful",
                    nr_segment_set_custom(&s), "Expected true");
  tlib_pass_if_int_equal(
      "Setting an untyped segment to custom must set the type",
      (int)NR_SEGMENT_CUSTOM, (int)s.type);
  nr_segment_set_datastore(&t, &d);
  tlib_pass_if_true("Setting a datastore segment to custom must be successful",
                    nr_segment_set_custom(&t), "Expected true");
  tlib_pass_if_int_equal(
      "Setting an untyped segment to custom must set the type",
      (int)NR_SEGMENT_CUSTOM, (int)s.type);

  /* Valgrind shall affirm that the datastore attributes for t were cleaned up
   */
}

static void test_set_destroy_datastore_fields(void) {
  nr_segment_t s = {.type = NR_SEGMENT_DATASTORE};

  nr_segment_datastore_t d = {.component = "component",
                              .sql = "sql",
                              .sql_obfuscated = "sql_obfuscated",
                              .input_query_json = "input_query_json",
                              .backtrace_json = "backtrace_json",
                              .explain_plan_json = "explain_plan_json"};

  nr_segment_external_t e = {.transaction_guid = "transaction_guid",
                             .uri = "uri",
                             .library = "library",
                             .procedure = "procedure",
                             .status = 200};
  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_false(
      "Setting a NULL segment's datastore attributes must not be successful",
      nr_segment_set_datastore(NULL, &d), "Expected false");

  tlib_pass_if_false(
      "Setting a segment with NULL datastore attributes must not be successful",
      nr_segment_set_datastore(&s, NULL), "Expected false");

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_true(
      "Setting a segment's datastore attributes must be successful",
      nr_segment_set_datastore(&s, &d), "Expected true");
  tlib_pass_if_int_equal(
      "Setting a segment's datastore attributes must also set the type",
      (int)NR_SEGMENT_DATASTORE, (int)s.type);

  tlib_pass_if_true(
      "Setting a segment from datastore attributes to external attributes must "
      "be successful",
      nr_segment_set_external(&s, &e), "Expected true");
  tlib_pass_if_int_equal(
      "Setting a segment's external attributes must also set the type",
      (int)NR_SEGMENT_EXTERNAL, (int)s.type);

  /* Valgrind shall affirm that the datastore attributes for s were cleaned
   * up when the segment type was changed from datastore to external.
   */

  /* Clean up */
  nr_segment_destroy_typed_attributes(NR_SEGMENT_EXTERNAL, &s.typed_attributes);
}

static void test_set_destroy_external_fields(void) {
  nr_segment_t s = {.type = NR_SEGMENT_EXTERNAL};

  nr_segment_datastore_t d = {.component = "component",
                              .sql = "sql",
                              .sql_obfuscated = "sql_obfuscated",
                              .input_query_json = "input_query_json",
                              .backtrace_json = "backtrace_json",
                              .explain_plan_json = "explain_plan_json"};

  nr_segment_external_t e = {.transaction_guid = "transaction_guid",
                             .uri = "uri",
                             .library = "library",
                             .procedure = "procedure",
                             .status = 200};
  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_false(
      "Setting a NULL segment's external attributes must not be successful",
      nr_segment_set_external(NULL, &e), "Expected false");

  tlib_pass_if_false(
      "Setting a segment with NULL external attributes must not be successful",
      nr_segment_set_external(&s, NULL), "Expected false");

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_true(
      "Setting a segment's external attributes must be successful",
      nr_segment_set_external(&s, &e), "Expected true");

  tlib_pass_if_true(
      "Setting a segment from external attributes to datastore attributes must "
      "be successful",
      nr_segment_set_datastore(&s, &d), "Expected true");

  /* Valgrind shall affirm that the external attributes for s were cleaned
   * up when the segment type was changed from external to datastore.
   */

  /* Clean up */
  nr_segment_destroy_typed_attributes(NR_SEGMENT_DATASTORE,
                                      &s.typed_attributes);
}

static void test_set_destroy_message_fields(void) {
  nr_segment_t s = {.type = NR_SEGMENT_MESSAGE};

  nr_segment_message_t m = {.message_action = NR_SPANKIND_CLIENT,
                            .messaging_system = "my_messaging_system",
                            .server_address = "localhost"};

  nr_segment_external_t e = {.transaction_guid = "transaction_guid",
                             .uri = "uri",
                             .library = "library",
                             .procedure = "procedure",
                             .status = 200};
  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_false(
      "Setting a NULL segment's message attributes must not be successful",
      nr_segment_set_message(NULL, &m), "Expected false");

  tlib_pass_if_false(
      "Setting a segment with NULL message attributes must not be successful",
      nr_segment_set_message(&s, NULL), "Expected false");

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_true("Setting a segment's message attributes must be successful",
                    nr_segment_set_message(&s, &m), "Expected true");

  tlib_pass_if_true(
      "Setting a segment from message attributes to external attributes must "
      "be successful",
      nr_segment_set_external(&s, &e), "Expected true");

  /* Valgrind shall affirm that the attributes for s were cleaned
   * up when the segment type was changed from message to external.
   */

  /* Clean up */
  nr_segment_destroy_typed_attributes(NR_SEGMENT_EXTERNAL, &s.typed_attributes);
}

static void test_destroy_typed_attributes(void) {
  nr_segment_t s = {0};
  char* test_string = "0123456789";

  /*
   * Test : Bad parameters.
   */
  nr_segment_destroy_typed_attributes(NR_SEGMENT_EXTERNAL, NULL);
  nr_segment_destroy_typed_attributes(NR_SEGMENT_EXTERNAL, &s.typed_attributes);
  tlib_pass_if_null(
      "Even with bad parameters, nr_segment_destroy_typed_attributes should "
      "not crash and s.typed_attributes should be NULL",
      s.typed_attributes);
  nr_segment_destroy_typed_attributes(NR_SEGMENT_DATASTORE, NULL);
  nr_segment_destroy_typed_attributes(NR_SEGMENT_DATASTORE,
                                      &s.typed_attributes);
  tlib_pass_if_null(
      "Even with bad parameters, nr_segment_destroy_typed_attributes should "
      "not crash and s.typed_attributes should be NULL",
      s.typed_attributes);
  nr_segment_destroy_typed_attributes(NR_SEGMENT_MESSAGE, NULL);
  nr_segment_destroy_typed_attributes(NR_SEGMENT_MESSAGE, &s.typed_attributes);
  tlib_pass_if_null(
      "Even with bad parameters, nr_segment_destroy_typed_attributes should "
      "not crash and s.typed_attributes should be NULL",
      s.typed_attributes);

  /*
   * Test : Clean up typed attributes for a message segment
   */
  s.type = NR_SEGMENT_MESSAGE;
  s.typed_attributes = nr_zalloc(sizeof(nr_segment_typed_attributes_t));
  s.typed_attributes->message.destination_name = nr_strdup("queue_name");
  s.typed_attributes->message.messaging_system = nr_strdup("aws_sqs");
  s.typed_attributes->message.server_address = nr_strdup("localhost");

  /*
   * Valgrind shall affirm that the attributes were cleaned up.
   */
  nr_segment_destroy_typed_attributes(NR_SEGMENT_MESSAGE, &s.typed_attributes);
  tlib_pass_if_null(
      "After nr_segment_destroy_typed_attributes, s.typed_attributes should be "
      "NULL",
      s.typed_attributes);
  /*
   * Test : Clean up typed attributes for an external segment
   */
  s.type = NR_SEGMENT_EXTERNAL;
  s.typed_attributes = nr_zalloc(sizeof(nr_segment_typed_attributes_t));
  s.typed_attributes->external.transaction_guid = nr_strdup(test_string);
  s.typed_attributes->external.uri = nr_strdup(test_string);
  s.typed_attributes->external.library = nr_strdup(test_string);
  s.typed_attributes->external.procedure = nr_strdup(test_string);
  s.typed_attributes->external.status = 200;

  /*
   * Valgrind shall affirm that the attributes were cleaned up.
   */
  nr_segment_destroy_typed_attributes(NR_SEGMENT_EXTERNAL, &s.typed_attributes);
  tlib_pass_if_null(
      "After nr_segment_destroy_typed_attributes, s.typed_attributes should be "
      "NULL",
      s.typed_attributes);

  /*
   * Test : Clean up typed attributes for a datastore segment
   */
  s.type = NR_SEGMENT_DATASTORE;
  s.typed_attributes = nr_zalloc(sizeof(nr_segment_typed_attributes_t));
  s.typed_attributes->datastore.component = nr_strdup(test_string);
  s.typed_attributes->datastore.sql = nr_strdup(test_string);
  s.typed_attributes->datastore.sql_obfuscated = nr_strdup(test_string);
  s.typed_attributes->datastore.input_query_json = nr_strdup(test_string);
  s.typed_attributes->datastore.backtrace_json = nr_strdup(test_string);
  s.typed_attributes->datastore.explain_plan_json = nr_strdup(test_string);
  s.typed_attributes->datastore.instance.host = nr_strdup(test_string);
  s.typed_attributes->datastore.instance.port_path_or_id
      = nr_strdup(test_string);
  s.typed_attributes->datastore.instance.database_name = nr_strdup(test_string);

  /*
   * Valgrind shall affirm that the attributes were cleaned up.
   */
  nr_segment_destroy_typed_attributes(NR_SEGMENT_DATASTORE,
                                      &s.typed_attributes);
  tlib_pass_if_null(
      "After nr_segment_destroy_typed_attributes, s.typed_attributes should be "
      "NULL",
      s.typed_attributes);
}

static void test_destroy_fields(void) {
  nr_segment_t s = {0};
  char* test_string = "0123456789";

  s.id = nr_strdup(test_string);
  s.metrics = nr_vector_create(8, NULL, NULL);
  s.attributes = nr_attributes_create(NULL);
  s.type = NR_SEGMENT_CUSTOM;
  s.exclusive_time = nr_exclusive_time_create(0, 1, 2);

  nr_segment_destroy_fields(&s);
}

static void test_destroy_metric(void) {
  nr_segment_metric_t sm = {
      .name = nr_strdup("Custom/Metric/To/Be/Destroyed"),
      .scoped = false,
  };

  nr_segment_metric_destroy_fields(&sm);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_bad_parameters();
  test_create_add_destroy();
  test_create_add_destroy_extended();
  test_remove();
  test_set_custom();
  test_set_destroy_datastore_fields();
  test_set_destroy_external_fields();
  test_set_destroy_message_fields();
  test_destroy_typed_attributes();
  test_destroy_fields();
  test_destroy_metric();
}
