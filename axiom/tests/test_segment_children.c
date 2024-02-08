/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_segment.h"
#include "nr_segment_children.h"

#include "tlib_main.h"

static void test_segment_children_init(void) {
  nr_segment_children_t children;

  nr_segment_children_init(&children);
  tlib_pass_if_bool_equal(
      "is_packed must be true for an empty children structure", true,
      children.is_packed);
  tlib_pass_if_size_t_equal(
      "count must be zero for an empty children structure", 0,
      children.packed.count);
}

static void test_segment_children_deinit(void) {
  nr_segment_children_t children;
  nr_segment_t segment;

  nr_segment_children_init(&children);
  nr_segment_children_add(&children, &segment);
  nr_segment_children_deinit(&children);
  tlib_pass_if_bool_equal("is_packed must be true after deinit occurs", true,
                          children.is_packed);
  tlib_pass_if_size_t_equal("count must be zero after deinit occurs", 0,
                            children.packed.count);

  nr_segment_children_init(&children);
  nr_segment_children_add(&children, &segment);
  nr_segment_children_migrate_to_vector(&children);
  nr_segment_children_deinit(&children);
  tlib_pass_if_bool_equal("is_packed must be true after deinit occurs", true,
                          children.is_packed);
  tlib_pass_if_size_t_equal("count must be zero after deinit occurs", 0,
                            children.packed.count);
}

static void test_segment_children_size_invalid(void) {
  tlib_pass_if_size_t_equal("NULL children have 0 size", 0,
                            nr_segment_children_size(NULL));
}

static void test_segment_children_size(nr_segment_children_t* children,
                                       const size_t count) {
  size_t i;
  nr_segment_t* segments
      = (nr_segment_t*)nr_alloca(count * sizeof(nr_segment_t));

  for (i = 0; i < count; i++) {
    tlib_pass_if_size_t_equal("size must be equal to the number of children", i,
                              nr_segment_children_size(children));
    tlib_pass_if_bool_equal("adding a child should succeed", true,
                            nr_segment_children_add(children, &segments[i]));
  }

  tlib_pass_if_size_t_equal("size must be equal to the number of children",
                            count, nr_segment_children_size(children));
}

static void test_segment_children_size_packed(void) {
  nr_segment_children_t children;

  nr_segment_children_init(&children);
  test_segment_children_size(&children, NR_SEGMENT_CHILDREN_PACKED_LIMIT);
  tlib_pass_if_bool_equal("children structure is packed", true,
                          children.is_packed);
  nr_segment_children_deinit(&children);
}

static void test_segment_children_size_vector(void) {
  nr_segment_children_t children;

  nr_segment_children_init(&children);
  test_segment_children_size(&children, NR_SEGMENT_CHILDREN_PACKED_LIMIT + 1);
  tlib_pass_if_bool_equal("children structure is not packed", false,
                          children.is_packed);
  nr_segment_children_deinit(&children);
}

static void test_segment_children_get_invalid(void) {
  nr_segment_children_t children;
  nr_segment_t segment = {.parent = NULL};

  tlib_pass_if_null("NULL children have no children",
                    nr_segment_children_get(NULL, 0));

  nr_segment_children_init(&children);
  tlib_pass_if_null("empty children have no children to get",
                    nr_segment_children_get(&children, 0));
  tlib_pass_if_null("empty children have no children to get",
                    nr_segment_children_get(&children, 1));

  nr_segment_children_add(&children, &segment);
  tlib_pass_if_null("out of range indices will return NULL",
                    nr_segment_children_get(&children, 1));

  nr_segment_children_deinit(&children);
}

static void test_segment_children_get(nr_segment_children_t* children,
                                      const size_t count) {
  size_t i;
  nr_segment_t* segments
      = (nr_segment_t*)nr_alloca(count * sizeof(nr_segment_t));

  for (i = 0; i < count; i++) {
    tlib_pass_if_bool_equal("adding a child should succeed", true,
                            nr_segment_children_add(children, &segments[i]));
  }

  for (i = 0; i < count; i++) {
    tlib_pass_if_ptr_equal("get must return the correct element", &segments[i],
                           nr_segment_children_get(children, i));
  }
}

