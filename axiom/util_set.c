/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "util_memory.h"
#include "util_set.h"
#include "util_set_private.h"

nr_set_t* nr_set_create(void) {
  nr_set_t* set = nr_malloc(sizeof(nr_set_t));

  set->size = 0;
  RB_INIT(&set->tree);

  return set;
}

void nr_set_destroy(nr_set_t** set_ptr) {
  nr_set_node_t* var = NULL;
  nr_set_node_t* nxt = NULL;
  nr_set_t* set;

  if ((NULL == set_ptr) || (NULL == *set_ptr)) {
    return;
  }

  set = *set_ptr;

  if (set->size > 0) {
    for (var = RB_MIN(nr_set_tree_t, &set->tree); var != NULL; var = nxt) {
      nxt = RB_NEXT(nr_set_tree_t, &set->tree, var);
      RB_REMOVE(nr_set_tree_t, &set->tree, var);
      nr_free(var);
    }
  }

  nr_realfree((void**)set_ptr);
}

bool nr_set_contains(nr_set_t* set, const void* value) {
  struct nr_set_node_t lookup = {.value = value};

  if (NULL == set) {
    return false;
  }

  return NULL != RB_FIND(nr_set_tree_t, &set->tree, &lookup);
}

void nr_set_insert(nr_set_t* set, const void* value) {
  nr_set_node_t* node;

  if (NULL == set) {
    return;
  }

  node = nr_malloc(sizeof(nr_set_node_t));
  node->value = value;

  /*
   * RB_INSERT returns NULL if the value doesn't exist in the tree already,
   * otherwise it returns a pointer to the node within the tree but does _not_
   * overwrite it. As a result, if we get a non-NULL value back, we should free
   * the node we just allocated to avoid leaking it.
   */
  if (NULL == RB_INSERT(nr_set_tree_t, &set->tree, node)) {
    set->size++;
  } else {
    nr_free(node);
  }
}

size_t nr_set_size(const nr_set_t* set) {
  if (NULL == set) {
    return 0;
  }

  return set->size;
}
