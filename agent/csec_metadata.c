/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "csec_metadata.h"

#include "util_memory.h"

#include "nr_axiom.h"
#include "nr_agent.h"
#include "nr_app.h"
#include "php_includes.h"
#include "php_compat.h"
#include "php_newrelic.h"
#include "util_strings.h"

int nr_php_csec_get_metadata(const nr_php_csec_metadata_key_t key, char** p) {
  const char* value = NULL;
  char* id_value = NULL;
  bool is_allocated = false;

  if (NULL == p) {
    return -1;
  }

  if (NULL == NRPRG(app) || NULL == NRPRG(txn)) {
    return -2;
  }

  switch (key) {
    case NR_PHP_CSEC_METADATA_HIGH_SECURITY:
      if (NRPRG(app)->info.high_security) {
        value = "true";
      } else {
        value = "false";
      }
      break;
    case NR_PHP_CSEC_METADATA_ENTITY_NAME:
      value = nr_app_get_entity_name(NRPRG(app));
      break;
    case NR_PHP_CSEC_METADATA_ENTITY_TYPE:
      value = nr_app_get_entity_type(NRPRG(app));
      break;
    case NR_PHP_CSEC_METADATA_ENTITY_GUID:
      value = nr_app_get_entity_guid(NRPRG(app));
      break;
    case NR_PHP_CSEC_METADATA_HOST_NAME:
      value = nr_app_get_host_name(NRPRG(app));
      break;
    case NR_PHP_CSEC_METADATA_AGENT_RUN_ID:
      value = NRPRG(app)->agent_run_id;
      break;
    case NR_PHP_CSEC_METADATA_ACCOUNT_ID:
      value = NRPRG(app)->account_id;
      break;
    case NR_PHP_CSEC_METADATA_LICENSE:
      value = NRPRG(license).value;
      break;
    case NR_PHP_CSEC_METADATA_PLICENSE:
      value = NRPRG(app)->plicense;
      break;
    case NR_PHP_CSEC_METADATA_TRACE_ID:
      id_value = nr_txn_get_current_trace_id(NRPRG(txn));
      is_allocated = true;
      break;
    case NR_PHP_CSEC_METADATA_SPAN_ID:
      id_value = nr_txn_get_current_span_id(NRPRG(txn));
      is_allocated = true;
      break;
    default:
      return -4;
  }

  if (!is_allocated && NULL == value) {
    return -5;
  }

  if (is_allocated) {
    *p = id_value;
  } else {
    *p = nr_strdup(value);
  }

  if (NULL == *p) {
    return -3;
  }
  return 0;
}
