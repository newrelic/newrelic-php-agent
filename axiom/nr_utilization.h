/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_UTILIZATION_HDR
#define NR_UTILIZATION_HDR

typedef struct _nr_utilization_t {
  int aws : 1;
  int azure : 1;
  int gcp : 1;
  int pcf : 1;
  int docker : 1;
} nr_utilization_t;

static const nr_utilization_t nr_utilization_default = {
    .aws = 1,
    .azure = 1,
    .gcp = 1,
    .pcf = 1,
    .docker = 1,
};

#endif /* NR_UTILIZATION_HDR */
