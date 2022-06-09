/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains the agent's view of the appinfo command:
 * it is used by agents to query the daemon about the status of applications.
 */
#include "nr_axiom.h"

#include <errno.h>
#include <stddef.h>

#include "nr_agent.h"
#include "nr_commands.h"
#include "nr_commands_private.h"
#include "nr_limits.h"
#include "nr_rules.h"
#include "util_buffer.h"
#include "util_errno.h"
#include "util_flatbuffers.h"
#include "util_labels.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_network.h"
#include "util_reply.h"

uint64_t nr_cmd_appinfo_timeout_us = 100 * NR_TIME_DIVISOR_MS;

/*
 * Send the labels to the daemon in the format expected by the
 * collector in the connect command.
 */
static uint32_t nr_appinfo_prepend_labels(const nr_app_info_t* info,
                                          nr_flatbuffer_t* fb) {
  char* json;
  nrobj_t* labels;
  uint32_t offset;

  if ((NULL == info) || (NULL == info->labels)) {
    return 0;
  }

  labels = nr_labels_connector_format(info->labels);
  json = nro_to_json(labels);
  offset = nr_flatbuffers_prepend_string(fb, json);
  nro_delete(labels);
  nr_free(json);

  return offset;
}

static uint32_t nr_appinfo_prepend_settings(const nr_app_info_t* info,
                                            nr_flatbuffer_t* fb) {
  char* json;
  uint32_t offset;

  if ((NULL == info) || (NULL == info->settings)) {
    return 0;
  }

  json = nro_to_json(info->settings);
  offset = nr_flatbuffers_prepend_string(fb, json);
  nr_free(json);

  return offset;
}

/*
 * Send the metadata to the daemon in the format expected by the
 * collector in the connect command.
 */

static uint32_t nr_appinfo_prepend_metadata(const nr_app_info_t* info,
                                            nr_flatbuffer_t* fb) {
  char* json;
  uint32_t offset;

  if ((NULL == info) || (NULL == info->metadata)) {
    return 0;
  }
  json = nro_to_json(info->metadata);
  offset = nr_flatbuffers_prepend_string(fb, json);
  nr_free(json);

  return offset;
}

static nr_status_t convert_appenv(const char* key,
                                  const nrobj_t* val,
                                  void* ptr) {
  nrobj_t* envarray = (nrobj_t*)ptr;
  nrobj_t* entry = nro_new_array();

  nro_set_array_string(entry, 1, key);
  nro_set_array(entry, 2, val);

  nro_set_array(envarray, 0, entry);
  nro_delete(entry);

  return NR_SUCCESS;
}

/*
 * Send the environment to the daemon in the format expected by the
 * collector in the connect command.
 */
static uint32_t nr_appinfo_prepend_env(const nr_app_info_t* info,
                                       nr_flatbuffer_t* fb) {
  char* json;
  nrobj_t* env;
  uint32_t offset;

  if ((NULL == info) || (NULL == info->environment)) {
    return 0;
  }

  env = nro_new_array();
  nro_iteratehash(info->environment, convert_appenv, env);
  json = nro_to_json(env);
  offset = nr_flatbuffers_prepend_string(fb, json);
  nro_delete(env);
  nr_free(json);

  return offset;
}

