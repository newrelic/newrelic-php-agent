/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"
#include "nr_header.h"
#include "nr_segment_message.h"
#include "test_segment_helpers.h"

typedef struct {
  const char* test_name;
  const char* name;
  const char* txn_rollup_metric;
  const char* library_metric;
  uint32_t num_metrics;
  const char* destination_name;
  const char* cloud_region;
  const char* cloud_account_id;
  const char* messaging_system;
  const char* cloud_resource_id;
  const char* server_address;

} segment_message_expecteds_t;

static nr_segment_t* mock_txn_segment(void) {
  nrtxn_t* txn = new_txn(0);

  return nr_segment_start(txn, NULL, NULL);
}

static void test_message_segment(nr_segment_message_params_t* params,
                                 bool message_attributes_enabled,
                                 segment_message_expecteds_t expecteds) {
  nr_segment_t* seg = mock_txn_segment();
  nrtxn_t* txn = seg->txn;
  seg->txn->options.message_tracer_segment_parameters_enabled
      = message_attributes_enabled;

  test_segment_message_end_and_keep(&seg, params);

  tlib_pass_if_str_equal(expecteds.test_name, expecteds.name,
                         nr_string_get(seg->txn->trace_strings, seg->name));
  test_txn_metric_created(expecteds.test_name, txn->unscoped_metrics,
                          expecteds.txn_rollup_metric);
  test_txn_metric_created(expecteds.test_name, txn->unscoped_metrics,
                          expecteds.library_metric);
  test_metric_vector_size(seg->metrics, expecteds.num_metrics);
  tlib_pass_if_true(expecteds.test_name, NR_SEGMENT_MESSAGE == seg->type,
                    "NR_SEGMENT_MESSAGE");
  tlib_pass_if_str_equal(expecteds.test_name,
                         seg->typed_attributes->message.destination_name,
                         expecteds.destination_name);
  tlib_pass_if_str_equal(expecteds.test_name,
                         seg->typed_attributes->message.cloud_region,
                         expecteds.cloud_region);
  tlib_pass_if_str_equal(expecteds.test_name,
                         seg->typed_attributes->message.cloud_account_id,
                         expecteds.cloud_account_id);
  tlib_pass_if_str_equal(expecteds.test_name,
                         seg->typed_attributes->message.messaging_system,
                         expecteds.messaging_system);
  tlib_pass_if_str_equal(expecteds.test_name,
                         seg->typed_attributes->message.cloud_resource_id,
                         expecteds.cloud_resource_id);
  tlib_pass_if_str_equal(expecteds.test_name,
                         seg->typed_attributes->message.server_address,
                         expecteds.server_address);

  nr_txn_destroy(&txn);
}

static void test_bad_parameters(void) {
  nr_segment_t seg_null = {0};
  nr_segment_t* seg_null_ptr;
  nr_segment_t* seg = mock_txn_segment();
  nrtxn_t* txn = seg->txn;
  nr_segment_message_params_t params = {0};

  tlib_pass_if_false("bad parameters", nr_segment_message_end(NULL, &params),
                     "expected false");

  seg_null_ptr = NULL;
  tlib_pass_if_false("bad parameters",
                     nr_segment_message_end(&seg_null_ptr, &params),
                     "expected false");

  seg_null_ptr = &seg_null;
  tlib_pass_if_false("bad parameters",
                     nr_segment_message_end(&seg_null_ptr, &params),
                     "expected false");

  tlib_pass_if_false("bad parameters", nr_segment_message_end(&seg, NULL),
                     "expected false");
  test_metric_vector_size(seg->metrics, 0);

  nr_txn_destroy(&txn);
}

