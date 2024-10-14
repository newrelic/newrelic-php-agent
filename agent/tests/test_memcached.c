/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlib_php.h"
#include "tlib_datastore.h"

#include "php_agent.h"
#include "php_memcached.h"
#include "util_system.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static char* system_host_name;

static void test_create_datastore_instance(void) {
  assert_datastore_instance_equals_destroy(
      "named socket",
      &((nr_datastore_instance_t){
          .host = system_host_name,
          .database_name = "unknown",
          .port_path_or_id = "/tmp/memcached.sock",
      }),
      nr_php_memcached_create_datastore_instance("/tmp/memcached.sock", 0));

  assert_datastore_instance_equals_destroy(
      "empty socket",
      &((nr_datastore_instance_t){
          .host = system_host_name,
          .database_name = "unknown",
          .port_path_or_id = "unknown",
      }),
      nr_php_memcached_create_datastore_instance("", 0));

  assert_datastore_instance_equals_destroy(
      "empty host",
      &((nr_datastore_instance_t){
          .host = system_host_name,
          .database_name = "unknown",
          .port_path_or_id = "unknown",
      }),
      nr_php_memcached_create_datastore_instance(NULL, 0));

  assert_datastore_instance_equals_destroy(
      "host.name socket",
      &((nr_datastore_instance_t){
          .host = "host.name",
          .database_name = "unknown",
          .port_path_or_id = "11211",
      }),
      nr_php_memcached_create_datastore_instance("host.name", 11211));

  assert_datastore_instance_equals_destroy(
      "host and port",
      &((nr_datastore_instance_t){
          .host = "unknown",
          .database_name = "unknown",
          .port_path_or_id = "6379",
      }),
      nr_php_memcached_create_datastore_instance("", 6379));

  assert_datastore_instance_equals_destroy(
      "NULL socket",
      &((nr_datastore_instance_t){
          .host = "unknown",
          .database_name = "unknown",
          .port_path_or_id = "11211",
      }),
      nr_php_memcached_create_datastore_instance(NULL, 11211));
}

static void test_create_instance_metric(void) {
    nrtxn_t* txn;
    nrmetric_t* metric;
    char* metric_str;
    tlib_php_engine_create("");
    tlib_php_request_start();
    txn = NRPRG(txn);

    nr_php_memcached_create_instance_metric("host", 11211);
    metric = nrm_find(txn->unscoped_metrics, "Datastore/instance/Memcached/host/11211");
    tlib_pass_if_not_null("metric found", metric);

    nr_php_memcached_create_instance_metric("", 11211);
    metric = nrm_find(txn->unscoped_metrics, "Datastore/instance/Memcached/unknown/11211");
    tlib_pass_if_not_null("metric found", metric);

    nr_php_memcached_create_instance_metric(NULL, 7);
    metric = nrm_find(txn->unscoped_metrics, "Datastore/instance/Memcached/unknown/7");
    tlib_pass_if_not_null("metric found", metric);

    nr_php_memcached_create_instance_metric("path/to/sock", 0);
    metric_str = nr_formatf("Datastore/instance/Memcached/%s/path/to/sock", system_host_name);
    metric = nrm_find(txn->unscoped_metrics, metric_str);
    nr_free(metric_str);
    tlib_pass_if_not_null("metric found", metric);

    nr_php_memcached_create_instance_metric("", 0);
    metric_str = nr_formatf("Datastore/instance/Memcached/%s/unknown", system_host_name);
    metric = nrm_find(txn->unscoped_metrics, metric_str);
    nr_free(metric_str);
    tlib_pass_if_not_null("metric found", metric);

    // restart the transaction because the next metric is the same as a previous metric
    tlib_php_request_end();
    tlib_php_request_start();
    txn = NRPRG(txn);

    nr_php_memcached_create_instance_metric(NULL, 0);
    metric_str = nr_formatf("Datastore/instance/Memcached/%s/unknown", system_host_name);
    metric = nrm_find(txn->unscoped_metrics, metric_str);
    nr_free(metric_str);
    tlib_pass_if_not_null("metric found", metric);

    tlib_php_request_end();
    tlib_php_engine_destroy();
}

void test_main(void* p NRUNUSED) {
  system_host_name = nr_system_get_hostname();

  test_create_datastore_instance();
  test_create_instance_metric();

  nr_free(system_host_name);
}
