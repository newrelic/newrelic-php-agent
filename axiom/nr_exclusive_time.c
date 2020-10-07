/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_exclusive_time.h"
#include "nr_exclusive_time_private.h"

#include "nr_axiom.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_sort.h"
#include "util_time.h"

bool nr_exclusive_time_ensure(nr_exclusive_time_t** et_ptr,
                              size_t child_segments,
                              nrtime_t start_time,
                              nrtime_t stop_time) {
  nr_exclusive_time_t* et;
  size_t unused;

  if (NULL == et_ptr) {
    return false;
  }

  /*
   * The exclusive time structure is not initialized. Allocate and set a
   * new one.
   */
  if (NULL == *et_ptr) {
    (*et_ptr) = nr_exclusive_time_create(child_segments, start_time, stop_time);
    return (NULL != *et_ptr);
  }

  et = *et_ptr;

  /*
   * Ensure start and stop time are set.
   */
  et->start_time = start_time;
  et->stop_time = stop_time;

  /*
   * Ensure the given number of children can be added to the exclusive
   * time structure.
   */
  unused = et->transitions.capacity - et->transitions.used;
  if (unused < (child_segments * 2)) {
    size_t new_capacity = et->transitions.used + (child_segments * 2);

    et = nr_realloc(
        et, sizeof(nr_exclusive_time_t)
                + sizeof(nr_exclusive_time_transition_t) * new_capacity);

    if (NULL == et) {
      return false;
    }

    et->transitions.capacity = new_capacity;

    *et_ptr = et;
  }

  return true;
}

nr_exclusive_time_t* nr_exclusive_time_create(size_t child_segments,
                                              nrtime_t start_time,
                                              nrtime_t stop_time) {
  nr_exclusive_time_t* et;

  et = nr_malloc(sizeof(nr_exclusive_time_t)
                 + sizeof(nr_exclusive_time_transition_t) * child_segments * 2);
  et->start_time = start_time;
  et->stop_time = stop_time;
  et->transitions.capacity = child_segments * 2;
  et->transitions.used = 0;

  return et;
}

bool nr_exclusive_time_destroy(nr_exclusive_time_t** et_ptr) {
  if (NULL == et_ptr || NULL == *et_ptr) {
    return false;
  }

  nr_realfree((void**)et_ptr);

  return true;
}

bool nr_exclusive_time_add_child(nr_exclusive_time_t* parent_et,
                                 nrtime_t start_time,
                                 nrtime_t stop_time) {
  if (nrunlikely(NULL == parent_et
                 || (parent_et->transitions.used + 2)
                        > parent_et->transitions.capacity)) {
    return false;
  }

  if (start_time > stop_time) {
    nrl_verbosedebug(NRL_TXN,
                     "cannot have start time " NR_TIME_FMT
                     " > stop time " NR_TIME_FMT,
                     start_time, stop_time);
    return false;
  }

  /*
   * Basic theory of operation: we need to add a transition for both the start
   * and stop of this segment to the transitions array.
   */
  parent_et->transitions.transitions[parent_et->transitions.used++]
      = (nr_exclusive_time_transition_t){
          .time = start_time,
          .type = CHILD_START,
      };
  parent_et->transitions.transitions[parent_et->transitions.used++]
      = (nr_exclusive_time_transition_t){
          .time = stop_time,
          .type = CHILD_STOP,
      };

  return true;
}

int nr_exclusive_time_transition_compare(
    const nr_exclusive_time_transition_t* a,
    const nr_exclusive_time_transition_t* b,
    void* userdata NRUNUSED) {
  if (nrunlikely(NULL == a || NULL == b)) {
    /*
     * There really shouldn't be a scenario in which a NULL pointer finds its
     * way into this array.
     */
    nrl_error(NRL_TXN,
              "unexpected NULL pointer when comparing transitions: a=%p; b=%p",
              a, b);
    return 0;
  }

  if (a->time < b->time) {
    return -1;
  } else if (a->time > b->time) {
    return 1;
  }

  /*
   * Starts go before stops, if the times are equal. (There's no functional
   * difference in doing so, but it saves a tiny bit of work in
   * nr_exclusive_time_calculate() updating exclusive_time and last_start if we
   * stop the active counter dropping to 0.)
   */
  if (a->type != b->type) {
    if (CHILD_START == a->type) {
      return -1;
    } else if (CHILD_START == b->type) {
      return 1;
    }
  }

  // Eh, whatever. Some things _are_ created equal.
  return 0;
}

