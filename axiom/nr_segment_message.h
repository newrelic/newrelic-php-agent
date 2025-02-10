/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_SEGMENT_MESSAGE_HDR
#define NR_SEGMENT_MESSAGE_HDR

#include "nr_segment.h"
#include "nr_segment_traces.h"

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
  /* All strings are null-terminated. When NULL/empty the values are ingored. */

  /* Only used for creating metrics. */
  char* library; /* Library; Possible values are SQS, SNS, RabbitMQ, JMS */
  nr_segment_message_destination_type_t
      destination_type; /* Named/temp queue/topic/exchange */

  /* Used for creating message attributes. */
  nr_span_spankind_t
      message_action;     /*The action of the message, e.g.,Produce/Consume.*/
  char* destination_name; /*The name of the Queue, Topic, or Exchange;
                                otherwise, Temp. Needed for SQS relationship.*/
  char* messaging_system; /* for ex: aws_sqs. Needed for SQS relationship.*/
  char* server_address;   /*The server domain name or IP address.  Needed for
                             MQBROKER relationship.*/
  char*
      messaging_destination_publish_name;  /* Otel attribute for message
                                              consumers. (In the agent, this
                                              means Action is Consume in the span
                                              name). This attribute is equal to
                                              the corresponding attribute
                                              messaging.destination.name from the
                                              producer. This attribute is needed
                                              for apps using RabbitMQ and it
                                              represents the exchange name.*/
  char* messaging_destination_routing_key; /* The routing key for a RabbitMQ
                                              operation.*/
  uint64_t server_port;                    /*The server port.*/

} nr_segment_message_params_t;

/*
 * Purpose : End a message segment and record metrics.
 *
 * Params  : 1. nr_segment_t** segment: Segment to apply message params to and end
 *           2. const nr_segment_message_params_t* params: params to apply to segment
 *
 * Returns: true on success.
 */
extern bool nr_segment_message_end(nr_segment_t** segment,
                                   const nr_segment_message_params_t* params);

#endif
