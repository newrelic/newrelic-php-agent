/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Segment children functions that are not considered part of its "public" API.
 */
#ifndef NR_SEGMENT_CHILDREN_PRIVATE_HDR
#define NR_SEGMENT_CHILDREN_PRIVATE_HDR

/*
 * Purpose : Add an element to the children vector.
 *
 * Params  : 1. A pointer to a segment's nr_segment_children_t structure.
 *           2. A pointer to the segment to add.
 *
 * Returns : True if successful, false otherwise.
 *
 * Warning : This function does not check if the children pointer is valid, or
 *           if a vector is in use.
 */
static inline bool nr_segment_children_add_vector(
    nr_segment_children_t* children,
    nr_segment_t* child) {
  // If we're ever going to add a defensive check to guard against the vector
  // capacity growing to the point where the high bit is set, this would be
  // where we'd do it.
  return nr_vector_push_back(&children->vector, child);
}

/*
 * Purpose : Migrate a segment children structure to use a vector as its
 *           backing store, unconditionally.
 *
 * Params  : 1. A pointer to a segment's nr_segment_children_t structure.
 */
static inline void nr_segment_children_migrate_to_vector(
    nr_segment_children_t* children) {
  if (nrlikely(children->is_packed)) {
    if (nrlikely(children->packed.count > 0)) {
      // Using nr_alloca() because we should never have enough elements in the
      // packed variant to cause stack overflow issues.
      const size_t count = children->packed.count;
      nr_segment_t** packed
          = (nr_segment_t**)nr_alloca(sizeof(nr_segment_t*) * count);

      // Temporarily copy the elements out to the stack array above.
      nr_memcpy(packed, &children->packed.elements[0],
                sizeof(nr_segment_t*) * count);

      // Set up the vector.
      children->is_packed = false;
      nr_vector_init(&children->vector, count * 2, NULL, NULL);
      children->vector.used = count;
      nr_memcpy(&children->vector.elements[0], packed,
                sizeof(nr_segment_t*) * count);
    } else {
      children->is_packed = false;
      nr_vector_init(&children->vector, NR_SEGMENT_CHILDREN_PACKED_LIMIT, NULL,
                     NULL);
    }
  }
}

/*
 * Purpose : Get the sibling previous to or next to the given child.
 *           Also known as the pair of queries:
 *             "Who is your big sister?"
 *             "Who is your little brother?"
 *
 * Params  : 1. A pointer to a segment's nr_segment_children_t structure.
 *           2. A pointer to the child of interest.
 *
 * Returns : A pointer to the sibling segment; NULL if no such sibling exists.
 *
 * Note    : At the time of this writing, this pair of functions are provided
 *           for internal testing purposes.
 */
extern nr_segment_t* nr_segment_children_get_prev(
    nr_segment_children_t* children,
    nr_segment_t* child);

extern nr_segment_t* nr_segment_children_get_next(
    nr_segment_children_t* children,
    nr_segment_t* child);

#endif /* NR_SEGMENT_CHILDREN_PRIVATE_HDR */