nr_flatbuffer_t* nr_appinfo_create_query(const char* agent_run_id,
                                         const char* system_host_name,
                                         const nr_app_info_t* info) {
  nr_flatbuffer_t* fb;
  uint32_t display_host;
  uint32_t labels;
  uint32_t settings;
  uint32_t env;
  uint32_t license;
  uint32_t appname;
  uint32_t agent_lang;
  uint32_t agent_version;
  uint32_t collector;
  uint32_t appinfo;
  uint32_t agent_run_id_offset;
  uint32_t message;
  uint32_t security_policy_token;
  uint32_t supported_security_policies;
  uint32_t host_name;
  uint32_t trace_observer_host;
  uint32_t metadata;
  char* json_supported_security_policies;

  fb = nr_flatbuffers_create(0);

  display_host = nr_flatbuffers_prepend_string(fb, info->host_display_name);
  labels = nr_appinfo_prepend_labels(info, fb);
  settings = nr_appinfo_prepend_settings(info, fb);
  env = nr_appinfo_prepend_env(info, fb);
  collector = nr_flatbuffers_prepend_string(fb, info->redirect_collector);
  agent_version = nr_flatbuffers_prepend_string(fb, info->version);
  agent_lang = nr_flatbuffers_prepend_string(fb, info->lang);
  appname = nr_flatbuffers_prepend_string(fb, info->appname);
  license = nr_flatbuffers_prepend_string(fb, info->license);
  security_policy_token
      = nr_flatbuffers_prepend_string(fb, info->security_policies_token);
  host_name = nr_flatbuffers_prepend_string(fb, system_host_name);
  trace_observer_host
      = nr_flatbuffers_prepend_string(fb, info->trace_observer_host);

  json_supported_security_policies
      = nro_to_json(info->supported_security_policies);
  supported_security_policies
      = nr_flatbuffers_prepend_string(fb, json_supported_security_policies);

  metadata = nr_appinfo_prepend_metadata(info, fb);

  nr_flatbuffers_object_begin(fb, APP_NUM_FIELDS);
  nr_flatbuffers_object_prepend_u64(fb, APP_SPAN_QUEUE_SIZE,
                                    info->span_queue_size, 0);
  nr_flatbuffers_object_prepend_u64(fb, APP_SPAN_EVENTS_MAX_SAMPLES_STORED,
                                    info->span_events_max_samples_stored, 0);
  nr_flatbuffers_object_prepend_u16(fb, APP_TRACE_OBSERVER_PORT,
                                    info->trace_observer_port, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_TRACE_OBSERVER_HOST,
                                        trace_observer_host, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_HOST, host_name, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_SUPPORTED_SECURITY_POLICIES,
                                        supported_security_policies, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_METADATA, metadata, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_SECURITY_POLICY_TOKEN,
                                        security_policy_token, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_DISPLAY_HOST, display_host, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_FIELD_LABELS, labels, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_FIELD_SETTINGS, settings, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_FIELD_ENVIRONMENT, env, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_FIELD_REDIRECT_COLLECTOR,
                                        collector, 0);
  nr_flatbuffers_object_prepend_bool(fb, APP_FIELD_HIGH_SECURITY,
                                     info->high_security, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_FIELD_AGENT_VERSION,
                                        agent_version, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_FIELD_AGENT_LANGUAGE,
                                        agent_lang, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_FIELD_APPNAME, appname, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_FIELD_LICENSE, license, 0);
  appinfo = nr_flatbuffers_object_end(fb);

  if (agent_run_id && *agent_run_id) {
    agent_run_id_offset = nr_flatbuffers_prepend_string(fb, agent_run_id);
  } else {
    agent_run_id_offset = 0;
  }

  nr_flatbuffers_object_begin(fb, MESSAGE_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, MESSAGE_FIELD_DATA, appinfo, 0);
  nr_flatbuffers_object_prepend_u8(fb, MESSAGE_FIELD_DATA_TYPE,
                                   MESSAGE_BODY_APP, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, MESSAGE_FIELD_AGENT_RUN_ID,
                                        agent_run_id_offset, 0);
  message = nr_flatbuffers_object_end(fb);

  nr_flatbuffers_finish(fb, message);

  nr_free(json_supported_security_policies);

  return fb;
}

int nr_command_is_flatbuffer_invalid(nr_flatbuffer_t* msg, size_t msglen) {
  size_t offset = nr_flatbuffers_read_uoffset(nr_flatbuffers_data(msg), 0);

  if (msglen - MIN_FLATBUFFER_SIZE <= offset) {
    nrl_verbosedebug(NRL_DAEMON, "flatbuffer offset is too large, len=%zu",
                     offset);
    return 1;
  }

  return 0;
}

void nr_cmd_appinfo_process_harvest_timing(nr_flatbuffers_table_t* reply,
                                           nrapp_t* app) {
  nrtime_t connect_timestamp;
  nrtime_t harvest_frequency;
  uint16_t sampling_target;

  /* Try to read the connect timestamp. If it's unavailable, we need to call
   * nr_get_time(), but we don't want to do that as the default value for
   * nr_flatbuffers_table_read_u64() because it's potentially costly on systems
   * with slow gettimeofday() implementations, and is unnecessary in the normal
   * case where the daemon version is the same as the agent version. */
  connect_timestamp = nr_flatbuffers_table_read_u64(
      reply, APP_REPLY_FIELD_CONNECT_TIMESTAMP, 0);
  if (0 == connect_timestamp) {
    connect_timestamp = nr_get_time();
  } else {
    connect_timestamp *= NR_TIME_DIVISOR;
  }

  /* Try to get the harvest frequency. Unlike the above case, the default here
   * is always 60 seconds. */
  harvest_frequency = nr_flatbuffers_table_read_u16(
      reply, APP_REPLY_FIELD_HARVEST_FREQUENCY, 60);

  /* Try to get the sampling_target. The default here is 10 seconds. */
  sampling_target = nr_flatbuffers_table_read_u16(
      reply, APP_REPLY_FIELD_SAMPLING_TARGET, 10);

  nr_app_harvest_init(&app->harvest, connect_timestamp,
                      harvest_frequency * NR_TIME_DIVISOR, sampling_target);
}

