/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for dealing with synthetics headers.
 */
#ifndef NR_SYNTHETICS_PRIVATE_HDR
#define NR_SYNTHETICS_PRIVATE_HDR

#include "nr_axiom.h"
#include "nr_synthetics.h"
#include "util_object.h"

/*
 * The careful eye will note that the first five fields are the exact same
 * fields, in the same order, as the X-NewRelic-Synthetics JSON array for
 * version 1 of the synthetics spec.
 */
struct _nr_synthetics_t {
  int version;
  int account_id;
  char* resource_id;
  char* job_id;
  char* monitor_id;

  /*
   * Lazily generated, cached JSON for outbound headers.
   */
  char* outbound_json;
};

/*
 * Purpose : Parses a version 1 header.
 *
 * Params  : 1. The JSON array.
 *           2. The synthetics object to write into.
 *
 * Returns : NR_SUCCESS on success, or NR_FAILURE on failure.
 */
extern nr_status_t nr_synthetics_parse_v1(const nrobj_t* synth_obj,
                                          nr_synthetics_t* out);

/*
 * Registered parsers for synthetics versions.
 */
typedef nr_status_t (*nr_synthetics_parse_func_t)(const nrobj_t*,
                                                  nr_synthetics_t*);

typedef struct _nr_synthetics_parser_table_t {
  int version;
  nr_synthetics_parse_func_t parse_func;
} nr_synthetics_parser_table_t;

static const nr_synthetics_parser_table_t nr_synthetics_parsers[] = {
    {1, nr_synthetics_parse_v1},
};

#endif /* NR_SYNTHETICS_PRIVATE_HDR */
