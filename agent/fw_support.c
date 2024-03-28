/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_user_instrument.h"
#include "fw_support.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

void nr_php_framework_add_supportability_metric(const char* framework_name,
                                                const char* name TSRMLS_DC) {
  char buf[512];

  if (NULL == name) {
    return;
  }
  if (NULL == NRPRG(txn)) {
    return;
  }

  buf[0] = '\0';
  snprintf(buf, sizeof(buf), "Supportability/%s/%s", framework_name, name);

  nrm_force_add(NRPRG(txn) ? NRTXN(unscoped_metrics) : 0, buf, 0);
}

void nr_fw_support_add_library_supportability_metric(nrtxn_t* txn,
                                                     const char* library_name) {
  if (NULL == txn || NULL == library_name) {
    return;
  }

  char* metname
      = nr_formatf("Supportability/library/%s/detected", library_name);
  nrm_force_add(txn->unscoped_metrics, metname, 0);
  nr_free(metname);
}

void nr_fw_support_add_logging_supportability_metric(nrtxn_t* txn,
                                                     const char* library_name,
                                                     const bool is_enabled) {
  if (NULL == txn || NULL == library_name) {
    return;
  }

  char* metname = nr_formatf("Supportability/Logging/PHP/%s/%s", library_name,
                             is_enabled ? "enabled" : "disabled");
  nrm_force_add(txn->unscoped_metrics, metname, 0);
  nr_free(metname);
}

void nr_fw_support_add_package_supportability_metric(
    nrtxn_t* txn,
    const char* package_name,
    const char* package_version) {
  if (NULL == txn || NULL == package_name || NULL == package_version) {
    return;
  }

  char* metname = NULL;
  char major_version[4] = {0};

  /* The below for loop checks if the major version of the package is more than
   * one digit and keeps looping until a '.' is encountered or one of the
   * conditions is met.
   */
  for (int i = 0; package_version[i] && i < 4; i++) {
    if ('.' == package_version[i]) {
      strncpy(major_version, package_version, i);
      major_version[i] = '\0';
      break;
    }
  }

  if ('\0' == major_version[0]) {
    return;
  }

  if (NR_FW_UNSET == NRINI(force_framework)) {
    metname = nr_formatf("Supportability/PHP/package/%s/%s/detected",
                         package_name, major_version);
  } else {
    metname = nr_formatf("Supportability/PHP/package/%s/%s/forced",
                         package_name, major_version);
  }
  nrm_force_add(txn->unscoped_metrics, metname, 0);
  nr_free(metname);
}
