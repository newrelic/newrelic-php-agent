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

#define MAJOR_VERSION_LENGTH 8

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
    const char* package_version,
    nr_php_package_t* p) {
  if (NULL == txn || NULL == package_name) {
    return;
  }

  char* metname = NULL;
  char major_version[MAJOR_VERSION_LENGTH] = {0};
  const char* version = package_version;

  // override provided package_version only if:
  // - php_package is provided
  // - its version is not NULL
  // - its version is not PHP_PACKAGE_VERSION_UNKNOWN
  if (NULL != p && NULL != p->package_version
      && 0 != nr_strcmp(p->package_version, PHP_PACKAGE_VERSION_UNKNOWN)) {
    version = p->package_version;
  }

  // only generate metric if version is known
  if (NULL == version || 0 == nr_strcmp(version, PHP_PACKAGE_VERSION_UNKNOWN)) {
    return;
  }

  /* The below for loop checks if the major version of the package is more than
   * one digit and keeps looping until a '.' is encountered or one of the
   * conditions is met.
   */
  for (int i = 0; version[i] && i < MAJOR_VERSION_LENGTH - 1; i++) {
    if ('.' == version[i]) {
      break;
    }
    major_version[i] = version[i];
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
