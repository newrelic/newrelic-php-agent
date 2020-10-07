/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "nr_synthetics.h"
#include "nr_synthetics_private.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_object.h"

nr_synthetics_t* nr_synthetics_create(const char* header) {
  size_t i = 0;
  nr_synthetics_t* out = NULL;
  nrobj_t* synth_obj = NULL;
  int version = 0;

  if (NULL == header) {
    return NULL;
  }

  synth_obj = nro_create_from_json(header);
  if (NULL == synth_obj) {
    return NULL;
  } else if (NR_OBJECT_ARRAY != nro_type(synth_obj)) {
    nrl_verbosedebug(NRL_TXN,
                     "%s: expected synthetics object of type %d, got %d",
                     __func__, NR_OBJECT_ARRAY, nro_type(synth_obj));

    nro_delete(synth_obj);
    return NULL;
  }

  out = (nr_synthetics_t*)nr_zalloc(sizeof(nr_synthetics_t));

  /*
   * See if we have a parser for the given version.
   */
  version = nro_get_array_int(synth_obj, 1, NULL);
  for (i = 0; i < (sizeof(nr_synthetics_parsers)
                   / sizeof(nr_synthetics_parser_table_t));
       i++) {
    if (version == nr_synthetics_parsers[i].version) {
      nr_status_t st = (nr_synthetics_parsers[i].parse_func)(synth_obj, out);

      nro_delete(synth_obj);
      if (NR_SUCCESS == st) {
        return out;
      }
      nr_synthetics_destroy(&out);
      nrl_verbosedebug(NRL_TXN, "%s: invalid synthetics header of version %d",
                       __func__, version);
      return NULL;
    }
  }

  /*
   * Well, we fell through.
   */
  nrl_verbosedebug(NRL_TXN, "%s: unknown synthetics version %d", __func__,
                   version);

  nro_delete(synth_obj);
  nr_synthetics_destroy(&out);

  return NULL;
}

#define PARSE_V1_INT(synth_obj, idx, out)                                     \
  {                                                                           \
    nr_status_t result = NR_FAILURE;                                          \
                                                                              \
    out = nro_get_array_int(synth_obj, idx, &result);                         \
    if (NR_FAILURE == result) {                                               \
      nrl_verbosedebug(NRL_TXN, "%s: error parsing field %d", __func__, idx); \
      return NR_FAILURE;                                                      \
    }                                                                         \
  }

#define PARSE_V1_STR(synth_obj, idx, out)                                     \
  {                                                                           \
    nr_status_t result = NR_FAILURE;                                          \
                                                                              \
    out = nr_strdup(nro_get_array_string(synth_obj, idx, &result));           \
    if (NR_FAILURE == result) {                                               \
      nrl_verbosedebug(NRL_TXN, "%s: error parsing field %d", __func__, idx); \
      return NR_FAILURE;                                                      \
    }                                                                         \
  }

nr_status_t nr_synthetics_parse_v1(const nrobj_t* synth_obj,
                                   nr_synthetics_t* out) {
  if ((NULL == synth_obj) || (NULL == out)) {
    return NR_FAILURE;
  }

  /*
   * The cross agent tests mandate that if additional fields are present then
   * the header should be considered invalid.
   */
  if (5 != nro_getsize(synth_obj)) {
    nrl_verbosedebug(
        NRL_TXN, "%s: invalid number of synthetics fields; expected 5, got %d",
        __func__, nro_getsize(synth_obj));
    return NR_FAILURE;
  }

  PARSE_V1_INT(synth_obj, 1, out->version);
  PARSE_V1_INT(synth_obj, 2, out->account_id);
  PARSE_V1_STR(synth_obj, 3, out->resource_id);
  PARSE_V1_STR(synth_obj, 4, out->job_id);
  PARSE_V1_STR(synth_obj, 5, out->monitor_id);

  return NR_SUCCESS;
}

void nr_synthetics_destroy(nr_synthetics_t** synthetics_ptr) {
  if ((NULL == synthetics_ptr) || (NULL == (*synthetics_ptr))) {
    return;
  }

  nr_free((*synthetics_ptr)->resource_id);
  nr_free((*synthetics_ptr)->job_id);
  nr_free((*synthetics_ptr)->monitor_id);
  nr_free((*synthetics_ptr)->outbound_json);

  nr_realfree((void**)synthetics_ptr);
}

int nr_synthetics_version(const nr_synthetics_t* synthetics) {
  if (NULL == synthetics) {
    return 0;
  }

  return synthetics->version;
}

int nr_synthetics_account_id(const nr_synthetics_t* synthetics) {
  if (NULL == synthetics) {
    return 0;
  }

  return synthetics->account_id;
}

const char* nr_synthetics_resource_id(const nr_synthetics_t* synthetics) {
  if (NULL == synthetics) {
    return NULL;
  }

  return synthetics->resource_id;
}

const char* nr_synthetics_job_id(const nr_synthetics_t* synthetics) {
  if (NULL == synthetics) {
    return NULL;
  }

  return synthetics->job_id;
}

const char* nr_synthetics_monitor_id(const nr_synthetics_t* synthetics) {
  if (NULL == synthetics) {
    return NULL;
  }

  return synthetics->monitor_id;
}

const char* nr_synthetics_outbound_header(nr_synthetics_t* synthetics) {
  nrobj_t* obj = NULL;

  if (NULL == synthetics) {
    return NULL;
  }

  if (NULL != synthetics->outbound_json) {
    return synthetics->outbound_json;
  }

  obj = nro_new_array();

  /*
   * If we eventually support more versions than just version 1, this will need
   * to be changed to output the new highest version.
   */
  nro_set_array_int(obj, 0, synthetics->version);
  nro_set_array_int(obj, 0, synthetics->account_id);
  nro_set_array_string(obj, 0, synthetics->resource_id);
  nro_set_array_string(obj, 0, synthetics->job_id);
  nro_set_array_string(obj, 0, synthetics->monitor_id);

  synthetics->outbound_json = nro_to_json(obj);
  nro_delete(obj);

  return synthetics->outbound_json;
}