nr_status_t nr_cmd_appinfo_process_reply(const uint8_t* data,
                                         int len,
                                         nrapp_t* app) {
  nr_flatbuffers_table_t msg;
  nr_flatbuffers_table_t reply;
  int data_type;
  int status;
  int reply_len;
  const char* reply_json;
  const char* entity_guid;

  if ((NULL == data) || (0 == len)) {
    return NR_FAILURE;
  }
  if (NULL == app) {
    return NR_FAILURE;
  }

  nr_flatbuffers_table_init_root(&msg, data, len);

  data_type = nr_flatbuffers_table_read_u8(&msg, MESSAGE_FIELD_DATA_TYPE,
                                           MESSAGE_BODY_NONE);
  if (MESSAGE_BODY_APP_REPLY != data_type) {
    nrl_error(NRL_ACCT, "unexpected message type, data_type=%d", data_type);
    return NR_FAILURE;
  }

  if (0 == nr_flatbuffers_table_read_union(&reply, &msg, MESSAGE_FIELD_DATA)) {
    nrl_error(NRL_ACCT, "APPINFO reply missing a body");
    return NR_FAILURE;
  }

  status = nr_flatbuffers_table_read_i8(&reply, APP_REPLY_FIELD_STATUS,
                                        APP_STATUS_UNKNOWN);

  switch (status) {
    case APP_STATUS_UNKNOWN:
      app->state = NR_APP_UNKNOWN;
      nrl_debug(NRL_ACCT, "APPINFO reply unknown app=" NRP_FMT,
                NRP_APPNAME(app->info.appname));
      return NR_SUCCESS;
    case APP_STATUS_DISCONNECTED:
      app->state = NR_APP_INVALID;
      nrl_info(NRL_ACCT, "APPINFO reply disconnected app=" NRP_FMT,
               NRP_APPNAME(app->info.appname));
      return NR_SUCCESS;
    case APP_STATUS_INVALID_LICENSE:
      app->state = NR_APP_INVALID;
      nrl_error(NRL_ACCT,
                "APPINFO reply invalid license app=" NRP_FMT
                " please check your license "
                "key and restart your web server.",
                NRP_APPNAME(app->info.appname));
      return NR_SUCCESS;
    case APP_STATUS_CONNECTED:
      /* Don't return here, instead break and continue below. */
      nrl_debug(NRL_ACCT, "APPINFO reply connected");
      break;
    case APP_STATUS_STILL_VALID:
      app->state = NR_APP_OK;
      nrl_debug(NRL_ACCT, "APPINFO reply agent run id still valid app='%.*s'",
                NRP_APPNAME(app->info.appname));
      return NR_SUCCESS;
    default:
      nrl_error(NRL_ACCT, "APPINFO reply has unknown status status=%d", status);
      return NR_FAILURE;
  }

  /*
   * Connected: Full app reply
   */
  reply_len = (int)nr_flatbuffers_table_read_vector_len(
      &reply, APP_REPLY_FIELD_CONNECT_REPLY);
  reply_json = (const char*)nr_flatbuffers_table_read_bytes(
      &reply, APP_REPLY_FIELD_CONNECT_REPLY);

  nro_delete(app->connect_reply);
  app->connect_reply = nro_create_from_json_unterminated(reply_json, reply_len);

  if (NULL == app->connect_reply) {
    nrl_error(NRL_ACCT, "APPINFO reply bad connect reply: len=%d json=%p",
              reply_len, reply_json);
    return NR_FAILURE;
  }

  nr_free(app->agent_run_id);
  app->agent_run_id = nr_strdup(
      nro_get_hash_string(app->connect_reply, "agent_run_id", NULL));
  app->state = NR_APP_OK;
  nr_rules_destroy(&app->url_rules);
  app->url_rules = nr_rules_create_from_obj(
      nro_get_hash_array(app->connect_reply, "url_rules", 0));
  nr_rules_destroy(&app->txn_rules);
  app->txn_rules = nr_rules_create_from_obj(
      nro_get_hash_array(app->connect_reply, "transaction_name_rules", 0));
  nr_segment_terms_destroy(&app->segment_terms);
  app->segment_terms = nr_segment_terms_create_from_obj(
      nro_get_hash_array(app->connect_reply, "transaction_segment_terms", 0));

  nr_free(app->entity_guid);
  entity_guid = nro_get_hash_string(app->connect_reply, "entity_guid", NULL);
  if (NULL != entity_guid) {
    app->entity_guid = nr_strdup(entity_guid);
  } else {
    app->entity_guid = NULL;
  }

  nrl_debug(NRL_ACCT, "APPINFO reply full app='%.*s' agent_run_id=%s",
            NRP_APPNAME(app->info.appname), app->agent_run_id);

  /*
   * Grab security policies (empty hash when non-LASP).
   */

  reply_len = (int)nr_flatbuffers_table_read_vector_len(
      &reply, APP_REPLY_FIELD_SECURITY_POLICIES);
  reply_json = (const char*)nr_flatbuffers_table_read_bytes(
      &reply, APP_REPLY_FIELD_SECURITY_POLICIES);

  nro_delete(app->security_policies);
  app->security_policies
      = nro_create_from_json_unterminated(reply_json, reply_len);

  /*
   * Disable any event types the backend is uninterested in.
   */
  nr_cmd_appinfo_process_event_harvest_config(
      nro_get_hash_hash(app->connect_reply, "event_harvest_config", NULL),
      &app->limits, app->info);

  /*
   * Finally, handle the harvest timing information.
   */
  nr_cmd_appinfo_process_harvest_timing(&reply, app);

  return NR_SUCCESS;
}

