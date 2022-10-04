/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "fw_support.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static void test_fw_supportability_metrics(void) {
#define LIBRARY_NAME "php-package"
#define LIBRARY_METRIC "Supportability/library/" LIBRARY_NAME "/detected"
#define LOGGING_LIBRARY_METRIC "Supportability/Logging/PHP/" LIBRARY_NAME
  nrtxn_t t;
  nrtxn_t* txn = &t;
  txn->unscoped_metrics = nrm_table_create(10);

  /* NULL tests - don't blow up */
  nr_fw_support_add_library_supportability_metric(NULL, LIBRARY_NAME);
  tlib_pass_if_int_equal("library metric not created in NULL metrics", 0,
                         nrm_table_size(txn->unscoped_metrics));

  nr_fw_support_add_library_supportability_metric(txn, NULL);
  tlib_pass_if_int_equal("NULL library metric not created", 0,
                         nrm_table_size(txn->unscoped_metrics));

  nr_fw_support_add_logging_supportability_metric(NULL, LIBRARY_NAME, true);
  tlib_pass_if_int_equal("logging library metric not created in NULL metrics",
                         0, nrm_table_size(txn->unscoped_metrics));

  nr_fw_support_add_logging_supportability_metric(txn, NULL, true);
  tlib_pass_if_int_equal("NULL logging library metric not created", 0,
                         nrm_table_size(txn->unscoped_metrics));

  /* Happy path */
  nr_fw_support_add_library_supportability_metric(txn, LIBRARY_NAME);
  tlib_pass_if_not_null("happy path: library metric created",
                        nrm_find(txn->unscoped_metrics, LIBRARY_METRIC));

  nr_fw_support_add_logging_supportability_metric(txn, LIBRARY_NAME, true);
  tlib_pass_if_not_null(
      "happy path: logging library metric created",
      nrm_find(txn->unscoped_metrics, LOGGING_LIBRARY_METRIC "/enabled"));

  nr_fw_support_add_logging_supportability_metric(txn, LIBRARY_NAME, false);
  tlib_pass_if_not_null(
      "happy path: logging library metric created",
      nrm_find(txn->unscoped_metrics, LOGGING_LIBRARY_METRIC "/disabled"));

  nrm_table_destroy(&txn->unscoped_metrics);
}

void test_main(void* p NRUNUSED) {
  test_fw_supportability_metrics();
}