static void test_segment_message_destination_type(void) {
  /*
   * The following values are used to create metrics:
   * library
   * destination_type
   * message_action
   * destination_name
   */
  /* Test NR_MESSAGE_DESTINATION_TYPE_TEMP_TOPIC destination type */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TEMP_TOPIC,
          .destination_name = "my_queue_or_topic"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name
          = "Test NR_MESSAGE_DESTINATION_TYPE_TEMP_TOPIC destination type",
          .name = "MessageBroker/SQS/Topic/Produce/Temp",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /* Test NR_MESSAGE_DESTINATION_TYPE_TEMP_QUEUE destination type */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TEMP_QUEUE,
          .destination_name = "my_queue_or_topic"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name
          = "Test NR_MESSAGE_DESTINATION_TYPE_TEMP_QUEUE destination type",
          .name = "MessageBroker/SQS/Queue/Produce/Temp",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /* Test NR_MESSAGE_DESTINATION_TYPE_EXCHANGE destination type */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_EXCHANGE,
          .destination_name = "my_queue_or_topic"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name
          = "Test NR_MESSAGE_DESTINATION_TYPE_EXCHANGE destination type",
          .name = "MessageBroker/SQS/Exchange/Produce/Named/my_queue_or_topic",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /* Test NR_MESSAGE_DESTINATION_TYPE_TOPIC destination type */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_queue_or_topic"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name
          = "Test NR_MESSAGE_DESTINATION_TYPE_EXCHANGE destination type",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_queue_or_topic",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /* Test NR_MESSAGE_DESTINATION_TYPE_QUEUE destination type */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_QUEUE,
          .destination_name = "my_queue_or_topic"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name
          = "Test NR_MESSAGE_DESTINATION_TYPE_QUEUE destination type",
          .name = "MessageBroker/SQS/Queue/Produce/Named/my_queue_or_topic",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});
}

static void test_segment_message_message_action(void) {
  /*
   * The following values are used to create metrics:
   * library
   * destination_type
   * message_action
   * destination_name
   */

  /* Test NR_SPAN_PRODUCER message action */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_queue_or_topic"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test NR_SPAN_PRODUCER message action",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_queue_or_topic",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /* Test NR_SPAN_CONSUMER message action */

  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPAN_CONSUMER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_queue_or_topic"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test NR_SPAN_CONSUMER message action",
          .name = "MessageBroker/SQS/Topic/Consume/Named/my_queue_or_topic",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /*
   * Test NR_SPAN_CLIENT message action; this is not
   * allowed for message segments, should show unknown.
   */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPAN_CLIENT,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_queue_or_topic"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test NR_SPAN_CLIENT message action",
          .name = "MessageBroker/SQS/Topic/<unknown>/Named/my_queue_or_topic",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});
}

static void test_segment_message_library(void) {
  /*
   * The following values are used to create metrics:
   * library
   * destination_type
   * message_action
   * destination_name
   */
  /* Test null library */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = NULL,
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_queue_or_topic"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test null library",
          .name
          = "MessageBroker/<unknown>/Topic/Produce/Named/my_queue_or_topic",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/<unknown>/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /* Test empty library */

  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_queue_or_topic"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test empty library",
          .name
          = "MessageBroker/<unknown>/Topic/Produce/Named/my_queue_or_topic",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/<unknown>/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /* Test valid library */

  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_queue_or_topic"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test valid library",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_queue_or_topic",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});
}

static void test_segment_message_destination_name(void) {
  /*
   * The following values are used to create metrics:
   * library
   * destination_type
   * message_action
   * destination_name
   */
  /* Test null destination_name */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = NULL},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test null destination_name",
          .name = "MessageBroker/SQS/Topic/Produce/Named/<unknown>",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = NULL,
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /* Test empty destination_name */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = ""},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test empty destination_name",
          .name = "MessageBroker/SQS/Topic/Produce/Named/<unknown>",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = NULL,
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /* Test valid destination_name */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test valid destination_name",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});
}