static void test_segment_children_get_packed(void) {
  nr_segment_children_t children;

  nr_segment_children_init(&children);
  test_segment_children_get(&children, NR_SEGMENT_CHILDREN_PACKED_LIMIT);
  tlib_pass_if_bool_equal("children structure is packed", true,
                          children.is_packed);
  nr_segment_children_deinit(&children);
}

static void test_segment_children_get_vector(void) {
  nr_segment_children_t children;

  nr_segment_children_init(&children);
  test_segment_children_get(&children, NR_SEGMENT_CHILDREN_PACKED_LIMIT + 1);
  tlib_pass_if_bool_equal("children structure is not packed", false,
                          children.is_packed);
  nr_segment_children_deinit(&children);
}

static void test_segment_children_add_invalid(void) {
  nr_segment_children_t children;
  nr_segment_t child = {.parent = NULL};

  nr_segment_children_init(&children);
  tlib_pass_if_bool_equal("adding to a NULL children should fail", false,
                          nr_segment_children_add(NULL, &child));
  tlib_pass_if_bool_equal("adding a NULL child should fail", false,
                          nr_segment_children_add(&children, NULL));
  nr_segment_children_deinit(&children);
}

static void test_segment_children_remove_invalid(void) {
  nr_segment_children_t children;
  nr_segment_t child = {.parent = NULL};

  nr_segment_children_init(&children);
  tlib_pass_if_bool_equal("removing from NULL children should fail", false,
                          nr_segment_children_remove(NULL, &child));
  tlib_pass_if_bool_equal("removing a NULL child should fail", false,
                          nr_segment_children_remove(&children, NULL));
  tlib_pass_if_bool_equal("removing from empty children should fail", false,
                          nr_segment_children_remove(&children, &child));
  nr_segment_children_deinit(&children);
}

static void test_segment_children_remove(nr_segment_children_t* children,
                                         size_t count) {
  size_t i;
  nr_segment_t* segments
      = (nr_segment_t*)nr_alloca((count + 1) * sizeof(nr_segment_t));

  for (i = 0; i < count; i++) {
    tlib_pass_if_bool_equal("adding a child should succeed", true,
                            nr_segment_children_add(children, &segments[i]));
  }
  /*
   * initialize the child_ix value of this segment so that the attempted
   * removal does not check an uninitialized value. In the real operation
   * of the agent, external constructs should prevent the attempted
   * removal of uninitialized segments.
   */
  segments[count].child_ix=count;

  tlib_pass_if_size_t_equal("adding elements should increment size",
                            count,
                            nr_segment_children_size(children));

  tlib_pass_if_bool_equal(
      "removing a non-existent element should fail", false,
      nr_segment_children_remove(children, &segments[count]));

  tlib_pass_if_bool_equal(
      "removing the last element should succeed", true,
      nr_segment_children_remove(children, &segments[count - 1]));
  tlib_pass_if_size_t_equal("removing the last element should change the size",
                            count - 1, nr_segment_children_size(children));

  for (i = 0; i < count - 1; i++) {
    tlib_pass_if_bool_equal("removing an element should succeed", true,
                            nr_segment_children_remove(children, &segments[i]));
    tlib_pass_if_size_t_equal("removing an element should decrement the size",
                              count - 2 - i,
                              nr_segment_children_size(children));
  }
}

static void test_segment_children_remove_packed(void) {
  nr_segment_children_t children;

  nr_segment_children_init(&children);
  test_segment_children_remove(&children, NR_SEGMENT_CHILDREN_PACKED_LIMIT);
  tlib_pass_if_bool_equal("segment children is packed", true,
                          children.is_packed);
  nr_segment_children_deinit(&children);
}

static void test_segment_children_remove_vector(void) {
  nr_segment_children_t children;

  nr_segment_children_init(&children);
  test_segment_children_remove(&children, NR_SEGMENT_CHILDREN_PACKED_LIMIT * 2);
  tlib_pass_if_bool_equal("segment children is not packed", false,
                          children.is_packed);
  nr_segment_children_deinit(&children);
}

