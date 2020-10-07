/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_STACKED_SEGMENT_HDR
#define PHP_STACKED_SEGMENT_HDR

#include "nr_segment.h"

/*
 * Stacked segments
 *
 * php_execute.c explicitly warns about using stack space
 * in an excessive way - nevertheless with stacked segments we do exactly
 * that: adding a segment struct to the stack for every php_execute call
 * frame.
 *
 * The reason for this is, that having temporary segments on the stack
 * gives us a considerable performance advantage. The usual workflow for
 * starting a segment looks like this at the time of this writing.
 *
 *  - nr_segment_start
 *    - 2 obsolete if checks
 *    - nr_slab_next
 *      - 1 if check
 *      - vector size comparison
 *      - pointer return
 *    - nr_segment_init
 *      - 3 value changes
 *      - get start time
 *      - init children (2 value changes)
 *      - get current segment
 *      - set parent
 *      - set current segment
 *
 * Here's what happens when a segment is discarded. Note that this is
 * the _best_ case, as this is a segment without children and metrics.
 *
 *   - nr_segment_discard
 *     - 4 if checks
 *     - retire current segment
 *     - check metrics size
 *     - remove segment from parent vector
 *     - reparent children (3 if checks)
 *     - de-init children
 *     - destroy fields
 *     - nr_slab_release
 *       - zero-out segment
 *
 * As a comparision, here's what happens when using stacked segments.
 * Also for the best case.
 *
 *   - nr_php_stacked_segment_init        - nr_php_stacked_segment_discard
 *     - 3 value changes                    - reparent children (3 if checks)
 *     - get start time                     - 1 if check for segment->id
 *     - init children (2 value changes)    - 1 value change
 *
 * This simplified behavior saves us a lot, as especially in real-world
 * applications we are dealing with lots of short running segments that
 * are immediately discarded. Speeding up the segment init/discard cycle
 * is crucial for improving the performance of the agent.
 *
 * What enables us to eliminate much of the work done in the
 * nr_segment_start/nr_segment_discard cycle:
 *
 *  - Not using the parent stacks on the transaction, but using pointers
 *    between stacked segments to determine the current active segment.
 *  - Not having segments go in and out of the slab allocator.
 *  - Functions are optimized to the context in which they are called.
 *    They are only called for segments that have no metrics, so we assume
 *    the metrics vector is not initialized.
 *  - We avoid lots of sanity if-checks happening throughout the
 *    nr_segment_* call stack.
 *  - Effective destroying due to context. Only segment->id can reasonably be
 *    allocated for segments we're dealing with here. No other nr_free
 *    calls are needed.
 *
 * The workflow of using stacked segments in connection with regular
 * segments is complicated. It's best illustrated by a short ASCII
 * cartoon.
 *
 *  root <                      root                      root
 *                               |                         |
 *                               *A <                      *A
 *                                                         |
 *                                                         *B
 *
 * We start out with a root segment, starting stacked segment *A and
 * then stacked segment *B as a child of *A.
 *
 *  root                        root                       root
 *   |                           |                          |
 *   *A <                        *A                         *A <
 *                               |
 *                               *C <
 *
 * *B gets discarded, and *A is the current segment again. *C gets
 * started as child of *A and gets discarded too. Note that up to this
 * point, no segment except the root segment ever was allocated via the
 * slab and lived on the heap.
 *
 *   root                        root                       root
 *    |                           |                          |
 *    *A <                        *A                         *A
 *                                |                          |
 *                                *D <                       *D
 *                                                           |
 *                                                           *E <
 *
 * In a next exciting step, stacked segment *D is started as child of *A and *E
 * is started as child of *D.
 *
 *   root                        root
 *    |                           |
 *    *A                          *A <
 *    |                           |
 *    *D <                        e
 *    |
 *    e
 *
 * Now something new happens. We want to keep the stacked segment *E. We
 * copy the contents of the stacked segment *E into a segment e we
 * obtained from the slab allocator, and we make e a child of the
 * stacked segment *D. When the stacked segment *D is discarded, its
 * child e is made a child of *D's parent *A.
 *
 *   root                        root                       root
 *    |                           |                          |
 *    *A                          *A <                       *A
 *   / \                          |                         / \
 *  e   *F <                      e                        e   *G <
 *
 * More of the same. We create a stacked segment *F as child of A and
 * discard it. We then create a stacked segment *G.
 *
 *   root                        root <                     root
 *    |                           |                         / \
 *    *A <                        a                        a   *H
 *   / \                         / \                      / \
 *  e   g                       e   g                    b   e
 *
 * Finally we also decide to keep *G. Again, it is turned into a regular
 * segment g and made a child of *A. Then we decide to keep *A, turning
 * it into regular segment a and making it a child of the root segment.
 * Afterward a stacked segment *H is created as child of the root
 * segment.
 *
 * Note that with this workflow, we went through the
 * nr_segment_start/nr_segment_discard cycle for only 3 times,
 * although we used 8 different segments. For the remaining 5 segments, we
 * went through the much cheaper stacked segment cycle.
 *
 * Also note that this only works with segments on the default parent stack.
 * Stacked segments cannot be used to model async segments.
 */

#include "php_agent.h"

/*
 * Purpose : Initialize a stacked segment.
 *
 *           This sets the passed stacked segment as the current custom
 *           segment and initializes necessary fields (being the transaction
 *           pointer, the start time and the children vector).
 *
 * Params  : 1. A pointer to a stacked segment. It is assumed that the
 *              stacked segment is zero'd and not NULL.
 *
 * Returns : A pointer to the stacked segment, or NULL if the segment could not
 *           be initialized.
 */
extern nr_segment_t* nr_php_stacked_segment_init(
    nr_segment_t* stacked TSRMLS_DC);

/*
 * Purpose : Deinitialize a stacked segment.
 *
 *           This resets the current custom segment to the parent of
 *           this stacked segment, reparents children and  and de-initializes
 *           necessary fields on the segments: children are
 *           de-initialized during reparenting, a possible set id is
 *           freed.
 *
 * Params  : 1. A pointer to an initialized stacked segment. This must not be
 *              NULL.
 */
extern void nr_php_stacked_segment_deinit(nr_segment_t* stacked TSRMLS_DC);

/*
 * Purpose : Unwind the stack of stacked segments.
 *
 *           All segments in the stack of stacked segments are turned into
 *           regular segments. This avoids leaking memory due to stacked
 *           segments that might have regular segments as children.
 */
extern void nr_php_stacked_segment_unwind(TSRMLS_D);

/*
 * Purpose : Transform a stacked segment into a regular segment.
 *
 *           This retrieves a regular segment from the slab allocator and copies
 *           the contents of the stacked segment into the regular
 *           segment. All children of the stacked segment are correctly
 *           reparented with the regular segment.
 *
 *           After successfully calling this function, the stacked segment can
 *           be seen as de-initialized.
 *
 * Params  : 1. A pointer to an initialized stacked segment. This must not be
 *              NULL.
 *
 * Returns : A pointer to a regular segment.
 */
extern nr_segment_t* nr_php_stacked_segment_move_to_heap(
    nr_segment_t* stacked TSRMLS_DC);

#endif /* PHP_STACKED_SEGMENT_HDR */
