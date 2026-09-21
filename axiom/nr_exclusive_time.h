/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file provides a struct and associated functions to calculate exclusive
 * time for a segment.
 */
#ifndef NR_EXCLUSIVE_TIME_HDR
#define NR_EXCLUSIVE_TIME_HDR

#include <stdbool.h>
#include <stddef.h>

#include "util_time.h"

typedef struct _nr_exclusive_time_t nr_exclusive_time_t;

/*
 * Purpose : Create an exclusive time structure.
 *
 * Params  : 1. The number of child segments within the parent segment.
 *           2. The start time of the parent segment.
 *           3. The stop time of the parent segment.
 *           4. The suspend time of the parent segment (0 if it never
 *              self-suspended); subtracted from stop_time before duration
 *              is computed. See nr_segment.h's suspend_time field comment.
 *
 * Returns : A pointer to the exclusive time structure, or NULL on error.
 */
extern nr_exclusive_time_t* nr_exclusive_time_create(size_t child_segments,
                                                     nrtime_t start_time,
                                                     nrtime_t stop_time,
                                                     nrtime_t suspend_time
                                                    );

/*
 * Purpose : Destroy an exclusive time structure.
 *
 * Params  : 1. A pointer to the exclusive time structure.
 *
 * Returns : True on success; false otherwise.
 */
extern bool nr_exclusive_time_destroy(nr_exclusive_time_t** et_ptr);

/*
 * Purpose : Add a child period to the exclusive time structure.
 *
 *           The period described by the start and stop times will be removed
 *           from the exclusive time calculated for the parent segment.
 *
 * Params  : 1. A pointer to the exclusive time structure.
 *           2. The start time of the child segment.
 *           3. The stop time of the child segment.
 *           4. The suspend time of the child segment; stop_time is amended
 *              by this before the period is removed, so a self-suspended
 *              child only removes the time it was actually active.
 *
 * Returns : True on success; false otherwise.
 */
extern bool nr_exclusive_time_add_child(nr_exclusive_time_t* parent_et,
                                        nrtime_t start_time,
                                        nrtime_t stop_time,
                                        nrtime_t suspend_time);

/*
 * Purpose : Calculate how much exclusive time the parent segment actually had.
 *
 * Params  : 1. A pointer to the exclusive time structure.
 *
 * Returns : The amount of exclusive time. On error, 0 is returned, but note
 *           that 0 may also be a valid value. Also returns 0 if a child's
 *           duration exceeds the parent's remaining exclusive-time budget -
 *           logged as "this should be impossible" - which only happens if
 *           a suspend_time fed into this structure was wrong (see
 *           nr_segment.h's suspend_time field comment).
 */
extern nrtime_t nr_exclusive_time_calculate(nr_exclusive_time_t* et);

/*
 * Purpose : Ensure an exclusive time structure has the given size, start and
 *           stop times. .
 *
 *           This function does the following:
 *            - If the first parameter is NULL, a new exclusive time structure
 *              with the given parameters is allocated.
 *            - If the first parameter is given, start, stop and suspend times are set
 *            - If the first paramester is given and the exclusive time
 *              structure is not large enough to accomodate adding the given
 *              number of children, then it is resized.
 *
 * Params  : 1. The address of an exclusive time structure. If this is NULL, a
 *              new exclusive time structure will be allocated.
 *           2. The number of child segments that can be added to the exclusive
 *              time structure.
 *           3. The start time of the parent segment.
 *           4. The stop time of the parent segment.
 *           5. The suspend time of the parent segment.
 *
 * Returns : true if the exclusive time structure fits the given parameters or
 *           could be resized/changed to fit them.
 */
extern bool nr_exclusive_time_ensure(nr_exclusive_time_t** et_ptr,
                                     size_t child_segments,
                                     nrtime_t start_time,
                                     nrtime_t stop_time,
                                     nrtime_t suspend_time);

#endif /* NR_EXCLUSIVE_TIME_HDR */
