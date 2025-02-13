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
#include "util_logging.h"

/*
 * Purpose : Set all the typed message attributes on the segment.
 *
 * Params  : 1. nr_segment_t* ASSUMED TO BE NON-NULL - the segment to set the
 * attributes on
 *           2. nr_segment_message_params_t* ASSUMED TO BE NON-NULL - the
 * parameters set the attributes to
 *
 * Returns: true on success.
 *
 * Note: This is a function private to this file and assumes the calling
 * function has already checked the input parameters for NULL prior to calling
 * this function.  Calling function is assumed to check the following items for
 * NULL: if (NULL == segment || NULL == message_params || NULL == segment->txn)
 */
static void nr_segment_message_set_attrs(
    nr_segment_t* segment,
    const nr_segment_message_params_t* params) {
  nr_segment_message_t message_attributes = {0};

  message_attributes.message_action = params->message_action;

  if (segment->txn->options.message_tracer_segment_parameters_enabled) {
    message_attributes.destination_name = params->destination_name;
    message_attributes.messaging_system = params->messaging_system;
    message_attributes.server_address = params->server_address;
    message_attributes.messaging_destination_routing_key
        = params->messaging_destination_routing_key;
    message_attributes.messaging_destination_publish_name
        = params->messaging_destination_publish_name;
    message_attributes.server_port = params->server_port;
  }

  nr_segment_set_message(segment, &message_attributes);
}

/*
 * Purpose : Create metrics for a completed message call and set the segment
 *           name.
 *
 * Metrics created during this call
 * ----------------------------------------------------------------------------------
 * MessageBroker/all                                         Unscoped Always
 * MessageBroker/{library}/all                               Scoped   Always
 *
 * Metrics created based on MessageBroker/all (in nr_txn_create_rollup_metrics)
 * ----------------------------------------------------------------------------------
 * MessageBroker/allWeb                                  Unscoped Web
 * MessageBroker/allOther                                Unscoped non-Web
 *
 * Segment name
 * -----------------------------------------------------------------------------------
 * MessageBroker/{library}/all Always
 * For non-temp:
 * MessageBroker/{Library}/{DestinationType}/{Action}/Named/{DestinationName}
 * For temp:
 * MessageBroker/{Library}/{DestinationType}/{Action}/Temp
 *
 *
 * These metrics are dictated by the agent-spec file here:
 * APIs/messaging.md#metrics
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
  const char* library_string = NULL;
  const char* final_destination_string = NULL;
  const char* destination_string = NULL;
  char* rollup_metric = NULL;
  char* scoped_metric = NULL;

  if (NULL == segment) {
    return NULL;
  }

  if (NULL == message_params) {
    return NULL;
  }

  /* Rollup metric.
   *
   * This has to be created on the transaction in order to create
   * MessageBroker/allWeb and MessageBroker/allOther and to calculate
   * messageDuration later on.
   */

  nrm_force_add(segment->txn->unscoped_metrics, "MessageBroker/all", duration);

  if (nr_strempty(message_params->library)) {
    library_string = "<unknown>";
  } else {
    library_string = message_params->library;
  }
  rollup_metric = nr_formatf("MessageBroker/%s/all", library_string);
  nrm_force_add(segment->txn->unscoped_metrics, rollup_metric, duration);
  nr_free(rollup_metric);

  /*
   * Note: although the concept of Temporary queues/topics is detailed in the
   * spec, in practice, we are unlikely to encounter it as it is currently only
   * meaningful with JMS (Java Message Service).  It is added here for adherence
   * with spec.
   */

  if (NR_SPANKIND_PRODUCER == message_params->message_action) {
    action_string = "Produce";
  } else if (NR_SPANKIND_CONSUMER == message_params->message_action) {
    action_string = "Consume";
  } else {
    action_string = "<unknown>";
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
      destination_type_string = "<unknown>";
      break;
  }

  destination_string = nr_strempty(message_params->destination_name)
                           ? "<unknown>"
                           : message_params->destination_name;
  /*
   * messaging_destination_publish_name is only used if it exists; In all other
   * cases, we use the value from destination_string.
   */
  final_destination_string
      = nr_strempty(message_params->messaging_destination_publish_name)
            ? destination_string
            : message_params->messaging_destination_publish_name;

  /*
   * Create the scoped metric
   * MessageBroker/{Library}/{DestinationType}/{Action}/Named/{DestinationName}
   * non-temp MessageBroker/{Library}/{DestinationType}/{Action}/Temp
   */
  if (NR_MESSAGE_DESTINATION_TYPE_TEMP_QUEUE == message_params->destination_type
      || NR_MESSAGE_DESTINATION_TYPE_TEMP_TOPIC
             == message_params->destination_type) {
    scoped_metric = nr_formatf("MessageBroker/%s/%s/%s/Temp", library_string,
                               destination_type_string, action_string);
  } else {
    scoped_metric = nr_formatf("MessageBroker/%s/%s/%s/Named/%s",
                               library_string, destination_type_string,
                               action_string, final_destination_string);
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
  nr_segment_t* child = NULL;

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
   * By destroying the tree we are able to destroy all descendants vs just
   * destroying the child which then reparents all it's children to the segment.
   */
  if (segment) {
    for (size_t i = 0; i < nr_segment_children_size(&segment->children); i++) {
      child = nr_segment_children_get(&segment->children, i);
      nr_segment_destroy_tree(child);
    }
    nr_segment_children_deinit(&segment->children);
  }

  nr_segment_message_set_attrs(segment, message_params);

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
