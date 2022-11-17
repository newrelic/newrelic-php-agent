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

/*
 * Observer API paradigm.
 *
 * Here's what happens when using stacked segments with OAPI.
 *
 *   - nr_php_stacked_segment_init        - nr_php_stacked_segment_deinit
 *     - calloc stacked segment
 *     - calloc metadata
 *     - 3 value changes                    - reparent children (3 if checks)
 *     - get start time                     - 1 if check for segment->id
 *     - init children (2 value changes)    - 1 value change
 *
 * Speeding up the segment init/discard cycle
 * is crucial for improving the performance of the agent.
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
 * We start out with a root segment, OAPI calls nr_php_observer_fcall_begin for
 * A, and it starts stacked segment *A and then nr_php_observer_fcall_begin(B)
 * starts stacked segment *B as a child of *A.
 *
 *  root                        root                       root
 *   |                           |                          |
 *   *A <                        *A                         *A <
 *                               |
 *                               *C <
 *
 * *nr_php_observer_fcall_end(B) decides to discard *B, and *A is the current
 * segment again. nr_php_observer_fcall_begin(C) starts *C gets started as child
 * of *A and when nr_php_observer_fcall_end(C) is called, *C gets discarded too.
 * Note that up to this point, no segment except the root segment ever was
 * allocated via the slab; however, stacked segments are being calloced in
 * stacked_segment_init.
 *
 *   root                        root                       root
 *    |                           |                          |
 *    *A <                        *A                         *A
 *                                |                          |
 *                                *D <                       *D
 *                                                           |
 *                                                           *E <
 *
 * In a next exciting step, nr_php_observer_fcall_begin(D) starts stacked
 * segment *D as child of *A and nr_php_observer_fcall_begin(E) *E is started as
 * child of *D.
 *
 *   root                        root
 *    |                           |
 *    *A                          *A <
 *    |                           |
 *    *D <                        e
 *    |
 *    e
 *
 * Now something new happens. nr_php_observer_fcall_end(E) decides to keep the
 * stacked segment *E. We copy the contents of the stacked segment *E into a
 * segment e we obtained from the slab allocator, and we make e a child of the
 * stacked segment *D. nr_php_observer_fcall_end(D) discards stacked segment *D
 * is, and its child e is made a child of *D's parent *A.
 *
 *   root                        root                       root
 *    |                           |                          |
 *    *A                          *A <                       *A
 *   / \                          |                         / \
 *  e   *F <                      e                        e   *G <
 *
 * More of the same. nr_php_observer_fcall_begin(F) creates a stacked segment *F
 * as child of A and nr_php_observer_fcall_end(F) eventually discards it.
 * nr_php_observer_fcall_begin(G) then creates a stacked segment *G.
 *
 *   root                        root <                     root
 *    |                           |                         / \
 *    *A <                        a                        a   *H
 *   / \                         / \                      / \
 *  e   g                       e   g                    e   g
 *
 * Finally nr_php_observer_fcall_end(G) also decides to keep *G. Again, it is
 * turned into a regular segment g and made a child of *A. Then we decide to
 * keep *A, turning it into regular segment a and making it a child of the root
 * segment. Afterward a nr_php_observer_fcall_begin(H) starts stacked segment *H
 * as child of the root segment.
 *
 * Note that with this workflow, we went through the
 * nr_segment_start/nr_segment_discard cycle for only 3 times,
 * although we used 8 different segments. For the remaining 5 segments, we
 * went through the stacked segment cycle.
 *
 * Also note that this only works with segments on the default parent stack.
 * Stacked segments cannot be used to model async segments.
 *
 * Dangling segments:
 * With the use of Observer we have the possibility of dangling segments.  In
 * the normal course of events, the above scenario shows
 * nr_php_observer_fcall_begin starting segments and nr_php_observer_fcall_end
 * keeping/discarding/ending segments. However, in the case of an uncaught
 * exception, nr_php_observer_fcall_end is never called and therefore, the logic
 * to keep/discard/end the segment doesn't automatically get initiated.
 * Additionally, PHP only provides the last exception (meaning if exceptions
 * were thrown then rethrown or another exception thrown, nothing gets
 * communicated except for the last exception. PHP has a hook that can be used
 * to notify whenever an exception is triggered but it doesn't give any
 * indication if that exception was ever caught.
 *
 * To handle this, dangling exception sweeps occur in
 * nr_php_observer_exception_segments_end and is called from 5 different places:
 * 1) nr_php_observer_fcall_begin - before a new segment starts
 * 2) nr_php_observer_fcall_end - before a segment is ended(kept/discarded)
 * 3) nr_php_stacked_segment_unwind - when a txn ends and we are closing up shop
 * 4) php_observer_handle_exception_hook - when a new exception is noticed
 * 5) in newrelic APIs that depend on having the current segment
 *
 *
 * The workflow of using stacked segments in connection with regular
 * segments when an exception occurs is complicated.
 * These cases are illustrated by a series of short ASCII cartoons.
 *
 * case 1 nr_php_observer_fcall_begin - before a new segment starts
 *  root <                      root                      root
 *                               |                         |
 *                               *A <                      *A
 *                                                         |
 *                                                         *B
 *
 * We start out with a root segment, OAPI calls nr_php_observer_fcall_begin(A),
 * and it starts stacked segment *A and then nr_php_observer_fcall_begin(B)
 * starts stacked segment *B as a child of *A.
 *
 *  root                       root
 *   |                           |
 *   *A                          *A
 *   |                           |
 *   *B                          *B <
 *    |                           |
 *    *C <                        c
 *
 * nr_php_observer_fcall_begin(C) starts *C gets started as child
 * of *B. Function C throws an uncaught exception which B does not catch so
 * neither nr_php_observer_fcall_end(B) nor nr_php_observer_fcall_end(C) is
 * called and *C remains the current segment. A catches the exception and calls
 * function D, so nr_php_observer_fcall_begin(D) is triggered. At this point we
 * realize the current stacked_segment->metadata->This value and the
 * execute_data->prev_execute_data->This don't match so we don't want to parent
 * *D to the wrong segment. We check the global exception hook and see it
 * has a value and that the global uncaught_exception_this also matches the
 * current segment `this`. Time to apply the exception and clean up dangling
 * segments. We pop the current segment *C and apply the exception.
 * Because it has an exception, the segment is kept so we copy the contents of
 * the stacked segment *C into a segment c we obtained from the slab allocator,
 * and we make c a child of the stacked segment *B which becomes the current
 * segment.
 *
 *  root                        root                       root
 *   |                           |                          |
 *   *A <                        *A <                       *A
 *    |                          |                          / \
 *    b                          b                          b  *D <
 *    |                          |                          |
 *    c                          c                          c
 *
 *
 * But we aren't done yet.
 * current stacked_segment->metadata->this still doesn't equal the
 * execute_data->prev_execute_data->This provided by
 * nr_php_observer_fcall_begin(D). We pop the current segment *B and apply the
 * exception. Because it has an exception, the segment is kept so we copy the
 * contents of the stacked segment *B into a segment b we obtained from the
 * slab allocator, and we make b a child of the stacked segment *A which
 * becomes the current segment.  Now current stacked_segment->metadata->this
 * still DOES equal the execute_data->prev_execute_data->This provided by
 * nr_php_observer_fcall_begin(D) so we proceed and create stacked segment *D
 * correctly parented as a child of *A and *D becomes the current segment.
 *
 * case 2 nr_php_observer_fcall_end - before a segment is ended(kept/discarded)
 *  root <                      root                      root
 *                               |                         |
 *                               *A <                      *A
 *                                                         |
 *                                                         *B
 *
 * We start out with a root segment, OAPI calls nr_php_observer_fcall_begin(A),
 * and it starts stacked segment *A and then nr_php_observer_fcall_begin(B)
 * starts stacked segment *B as a child of *A.
 *
 *  root                       root
 *   |                           |
 *   *A                          *A
 *   |                           |
 *   *B                          *B <
 *    |                           |
 *    *C <                        c
 *
 * nr_php_observer_fcall_begin(C) starts segment *C as child
 * of *B. Function C throws an uncaught exception which B does not catch so
 * neither nr_php_observer_fcall_end(B) nor nr_php_observer_fcall_end(C) is
 * called and *C remains the current segment. A catches the exception and
 * nr_php_observer_fcall_end(A) is triggered. At this point we compare the
 * current stacked_segment->metadata->This value with the execute_data->This and
 * realize the two don't match. We check the global exception hook and see it
 * has a value and that the global uncaught_exception_this also matches the
 * current segment `this`. Time to apply the exception and clean up dangling
 * segments. We pop the current segment *C and apply the exception.
 * Because it has an exception, the segment is kept so we copy the contents of
 * the stacked segment *C into a segment c we obtained from the slab allocator,
 * and we make c a child of the stacked segment *B which becomes the current
 * segment.
 *
 *  root                        root <
 *   |                           |
 *   *A <                        a
 *    |                          |
 *    b                          b
 *    |                          |
 *    c                          c
 *
 *
 * But we aren't done yet.
 * current stacked_segment->metadata->this still doesn't equal the
 * execute_data-> this provided by nr_php_observer_fcall_end(A). We pop the
 * current segment *B and apply the exception. Because it has an exception, the
 * segment is kept so we copy the contents of the stacked segment *B into a
 * segment b we obtained from the slab allocator, and we make b a child of the
 * stacked segment *A which becomes the current segment.  Now current
 * stacked_segment->metadata->this still DOES equal the execute_data-> this
 * provided by nr_php_observer_fcall_end(A) so it proceeds, decides to keep the
 * segment and we copy the contents of the stacked segment *A into a segment a
 * we obtained from the slab allocator, and we make a a child of the stacked
 * segment root and root becomes the current segment.
 *
 * case 3 nr_php_stacked_segment_unwind - when a txn ends and we clean up
 *
 * root <                      root                       root
 *                              |                          |
 *                              *A <                       *A
 *                                                         |
 *                                                         *B <
 *
 * We start out with a root segment, OAPI calls nr_php_observer_fcall_begin(A),
 * and it starts stacked segment *A and then nr_php_observer_fcall_begin(B)
 * starts stacked segment *B as a child of *A.
 *
 *  root                       root
 *   |                           |
 *   *A                          *A
 *   |                           |
 *   *B                          *B <
 *   |                           |
 *   *C <                        c
 *
 * nr_php_observer_fcall_begin(C) starts *C gets started as child
 * of *B. Function C throws an uncaught exception which B does not catch so
 * netiher nr_php_observer_fcall_end(B) nor nr_php_observer_fcall_end(C) is
 * called and *C remains the current segment. A does not catch the exception,
 * but the txn has ended. Because we didn't get any nr_php_observer_fcall_end we
 * know no segment caught the exception. We'll apply the acception and
 * keep/close stacked segments all the way down the stack to clean up dangling
 * segments. We pop the current segment *C and apply the exception. Because it
 * has an exception, the segment is kept so we copy the contents of the stacked
 * segment *C into a segment c we obtained from the slab allocator, and we make
 * c a child of the stacked segment *B which becomes the current segment.
 *
 *  root                        root <                        root
 *   |                           |                             |
 *   *A <                        a                             a
 *    |                          |                             |
 *    b                          b                             b
 *    |                          |                             |
 *    c                          c                             c
 *
 * We pop the current segment *B and apply the exception. Because it has an
 * exception, the segment is kept so we copy the contents of the stacked segment
 * *B into a segment b we obtained from the slab allocator, and we make b a
 * child of the stacked segment *A which becomes the current segment.  Then we
 * pop the current segment *A and apply the exception. Because it has an
 * exception, the segment is kept so we copy the contents of the stacked segment
 * *A into a segment a we obtained from the slab allocator, and we make a a
 * child of the root.  The exception is applied to the root and the txn ends.
 *
 * case 4 php_observer_handle_exception_hook - when a new exception is noticed
 *  root <                      root                      root
 *                               |                         |
 *                               *A <                      *A
 *                                                         |
 *                                                         *B
 *
 * We start out with a root segment, OAPI calls nr_php_observer_fcall_begin(A),
 * and it starts stacked segment *A and then nr_php_observer_fcall_begin(B)
 * starts stacked segment *B as a child of *A.
 *
 *  root                        root                       root
 *   |                           |                           |
 *   *A <                        *A                          *A
 *                               |                           |
 *                               *B                          *B <
 *                               |                           |
 *                               *C <                        c
 *
 * nr_php_observer_fcall_begin(C) starts *C gets started as child
 * of *B. Function C throws an uncaught exception which B does not catch so
 * netiher nr_php_observer_fcall_end(B) nor nr_php_observer_fcall_end(C) is
 * called and *C remains the current segment. B catches the exception throws
 * another exception. At this point we realize the current
 * exception->This value indicates another function is active.  Because we
 * received no nr_php_observer_fcall_end up to that point, we know the exception
 * was uncaught until the exception->This function. We check the global
 * exception hook and see it has a value and that the global
 * uncaught_exception_this also matches the current segment `this`. Time to
 * apply the exception and clean up dangling segments. We pop the current
 * segment *C and apply the exception. Because it has an exception, the segment
 * is kept so we copy the contents of the stacked segment *C into a segment c we
 * obtained from the slab allocator, and we make c a child of the stacked
 * segment *B which becomes the current segment.
 *
 *  root
 *   |
 *   *A
 *    |
 *    *B <
 *    |
 *    c
 *
 * current stacked_segment->metadata->this now equals the exception->This. so we
 * reserve judgement on what eventually happens to segment *B and *B becomes the
 * current segment with the new active exception stored. Any subsequent dangling
 * segments are cleaned when the next scenario 1-5 occurs.
 *
 * case 5 in newrelic APIs that depend on having the current segment
 *  root <                      root                      root
 *                               |                         |
 *                               *A <                      *A
 *                                                         |
 *                                                         *B
 *
 * We start out with a root segment, OAPI calls nr_php_observer_fcall_begin(A),
 * and it starts stacked segment *A and then nr_php_observer_fcall_begin(B)
 * starts stacked segment *B as a child of *A.
 *
 *  root                       root
 *   |                           |
 *   *A                          *A
 *   |                           |
 *   *B                          *B <
 *   |                           |
 *   *C <                        c
 *
 * nr_php_observer_fcall_begin(C) starts *C gets started as child
 * of *B. Function C throws an uncaught exception which B does not catch so
 * netiher nr_php_observer_fcall_end(B) nor nr_php_observer_fcall_end(C) is
 * called and *C remains the current segment. A catches the exception and calls
 * newrelic_notice_error. We check the `this` value of the function that called
 * newrelic_notice_error and see it is not the same. Because we received no
 * nr_php_observer_fcall_end up to that point, we know the exception was
 * uncaught until the Function A. We check the global exception hook and see it
 * has a value and that the global uncaught_exception_this also matches the
 * current segment `this`. Time to apply the exception and clean up dangling
 * segments as we don't want to apply the notice_error to the wrong segment. We
 * pop the current segment *C and apply the exception. Because it has an
 * exception, the segment is kept so we copy the contents of the stacked segment
 * *C into a segment c we obtained from the slab allocator, and we make c a
 * child of the stacked segment *B which becomes the current segment.
 *
 *  root
 *   |
 *   *A <
 *    |
 *    b
 *    |
 *    c
 *
 *
 * But we aren't done yet.
 * The `this` value of the function that called
 * newrelic_notice_error and see it is not the same as the current segment
 * `this`. We pop the current segment *B and apply the exception. Because it has
 * an exception, the segment is kept so we copy the contents of the stacked
 * segment *B into a segment b we obtained from the slab allocator, and we make
 * b a child of the stacked segment *A which becomes the current segment.  The
 * this` value of the function that called newrelic_notice_error and see it is
 * not the same as the current segment `this` so it proceeds and applies the
 * notice error to the current segment *A.
 * Note that this only works with segments on the default parent stack.
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
