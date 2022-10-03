/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <limits.h>

#include "tlib_main.h"
#include "nr_log_level.h"

static void test_log_level_rfc_to_psr(void) {
  /* Test with invalid values */
  tlib_pass_if_str_equal("invalid rfc value -1", LL_UNKN_STR,
                         nr_log_level_rfc_to_psr(-1));

  tlib_pass_if_str_equal("invalid rfc value INT_MAX", LL_UNKN_STR,
                         nr_log_level_rfc_to_psr(INT_MAX));

  tlib_pass_if_str_equal("invalid rfc value INT_MIN", LL_UNKN_STR,
                         nr_log_level_rfc_to_psr(INT_MIN));

  /* Test known values */
  tlib_pass_if_str_equal("rfc value 0", LL_EMER_STR,
                         nr_log_level_rfc_to_psr(0));
  tlib_pass_if_str_equal("rfc value 1", LL_ALER_STR,
                         nr_log_level_rfc_to_psr(1));
  tlib_pass_if_str_equal("rfc value 2", LL_CRIT_STR,
                         nr_log_level_rfc_to_psr(2));
  tlib_pass_if_str_equal("rfc value 3", LL_ERRO_STR,
                         nr_log_level_rfc_to_psr(3));
  tlib_pass_if_str_equal("rfc value 4", LL_WARN_STR,
                         nr_log_level_rfc_to_psr(4));
  tlib_pass_if_str_equal("rfc value 5", LL_NOTI_STR,
                         nr_log_level_rfc_to_psr(5));
  tlib_pass_if_str_equal("rfc value 6", LL_INFO_STR,
                         nr_log_level_rfc_to_psr(6));
  tlib_pass_if_str_equal("rfc value 7", LL_DEBU_STR,
                         nr_log_level_rfc_to_psr(7));

  /* Test unknown value */
  tlib_pass_if_str_equal("rfc value 8", LL_UNKN_STR,
                         nr_log_level_rfc_to_psr(8));
}

static void test_log_level_str_to_int(void) {
  /* Test NULL value */
  tlib_pass_if_int_equal("NULL str to rfc", LOG_LEVEL_UNKNOWN,
                         nr_log_level_str_to_int(NULL));

  /* custom level name */
  tlib_pass_if_int_equal("Unknown str to rfc", LOG_LEVEL_UNKNOWN,
                         nr_log_level_str_to_int("GOSSIP"));

  /* defined level names */
  tlib_pass_if_int_equal("Unknown str to rfc", LOG_LEVEL_EMERGENCY,
                         nr_log_level_str_to_int("EMERGENCY"));
  tlib_pass_if_int_equal("Unknown str to rfc", LOG_LEVEL_ALERT,
                         nr_log_level_str_to_int("ALERT"));
  tlib_pass_if_int_equal("Unknown str to rfc", LOG_LEVEL_CRITICAL,
                         nr_log_level_str_to_int("CRITICAL"));
  tlib_pass_if_int_equal("Unknown str to rfc", LOG_LEVEL_ERROR,
                         nr_log_level_str_to_int("ERROR"));
  tlib_pass_if_int_equal("Unknown str to rfc", LOG_LEVEL_WARNING,
                         nr_log_level_str_to_int("WARNING"));
  tlib_pass_if_int_equal("Unknown str to rfc", LOG_LEVEL_NOTICE,
                         nr_log_level_str_to_int("NOTICE"));
  tlib_pass_if_int_equal("Unknown str to rfc", LOG_LEVEL_INFO,
                         nr_log_level_str_to_int("INFO"));
  tlib_pass_if_int_equal("Unknown str to rfc", LOG_LEVEL_DEBUG,
                         nr_log_level_str_to_int("DEBUG"));
  tlib_pass_if_int_equal("Unknown str to rfc", LOG_LEVEL_UNKNOWN,
                         nr_log_level_str_to_int("UNKNOWN"));
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 1, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_log_level_str_to_int();
  test_log_level_rfc_to_psr();
}
