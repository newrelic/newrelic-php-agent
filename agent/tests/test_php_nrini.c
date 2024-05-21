/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_main.h"
#include "tlib_php.h"

#include "php_agent.h"
#include "php_nrini.h"

static void test_nr_ini_to_env(void) {
  char* res = NULL;

  res = nr_ini_to_env("newrelic.enabled");

  tlib_pass_if_str_equal("ini converted to env", "NEW_RELIC_ENABLED", res);

  nr_free(res);

  res = nr_ini_to_env(
      "newrelic.application_logging.forwarding.context_data.include");

  tlib_pass_if_str_equal(
      "ini converted to env",
      "NEW_RELIC_APPLICATION_LOGGING_FORWARDING_CONTEXT_DATA_INCLUDE", res);

  nr_free(res);

  res = nr_ini_to_env("newrelic.12345");

  tlib_pass_if_str_equal("numerical values handled correctly",
                         "NEW_RELIC_12345", res);

  nr_free(res);

  res = nr_ini_to_env("not_a_newrelic.ini_value");

  tlib_pass_if_null("invalid ini not converted", res);

  nr_free(res);

  res = nr_ini_to_env("newrelic.");

  tlib_pass_if_null("no value after prefix", res);

  nr_free(res);

  res = nr_ini_to_env(NULL);

  tlib_pass_if_null("reject null values", res);

  nr_free(res);

  res = nr_ini_to_env("newrelic.ini__value");

  tlib_pass_if_str_equal("double underscores handled correctly",
                         "NEW_RELIC_INI_VALUE", res);

  nr_free(res);

  res = nr_ini_to_env("newrelic._option_");

  tlib_pass_if_str_equal("dot and underscores handled correctly",
                         "NEW_RELIC_OPTION_", res);

  nr_free(res);
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_nr_ini_to_env();
}