static void test_segment_children_reparent_invalid(void) {
  nr_segment_children_t children;
  nr_segment_t segment = {.parent = NULL};

  tlib_pass_if_bool_equal("NULL children cannot be reparented", false,
                          nr_segment_children_reparent(NULL, &segment));
  tlib_pass_if_bool_equal("children cannot be reparented onto a NULL segment",
                          false, nr_segment_children_reparent(&children, NULL));
}

static void test_segment_children_reparent(nr_segment_children_t* children,
                                           size_t count) {
  size_t i;
  nr_segment_t parent;
  nr_segment_t* segments
      = (nr_segment_t*)nr_alloca(count * sizeof(nr_segment_t));

  nr_segment_children_init(&parent.children);

  for (i = 0; i < count; i++) {
    tlib_pass_if_bool_equal("adding a child should succeed", true,
                            nr_segment_children_add(children, &segments[i]));
  }

  tlib_pass_if_bool_equal("reparenting children should succeed", true,
                          nr_segment_children_reparent(children, &parent));
  tlib_pass_if_size_t_equal(
      "the original children struct should have no children left in it", 0,
      nr_segment_children_size(children));
  tlib_pass_if_size_t_equal("the new parent should have all the children",
                            count, nr_segment_children_size(&parent.children));

  for (i = 0; i < count; i++) {
    tlib_pass_if_ptr_equal("the child should have the new ", &parent,
                           segments[i].parent);
  }

  nr_segment_children_deinit(&parent.children);
}

static void test_segment_children_reparent_packed(void) {
  nr_segment_children_t children;

  nr_segment_children_init(&children);
  test_segment_children_reparent(&children, NR_SEGMENT_CHILDREN_PACKED_LIMIT);
  nr_segment_children_deinit(&children);
}

static void test_segment_children_reparent_vector(void) {
  nr_segment_children_t children;

  nr_segment_children_init(&children);
  test_segment_children_reparent(&children,
                                 NR_SEGMENT_CHILDREN_PACKED_LIMIT * 2);
  nr_segment_children_deinit(&children);
}

static void test_segment_children_vector_shrink(void) {
  nr_segment_children_t children;
  nr_segment_t parent = {0};
  nr_segment_t segments[9];
  size_t i;

  nr_segment_children_init(&children);
  nr_segment_children_init(&parent.children);

  nr_segment_children_add(&parent.children, &segments[0]);
  nr_segment_children_add(&parent.children, &segments[1]);

  for (i = 2; i < sizeof(segments) / sizeof(segments[0]); i++) {
    nr_segment_children_add(&children, &segments[i]);
  }

  nr_segment_children_reparent(&children, &parent);

  tlib_pass_if_bool_equal("parent children not packed", false,
                          parent.children.is_packed);

  for (i = 0; i < sizeof(segments) / sizeof(segments[0]); i++) {
    nr_segment_children_remove(&parent.children, &segments[i]);
    tlib_pass_if_bool_equal("is_packed must stay false after once set to false",
                            false, parent.children.is_packed);
  }

  nr_segment_children_deinit(&parent.children);
  nr_segment_children_deinit(&children);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_segment_children_init();
  test_segment_children_deinit();

  test_segment_children_size_invalid();
  test_segment_children_size_packed();
  test_segment_children_size_vector();

  test_segment_children_get_invalid();
  test_segment_children_get_packed();
  test_segment_children_get_vector();

  // This is the only add test because the size and get tests very thoroughly
  // exercise the normal operation of nr_segment_children_add() already.
  test_segment_children_add_invalid();

  test_segment_children_remove_invalid();
  test_segment_children_remove_packed();
  test_segment_children_remove_vector();

  test_segment_children_reparent_invalid();
  test_segment_children_reparent_packed();
  test_segment_children_reparent_vector();

  test_segment_children_vector_shrink();
}
