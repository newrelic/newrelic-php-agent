/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stdio.h>

#include "nr_header.h"
#include "nr_segment_message.h"
#include "nr_segment_private.h"
#include "util_strings.h"
#include "util_url.h"

/*
 * Purpose : Set all the typed message attributes on the segment.
 */
static void nr_segment_message_set_attrs(
    nr_segment_t* segment,
    const nr_segment_message_params_t* params,
    nrtxnopt_t options) {
  nr_segment_message_t message_attributes = {0};

  message_attributes.message_action = params->message_action;

  if (options.message_tracer_segment_parameters_enabled) {
    message_attributes.destination_name = params->destination_name;
    message_attributes.cloud_region = params->cloud_region;
    message_attributes.cloud_account_id = params->cloud_account_id;
    message_attributes.messaging_system = params->messaging_system;
    message_attributes.cloud_resource_id = params->cloud_resource_id;
    message_attributes.server_address = params->server_address;
  }

  nr_segment_set_message(segment, &message_attributes);
}

/*
 * Purpose : Create metrics for a completed message call and set the segment
 *           name.
 *
 * Metrics created during this call
 * ----------------------------------------------------------------------------------
 * MessageBroker/all                                                Unscoped
 * Always MessageBroker/{library}/all Scoped   Always
 *
 * Metrics created based on MessageBroker/all (in nr_txn_create_rollup_metrics)
 * ----------------------------------------------------------------------------------
 * MessageBroker/allWeb                                             Unscoped Web
 * MessageBroker/allOther                                           Unscoped
 * non-Web
 *
 * Segment name
 * -----------------------------------------------------------------------------------
 * MessageBroker/{library}/all Always
 * MessageBroker/{Library}/{DestinationType}/{Action}/Named/{DestinationName}
 * non-temp MessageBroker/{Library}/{DestinationType}/{Action}/Temp temp dest
 *
 *
 * These metrics are dictated by the spec located here:
 * https://source.datanerd.us/agents/agent-specs/blob/master/APIs/messaging.md#metrics
 * When the destination is temporary (such as a temporary queue, or a temporary
 * topic), the destination name MUST be omitted. The metric segment 'Named' MUST
 * be replaced with 'Temp'. The DestinationType segment SHOULD NOT contain
 * "Temporary". Thus, "Temporary " should be removed from the destination type
 * enum before metric use. Examples: MessageBroker/JMS/Queue/Produce/Temp,
 * MessageBroker/JMS/Topic/Produce/Temp
 *
 * Further note that for pull-style messaging, the transaction segment name MUST
 * be equal to the scoped metric name (e.g.,
 * MessageBroker/JMS/Queue/Produce/Named/SortQueue)
 *
 *
 * Params  : 1. The message segment.
 *           2. Message parameters
 *           3. Duration of the segment
 *
 * Returns : the scoped metric that was created.  Caller is responsible for
 * freeing this value.
 */

static char* nr_segment_message_create_metrics(
    nr_segment_t* segment,
    const nr_segment_message_params_t* message_params,
    nrtime_t duration) {
  const char* action_string = NULL;
  const char* destination_type_string = NULL;
  char* rollup_metric = NULL;
  char* scoped_metric = NULL;

  if (NULL == segment) {
    return NULL;
  }

  if (NULL == message_params || NULL == message_params->library) {
    return NULL;
  }

  /* Rollup metric.
   *
   * This has to be created on the transaction in order to create
   * MessageBroker/allWeb and MessageBroker/allOther and to calculate
   * messageDuration later on.
   */

  nrm_force_add(segment->txn->unscoped_metrics, "MessageBroker/all", duration);

  rollup_metric = nr_formatf("MessageBroker/%s/all", message_params->library);
  nrm_force_add(segment->txn->unscoped_metrics, rollup_metric, duration);
  nr_free(rollup_metric);

  /*
   * Note: although the concept of Temporary queues/topics is detailed in the
   * spec, in practice, we are unlikely to encounter it as it is currently only
   * meaningful with JMS (Java Message Service).  It is added here for adherence
   * with spec.
   */

  if (NR_SPAN_PRODUCER == message_params->message_action) {
    action_string = "Produce";
  } else if (NR_SPAN_CONSUMER == message_params->message_action) {
    action_string = "Consume";
  } else {
    action_string = "Unknown";
  }

  switch (message_params->destination_type) {
    case NR_MESSAGE_DESTINATION_TYPE_TEMP_QUEUE:
    case NR_MESSAGE_DESTINATION_TYPE_QUEUE:
      destination_type_string = "Queue";
      break;
    case NR_MESSAGE_DESTINATION_TYPE_TEMP_TOPIC:
    case NR_MESSAGE_DESTINATION_TYPE_TOPIC:
      destination_type_string = "Topic";
      break;
    case NR_MESSAGE_DESTINATION_TYPE_EXCHANGE:
      destination_type_string = "Exchange";
      break;
    default:
      destination_type_string = "Unknown";
      break;
  }
  /*
   * Create the scoped metric
   * MessageBroker/{Library}/{DestinationType}/{Action}/Named/{DestinationName}
   * non-temp MessageBroker/{Library}/{DestinationType}/{Action}/Temp temp dest
   */
  if (NR_MESSAGE_DESTINATION_TYPE_TEMP_QUEUE == message_params->destination_type
      || NR_MESSAGE_DESTINATION_TYPE_TEMP_TOPIC
             == message_params->destination_type) {
    scoped_metric
        = nr_formatf("MessageBroker/%s/%s/%s/Temp", message_params->library,
                     destination_type_string, action_string);
  } else {
    scoped_metric = nr_formatf(
        "MessageBroker/%s/%s/%s/Named/%s", message_params->library,
        destination_type_string, action_string,
        message_params->destination_name ? message_params->destination_name
                                         : "Unknown");
  }

  nr_segment_add_metric(segment, scoped_metric, true);

  /*
   * The scoped metric will be used as the segment name.
   */
  return scoped_metric;
}

bool nr_segment_message_end(nr_segment_t** segment_ptr,
                            const nr_segment_message_params_t* message_params) {
  bool rv = false;
  nr_segment_t* segment;
  nrtime_t duration = 0;
  char* scoped_metric = NULL;

  if (NULL == segment_ptr) {
    return false;
  }

  segment = *segment_ptr;

  if (NULL == segment || NULL == message_params || NULL == segment->txn) {
    return false;
  }

  /*
   * We don't want message segments to have any children, as
   * this would scramble the exclusive time calculation.
   * Additionally, because it makes http calls under the hood,
   * we don't want additional external calls created for this same txn.
   * Therefore, we delete all children of the message segment.
   */
  if (segment) {
    for (size_t i = 0; i < nr_segment_children_size(&segment->children); i++) {
      nr_segment_t* child = nr_segment_children_get(&segment->children, i);
      nr_segment_discard(&child);
    }
  }

  nr_segment_message_set_attrs(segment, message_params, segment->txn->options);

  /*
   * We set the end time here because we need the duration, (nr_segment_end will
   * not overwrite this value if it's already set).
   */
  if (!segment->stop_time) {
    segment->stop_time
        = nr_time_duration(nr_txn_start_time(segment->txn), nr_get_time());
  }
  duration = nr_time_duration(segment->start_time, segment->stop_time);

  scoped_metric
      = nr_segment_message_create_metrics(segment, message_params, duration);
  nr_segment_set_name(segment, scoped_metric);

  rv = nr_segment_end(&segment);

  nr_free(scoped_metric);

  return rv;
}