void nr_cmd_appinfo_process_event_harvest_config(const nrobj_t* config,
                                                 nr_app_limits_t* app_limits,
                                                 nr_app_info_t info) {
  const nrobj_t* harvest_limits
      = nro_get_hash_hash(config, "harvest_limits", NULL);

  /* At the per-transaction agent level, the actual limits are only really
   * meaningful for custom and span events: the other event types generally only
   * result in one event per transaction, so we really just need to know if the
   * event type is enabled at all. We'll still cache the limit values for
   * consistency, but the defaults are more or less inconsequential. */
  app_limits->analytics_events = nr_cmd_appinfo_process_get_harvest_limit(
      harvest_limits, "analytic_event_data", NR_MAX_ANALYTIC_EVENTS);
  app_limits->custom_events = nr_cmd_appinfo_process_get_harvest_limit(
      harvest_limits, "custom_event_data", NR_MAX_CUSTOM_EVENTS);
  app_limits->error_events = nr_cmd_appinfo_process_get_harvest_limit(
      harvest_limits, "error_event_data", NR_MAX_ERRORS);
  app_limits->span_events = nr_cmd_appinfo_process_get_harvest_limit(
      harvest_limits, "span_event_data",
      0 == info.span_events_max_samples_stored
          ? NR_MAX_SPAN_EVENTS_MAX_SAMPLES_STORED
          : info.span_events_max_samples_stored);
}

int nr_cmd_appinfo_process_get_harvest_limit(const nrobj_t* limits,
                                             const char* key,
                                             int default_value) {
  int limit;
  nr_status_t status = NR_FAILURE;

  limit = nro_get_hash_int(limits, key, &status);
  return NR_SUCCESS == status ? limit : default_value;
}

/* Hook for stubbing APPINFO messages during testing. */
nr_status_t (*nr_cmd_appinfo_hook)(int daemon_fd, nrapp_t* app) = NULL;

nr_status_t nr_cmd_appinfo_tx(int daemon_fd, nrapp_t* app) {
  nr_flatbuffer_t* query;
  nrbuf_t* buf = NULL;
  nrtime_t deadline;
  nr_status_t st;
  size_t querylen;

  if (nr_cmd_appinfo_hook) {
    return nr_cmd_appinfo_hook(daemon_fd, app);
  }

  if (NULL == app) {
    return NR_FAILURE;
  }
  if (daemon_fd < 0) {
    return NR_FAILURE;
  }

  app->state = NR_APP_UNKNOWN;
  nrl_verbosedebug(NRL_DAEMON, "querying app=" NRP_FMT " from parent=%d",
                   NRP_APPNAME(app->info.appname), daemon_fd);

  query
      = nr_appinfo_create_query(app->agent_run_id, app->host_name, &app->info);
  querylen = nr_flatbuffers_len(query);

  nrl_verbosedebug(NRL_DAEMON, "sending appinfo message, len=%zu", querylen);

  if (nr_command_is_flatbuffer_invalid(query, querylen)) {
    nr_flatbuffers_destroy(&query);
    return NR_FAILURE;
  }

  deadline = nr_get_time() + nr_cmd_appinfo_timeout_us;

  nr_agent_lock_daemon_mutex();
  {
    st = nr_write_message(daemon_fd, nr_flatbuffers_data(query), querylen,
                          deadline);
    if (NR_SUCCESS == st) {
      buf = nr_network_receive(daemon_fd, deadline);
    }
  }
  nr_agent_unlock_daemon_mutex();

  nr_flatbuffers_destroy(&query);
  st = nr_cmd_appinfo_process_reply((const uint8_t*)nr_buffer_cptr(buf),
                                    nr_buffer_len(buf), app);
  nr_buffer_destroy(&buf);

  if (NR_SUCCESS != st) {
    app->state = NR_APP_UNKNOWN;
    nrl_error(NRL_DAEMON, "APPINFO failure: len=%zu errno=%s", querylen,
              nr_errno(errno));
    nr_agent_close_daemon_connection();
  }

  return st;
}
