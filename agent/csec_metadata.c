/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
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

int nr_php_csec_get_metadata(nr_php_csec_metadata_t* csec_metadata) {
  if (NULL == csec_metadata) {
    return -1;
  }

  if (NULL == NRPRG(app)) {
    return -2;
  }

  csec_metadata->high_security = NRPRG(app)->info.high_security;
  csec_metadata->entity_name = nr_strdup(nr_app_get_entity_name(NRPRG(app)));
  csec_metadata->entity_type = nr_strdup(nr_app_get_entity_type(NRPRG(app)));
  csec_metadata->entity_guid = nr_strdup(nr_app_get_entity_guid(NRPRG(app)));
  csec_metadata->host_name = nr_strdup(nr_app_get_host_name(NRPRG(app)));
  csec_metadata->agent_run_id = nr_strdup(NRPRG(app)->agent_run_id);
  csec_metadata->account_id = nr_strdup(NRPRG(app)->account_id);
  csec_metadata->license = nr_strdup(NRPRG(license).value);
  csec_metadata->plicense = nr_strdup(NRPRG(app)->plicense);

  return 0;
}
