/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_EXCLUSIVE_TIME_PRIVATE_HDR
#define NR_EXCLUSIVE_TIME_PRIVATE_HDR

/*
 * A record of a state transition: either the start or end (stop) of a child
 * segment.
 *
 * We'd probably call these "events" were it not for the heavily overloaded use
 * of that noun already.
 */
typedef struct _nr_exclusive_time_transition_t {
  nrtime_t time;
  enum {
    CHILD_START,
    CHILD_STOP,
  } type;
} nr_exclusive_time_transition_t;

struct _nr_exclusive_time_t {
  nrtime_t start_time;
  nrtime_t stop_time;
  struct {
    size_t capacity;
    size_t used;
    nr_exclusive_time_transition_t transitions[0];
  } transitions;
};

extern int nr_exclusive_time_transition_compare(
    const nr_exclusive_time_transition_t* a,
    const nr_exclusive_time_transition_t* b,
    void* userdata);

#endif /* NR_EXCLUSIVE_TIME_PRIVATE_HDR */
