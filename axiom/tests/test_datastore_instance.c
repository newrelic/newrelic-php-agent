/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "nr_datastore_instance.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_system.h"

#include "tlib_main.h"

static void test_is_localhost(void) {
  int outcome;

  outcome = nr_datastore_instance_is_localhost(NULL);
  tlib_pass_if_int_equal("null host", 0, outcome);

  outcome = nr_datastore_instance_is_localhost("");
  tlib_pass_if_int_equal("empty string", 0, outcome);

  outcome = nr_datastore_instance_is_localhost("127.0.0.2");
  tlib_pass_if_int_equal("not quite local address", 0, outcome);

  outcome = nr_datastore_instance_is_localhost("localhost");
  tlib_pass_if_int_equal("local address", 1, outcome);

  outcome = nr_datastore_instance_is_localhost("127.0.0.1");
  tlib_pass_if_int_equal("local address", 1, outcome);

  outcome = nr_datastore_instance_is_localhost("0.0.0.0");
  tlib_pass_if_int_equal("local address", 1, outcome);

  outcome = nr_datastore_instance_is_localhost("0:0:0:0:0:0:0:1");
  tlib_pass_if_int_equal("local address", 1, outcome);

  outcome = nr_datastore_instance_is_localhost("::1");
  tlib_pass_if_int_equal("local address", 1, outcome);

  outcome = nr_datastore_instance_is_localhost("0:0:0:0:0:0:0:0");
  tlib_pass_if_int_equal("local address", 1, outcome);

  outcome = nr_datastore_instance_is_localhost("::");
  tlib_pass_if_int_equal("local address", 1, outcome);
}

static void test_destroy(void) {
  nr_datastore_instance_t* instance = NULL;
  nr_datastore_instance_t stack
      = {.host = NULL, .port_path_or_id = NULL, .database_name = NULL};

  /* Don't explode! */
  nr_datastore_instance_destroy(NULL);
  nr_datastore_instance_destroy(&instance);
  nr_datastore_instance_destroy_fields(NULL);

  instance = nr_datastore_instance_create("a", "b", "c");
  nr_datastore_instance_destroy(&instance);

  tlib_pass_if_null("it's dead, Jim", instance);

  nr_datastore_instance_destroy_fields(&stack);

  stack.host = nr_strdup("host");
  stack.port_path_or_id = nr_strdup("port path or id");
  stack.database_name = nr_strdup("database name");
  nr_datastore_instance_destroy_fields(&stack);
  tlib_pass_if_null("host", stack.host);
  tlib_pass_if_null("port path or id", stack.port_path_or_id);
  tlib_pass_if_null("database name", stack.database_name);
}

static void test_getters(void) {
  nr_datastore_instance_t* instance = NULL;
  const char* host = NULL;
  const char* port_path_or_id = NULL;
  const char* database_name = NULL;
  char* system_host = NULL;

  host = nr_datastore_instance_get_host(NULL);
  port_path_or_id = nr_datastore_instance_get_port_path_or_id(NULL);
  database_name = nr_datastore_instance_get_database_name(NULL);

  tlib_pass_if_null("null host if instance is null", host);
  tlib_pass_if_null("null port_path_or_id if instance is null",
                    port_path_or_id);
  tlib_pass_if_null("null database_name if instance is null", database_name);

  instance
      = nr_datastore_instance_create("bluestar", "1234", "lemon_poppyseed");
  host = nr_datastore_instance_get_host(instance);
  port_path_or_id = nr_datastore_instance_get_port_path_or_id(instance);
  database_name = nr_datastore_instance_get_database_name(instance);

  tlib_pass_if_str_equal("host in matches host out", "bluestar", host);
  tlib_pass_if_str_equal("port_path_or_id in matches port_path_or_id out",
                         "1234", port_path_or_id);
  tlib_pass_if_str_equal("database_name in matches database_name out",
                         "lemon_poppyseed", database_name);

  nr_datastore_instance_set_host(instance, "localhost");
  host = nr_datastore_instance_get_host(instance);
  system_host = nr_system_get_hostname();
  tlib_pass_if_str_equal("localhost appropriately transformed", system_host,
                         host);

  nr_free(system_host);
  nr_datastore_instance_destroy(&instance);
}

static void test_setters(void) {
  nr_datastore_instance_t* instance = NULL;

  nr_datastore_instance_set_host(NULL, NULL);
  nr_datastore_instance_set_port_path_or_id(NULL, NULL);
  nr_datastore_instance_set_database_name(NULL, NULL);

  tlib_pass_if_null("null instance is unaffected by null input", instance);

  instance
      = nr_datastore_instance_create("bluestar", "1234", "lemon_poppyseed");
  nr_datastore_instance_set_host(instance, NULL);
  nr_datastore_instance_set_port_path_or_id(instance, NULL);
  nr_datastore_instance_set_database_name(instance, NULL);

  tlib_fail_if_null("non-null instance is unaffected by null input", instance);
  tlib_pass_if_str_equal("null host results in unknown", "unknown",
                         instance->host);
  tlib_pass_if_str_equal("null port_path_or_id results in unknown", "unknown",
                         instance->port_path_or_id);
  tlib_pass_if_str_equal("null database_name results in unknown", "unknown",
                         instance->database_name);

  nr_datastore_instance_set_host(instance, "");
  nr_datastore_instance_set_port_path_or_id(instance, "");
  nr_datastore_instance_set_database_name(instance, "");

  tlib_fail_if_null("non-null instance is unaffected by empty input", instance);
  tlib_pass_if_str_equal("empty host results in unknown", "unknown",
                         instance->host);
  tlib_pass_if_str_equal("empty port_path_or_id results in unknown", "unknown",
                         instance->port_path_or_id);
  tlib_pass_if_str_equal("empty database_name results in unknown", "unknown",
                         instance->database_name);

  nr_datastore_instance_set_host(instance, "voodoo");
  nr_datastore_instance_set_port_path_or_id(instance, "4321");
  nr_datastore_instance_set_database_name(instance, "chocolate");

  tlib_fail_if_null("non-null instance is unaffected by valid input", instance);
  tlib_pass_if_str_equal("host in matches host out", "voodoo", instance->host);
  tlib_pass_if_str_equal("port_path_or_id in matches host out", "4321",
                         instance->port_path_or_id);
  tlib_pass_if_str_equal("database_name in matches host out", "chocolate",
                         instance->database_name);

  nr_datastore_instance_destroy(&instance);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_is_localhost();
  test_destroy();
  test_getters();
  test_setters();
}