static void test_segment_message_cloud_region(void) {
  /*
   * cloud_region values should NOT impact the creation of
   * metrics.
   */

  /* Test null cloud_region */
  test_message_segment(
      &(nr_segment_message_params_t){
          .cloud_region = NULL,
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test null cloud_region",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /* Test empty cloud_region */
  test_message_segment(
      &(nr_segment_message_params_t){
          .cloud_region = "",
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test empty cloud_region",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /* Test valid cloud_region */
  test_message_segment(
      &(nr_segment_message_params_t){
          .cloud_region = "wild-west-1",
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test valid cloud_region",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = "wild-west-1",
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});
}

static void test_segment_message_cloud_account_id(void) {
  /*
   * cloud_account_id values should NOT impact the creation
   * of metrics.
   */

  /* Test null cloud_account_id */
  test_message_segment(
      &(nr_segment_message_params_t){
          .cloud_account_id = NULL,
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test null cloud_account_id",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /* Test empty cloud_account_id */
  test_message_segment(
      &(nr_segment_message_params_t){
          .cloud_account_id = "",
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test empty cloud_account_id",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /* Test valid cloud_account_id */
  test_message_segment(
      &(nr_segment_message_params_t){
          .cloud_account_id = "12345678",
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test valid cloud_account_id",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = "12345678",
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});
}

static void test_segment_message_messaging_system(void) {
  /*
   * messaging_system values should NOT impact the creation
   * of metrics.
   */

  /* Test null messaging_system */
  test_message_segment(
      &(nr_segment_message_params_t){
          .messaging_system = NULL,
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test null messaging_system",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /* Test empty messaging_system */
  test_message_segment(
      &(nr_segment_message_params_t){
          .messaging_system = "",
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test empty messaging_system",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /* Test valid messaging_system */
  test_message_segment(
      &(nr_segment_message_params_t){
          .messaging_system = "my_messaging_system",
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test valid messaging_system",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = "my_messaging_system",
          .cloud_resource_id = NULL,
          .server_address = NULL});
}

static void test_segment_message_cloud_resource_id(void) {
  /*
   * cloud_resource_id values should NOT impact the creation
   * of metrics.
   */

  /* Test null cloud_resource_id */
  test_message_segment(
      &(nr_segment_message_params_t){
          .cloud_resource_id = NULL,
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test null cloud_resource_id ",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /* Test empty cloud_resource_id */
  test_message_segment(
      &(nr_segment_message_params_t){
          .cloud_resource_id = "",
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test empty cloud_resource_id ",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /* Test valid cloud_resource_id */
  test_message_segment(
      &(nr_segment_message_params_t){
          .cloud_resource_id = "my_resource_id",
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test valid cloud_resource_id ",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = "my_resource_id",
          .server_address = NULL});
}

static void test_segment_message_server_address(void) {
  /*
   * server_address values should NOT impact the creation
   * of metrics.
   */

  /* Test null server_address */
  test_message_segment(
      &(nr_segment_message_params_t){
          .server_address = "localhost",
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test null server_address",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = "localhost"});

  /* Test empty server_address */
  test_message_segment(
      &(nr_segment_message_params_t){
          .server_address = "",
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test empty server_address",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});

  /* Test valid server_address */
  test_message_segment(
      &(nr_segment_message_params_t){
          .server_address = "localhost",
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test valid server_address",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = "localhost"});
}

static void test_segment_message_parameters_enabled(void) {
  /*
   * Attributes should be set based on value of parameters_enabled.
   */

  /* Test true message_parameters_enabled */
  test_message_segment(
      &(nr_segment_message_params_t){
          .server_address = "localhost",
          .cloud_region = "wild-west-1",
          .cloud_account_id = "12345678",
          .messaging_system = "my_system",
          .cloud_resource_id = "my_resource_id",
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test true message_parameters_enabled",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = "wild-west-1",
          .cloud_account_id = "12345678",
          .messaging_system = "my_system",
          .cloud_resource_id = "my_resource_id",
          .server_address = "localhost"});

  /* Test false message_parameters_enabled */
  test_message_segment(
      &(nr_segment_message_params_t){
          .server_address = "localhost",
          .cloud_region = "wild-west-1",
          .cloud_account_id = "12345678",
          .messaging_system = "my_system",
          .cloud_resource_id = "my_resource_id",
          .library = "SQS",
          .message_action = NR_SPAN_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      false /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test false message_parameters_enabled",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL});
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 4, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_bad_parameters();
  test_segment_message_destination_type();
  test_segment_message_message_action();
  test_segment_message_library();
  test_segment_message_destination_name();
  test_segment_message_cloud_region();
  test_segment_message_cloud_account_id();
  test_segment_message_messaging_system();
  test_segment_message_cloud_resource_id();
  test_segment_message_server_address();
  test_segment_message_parameters_enabled();
}