nrtime_t nr_exclusive_time_calculate(nr_exclusive_time_t* et) {
  unsigned int active_children = 0;
  nrtime_t exclusive_time;
  size_t i;
  nrtime_t last_start = 0;

  if (nrunlikely(NULL == et)) {
    return 0;
  }

  if (nrunlikely(et->start_time > et->stop_time)) {
    return 0;
  }

  if (0 == et->transitions.used) {
    return nr_time_duration(et->start_time, et->stop_time);
  }

  /*
   * Essentially, what we want to do in this function is walk the list of
   * transitions in time order. So, firstly, let's put it in time order.
   */
  nr_sort(&et->transitions.transitions, et->transitions.used,
          sizeof(nr_exclusive_time_transition_t),
          (nr_sort_cmp_t)nr_exclusive_time_transition_compare, NULL);

  /*
   * It's generally easier to reason about exclusive time if you think of it as
   * a subtractive process: all time that cannot be attributed to a direct
   * child is exclusive time, since it represents time the segment in question
   * was doing stuff. So we'll start by setting the exclusive time to be the
   * full duration of the segment.
   */
  exclusive_time = nr_time_duration(et->start_time, et->stop_time);

  for (i = 0; i < et->transitions.used; i++) {
    const nrtime_t time = et->transitions.transitions[i].time;

    switch (et->transitions.transitions[i].type) {
      case CHILD_START:
        /*
         * OK, so we have a start transition. If there are no active children,
         * then that means that the exclusive time for the segment ends at this
         * point, so we'll track this time as the last start.
         *
         * If the child segment is starting _before_ the parent segment (which
         * is possible in an async world), then we'll just clamp the time to the
         * segment start time for now and see what else we get.
         */
        if (0 == active_children) {
          if (time > et->start_time) {
            last_start = time;
          } else {
            last_start = et->start_time;
          }
        }
        active_children += 1;
        break;

      case CHILD_STOP:
        /*
         * Here we have a stop transition. If this is the last active child,
         * then this is the end of the period of non-exclusive time, and we
         * should adjust the segment's exclusive time accordingly.
         */
        if (0 == active_children) {
          nrl_warning(
              NRL_TXN,
              "child stopped, but no children were thought to be active");
          continue;
        } else if (1 == active_children) {
          nrtime_t duration;

          /*
           * As with start transitions, nothing can happen to the segment's
           * exclusive time after the stop time, so we'll clamp the duration to
           * the stop time if required.
           */
          if (et->stop_time < time) {
            duration = nr_time_duration(last_start, et->stop_time);
          } else {
            duration = nr_time_duration(last_start, time);
          }

          if (duration > exclusive_time) {
            /*
             * Hitting this arm is probably a logic bug.
             */
            nrl_verbosedebug(NRL_TXN,
                             "attempted to subtract " NR_TIME_FMT
                             " us from exclusive time of " NR_TIME_FMT
                             " us; this should be impossible",
                             duration, exclusive_time);
            exclusive_time = 0;
            goto end;
          } else {
            exclusive_time -= duration;
          }

          /*
           * If we're past the end of the parent, we can just bail early;
           * nothing else can affect the exclusive time from here on, since we
           * know the array is sorted.
           */
          if (time > et->stop_time) {
            goto end;
          }
        }

        active_children -= 1;
        break;

      default:
        nrl_error(NRL_TXN, "unknown transition type %d",
                  et->transitions.transitions[i].type);
    }
  }

end:
  return exclusive_time;
}
