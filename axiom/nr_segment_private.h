/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_SEGMENT_PRIVATE_HDR
#define NR_SEGMENT_PRIVATE_HDR

#include "nr_segment.h"

/*
 * Purpose : Free all data related to a segment's typed attributes.
 *
 * Params  : 1. A segment's type.
 *           2. A pointer to a segment's _nr_segment_typed_attributes_t
 *              structure.
 */
void nr_segment_destroy_typed_attributes(
    nr_segment_type_t type,
    nr_segment_typed_attributes_t** attributes);

/*
 * Purpose : Free all data related to a segment's datastore metadata.
 *
 * Params  : 1. A pointer to a segment's nr_segment_datastore_t structure.
 */
void nr_segment_datastore_destroy_fields(nr_segment_datastore_t* datastore);

/*
 * Purpose : Free all data related to a segment's external metadata.
 *
 * Params  : 1. A pointer to a segment's nr_segment_external_t structure.
 */
void nr_segment_external_destroy_fields(nr_segment_external_t* external);

/*
 * Purpose : Free all data related to a segment metric.
 *
 * Params  : 1. A pointer to a segment's nr_segment_metric_t structure.
 */
void nr_segment_metric_destroy_fields(nr_segment_metric_t* sm);

/*
 * Purpose : Free all data related to a segment error.
 *
 * Params  : 1. A pointer to a segment's nr_segment_error_t structure.
 */
void nr_segment_error_destroy_fields(nr_segment_error_t* segment_error);

#endif
