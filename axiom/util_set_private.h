/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UTIL_SET_PRIVATE_HDR
#define UTIL_SET_PRIVATE_HDR

#include <stdint.h>

/*
 * The set type is built on top of the red-black tree provided in libbsd, which
 * (as a single header file) is vendored into axiom.
 */
#include "vendor/bsd/sys/tree.h"

/*
 * libbsd's red-black tree implementation requires structures for nodes within
 * the tree, and for the tree itself.
 *
 * Let's start with the node.
 */
typedef struct nr_set_node_t {
  const void* value;

  // This provides the linking fields required for the tree to operate.
  RB_ENTRY(nr_set_node_t) node;
} nr_set_node_t;

/*
 * Now we define the tree structure proper.
 */
RB_HEAD(nr_set_tree_t, nr_set_node_t);
typedef struct nr_set_tree_t nr_set_tree_t;

/*
 * As the libbsd red-black tree implementation doesn't track how many elements
 * are contained within it, we'll do that in a wrapper structure (that then
 * forms the exported type in the public header).
 */
struct _nr_set_t {
  size_t size;
  nr_set_tree_t tree;
};

/*
 * A comparison function is required for the libbsd red-black tree to sort
 * correctly. Since this set only deals with pointers, we'll use the simplest
 * possible implementation: treating the pointers as integers.
 *
 * Note that this technically violates the C standard: an architecture without a
 * flat memory model might have problems here. In practice, it's unlikely that
 * we'll ever need to support such an architecture.
 */
static inline int nr_set_node_cmp(const nr_set_node_t* a,
                                  const nr_set_node_t* b) {
  const uintptr_t av = (const uintptr_t)a->value;
  const uintptr_t bv = (const uintptr_t)b->value;

  /*
   * Since we can't guarantee that subtracting two uintptr_t values won't
   * overflow an int, but libbsd requires an int, we'll use comparisons instead.
   */
  if (av < bv) {
    return 1;
  } else if (av > bv) {
    return -1;
  }
  return 0;
}

/*
 * Finally, we need to actually define the functions to manipulate the tree.
 * libbsd uses a clever templating system to ensure type safety, and the
 * RB_GENERATE_* set of macros generate the required function definitions.
 *
 * Note that we're not using the documented RB_GENERATE_STATIC macro here: it
 * includes an __unused attribute on each function definition that gcc chokes
 * on. Since it delegates to RB_GENERATE_INTERNAL, and since the underlying
 * header is vendored, we'll just use that directly with our own attributes.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
RB_GENERATE_INTERNAL(nr_set_tree_t,
                     nr_set_node_t,
                     node,
                     nr_set_node_cmp,
                     static NRUNUSED)
#pragma GCC diagnostic pop

#endif /* UTIL_SET_PRIVATE_HDR */
