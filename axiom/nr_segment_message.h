/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_SEGMENT_MESSAGE_HDR
#define NR_SEGMENT_MESSAGE_HDR

#include "nr_segment.h"

/*
 * Note:
 * CAT is EOLed and this feature is not compatible with CAT.
 */

typedef enum _nr_segment_message_destination_type_t {
  NR_MESSAGE_DESTINATION_TYPE_QUEUE,
  NR_MESSAGE_DESTINATION_TYPE_TOPIC,
  NR_MESSAGE_DESTINATION_TYPE_TEMP_QUEUE,
  NR_MESSAGE_DESTINATION_TYPE_TEMP_TOPIC,
  NR_MESSAGE_DESTINATION_TYPE_EXCHANGE
} nr_segment_message_destination_type_t;

typedef struct {
  /* All strings are null-terminated. When unset, the strings are ingored. */

  /* Only used for creating metrics. */

  char* library; /* Library; Possible values are SQS, SNS, RabbitMQ, JMS */
  nr_segment_message_destination_type_t
      destination_type; /* Named/temp queue/topic/exchange */

  /* Used for creating message attributes. */
  nr_span_spankind_t
      message_action;      /*The action of the message, e.g.,Produce/Consume.*/
  char* destination_name;  /*The name of the Queue, Topic, or Exchange;
                                 otherwise, Temp. Needed for SQS relationship.*/
  char* cloud_region;      /*Targeted region; ex:us-east-1*. Needed for SQS
                              relationship.*/
  char* cloud_account_id;  /*The cloud provider account ID. Needed for SQS
                              relationship.*/
  char* messaging_system;  /* for ex: aws_sqs. Needed for SQS relationship.*/
  char* cloud_resource_id; /*A unique identifier given by the cloud resource.
                              For AWS, this is the ARN of the AWS resource being
                              accessed.*/
  char* server_address;    /*The server domain name or IP address.  Needed for
                              MQBROKER relationship.*/
  char* aws_operation;     /*The AWS operation being called.*/

} nr_segment_message_params_t;

static inline void nr_segment_message_destroy_message_params(
    nr_segment_message_params_t* message_params) {
  nr_free(message_params->library);
  nr_free(message_params->destination_name);
  nr_free(message_params->cloud_region);
  nr_free(message_params->cloud_account_id);
  nr_free(message_params->messaging_system);
  nr_free(message_params->cloud_resource_id);
  nr_free(message_params->server_address);
  nr_free(message_params->aws_operation);
}

/*
 * Purpose : End a message segment and record metrics.
 *
 * Params  : 1. nr_segment_message_params_t
 *
 * Returns: true on success.
 */
extern bool nr_segment_message_end(nr_segment_t** segment,
                                   const nr_segment_message_params_t* params);

#endif
