/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "util_vector.h"
#include "util_vector_private.h"

#include <sys/types.h>

#include "util_memory.h"
#include "util_sort.h"

/*
 * A number of functions in this file end in _impl: these are internal
 * implementation functions that perform actions without safety checks. This is
 * used to support initialising and finalising vectors regardless of allocation
 * strategy, and to keep common code for insert and removal operations common.
 */

static inline void nr_vector_init_impl(nr_vector_t* v,
                                       size_t initial,
                                       nr_vector_dtor_t dtor,
                                       void* dtor_userdata) {
  v->capacity = initial ? initial : 8;
  v->used = 0;
  v->elements = nr_calloc(v->capacity, sizeof(void*));
  v->dtor = dtor;
  v->dtor_userdata = dtor_userdata;
}

nr_vector_t* nr_vector_create(size_t initial,
                              nr_vector_dtor_t dtor,
                              void* dtor_userdata) {
  nr_vector_t* v = nr_malloc(sizeof(nr_vector_t));

  nr_vector_init_impl(v, initial, dtor, dtor_userdata);
  return v;
}

bool nr_vector_init(nr_vector_t* v,
                    size_t initial,
                    nr_vector_dtor_t dtor,
                    void* dtor_userdata) {
  if (NULL == v) {
    return false;
  }

  nr_vector_init_impl(v, initial, dtor, dtor_userdata);
  return true;
}

static inline void nr_vector_deinit_impl(nr_vector_t* v) {
  if (v->dtor) {
    size_t i;

    for (i = 0; i < v->used; i++) {
      (v->dtor)(v->elements[i], v->dtor_userdata);
    }
  }

  v->capacity = 0;
  v->used = 0;
  nr_free(v->elements);
}

bool nr_vector_destroy(nr_vector_t** v_ptr) {
  if (NULL == v_ptr || NULL == *v_ptr) {
    return false;
  }

  nr_vector_deinit_impl(*v_ptr);
  nr_realfree((void**)v_ptr);
  return true;
}

bool nr_vector_deinit(nr_vector_t* v) {
  if (NULL == v) {
    return false;
  }

  nr_vector_deinit_impl(v);
  return true;
}

static inline bool nr_vector_ensure_impl(nr_vector_t* v, size_t capacity) {
  size_t new_capacity;
  void** new_elements;

  if (v->capacity >= capacity) {
    return true;
  }

  for (new_capacity = v->capacity ? v->capacity : 8; new_capacity < capacity;
       new_capacity *= 2)
    ;

  new_elements = nr_reallocarray(v->elements, new_capacity, sizeof(void*));
  if (NULL == new_elements) {
    return false;
  }

  v->capacity = new_capacity;
  v->elements = new_elements;

  return true;
}

bool nr_vector_ensure(nr_vector_t* v, size_t capacity) {
  if (NULL == v || 0 == capacity) {
    return false;
  }

  return nr_vector_ensure_impl(v, capacity);
}

static inline bool nr_vector_shrink_if_necessary_impl(nr_vector_t* v) {
  size_t new_capacity;
  void** new_elements;

  // Not shrinking past 4 is admittedly arbitrary, but we need some sort of stop
  // condition so that the capacity can't drop to zero.
  if (v->used < 4 || v->used >= (v->capacity / 2)) {
    return true;
  }

  // We won't bother reducing as far as possible here; if the vector somehow
  // ends up in a state where the capacity is many multiples of the number of
  // elements used, we'll just let subsequent remove calls handle further
  // shrinkage.
  new_capacity = v->capacity / 2;
  new_elements = nr_reallocarray(v->elements, new_capacity, sizeof(void*));
  if (NULL == new_elements) {
    return false;
  }

  v->capacity = new_capacity;
  v->elements = new_elements;

  return true;
}

bool nr_vector_shrink_if_necessary(nr_vector_t* v) {
  return nr_vector_shrink_if_necessary_impl(v);
}

static inline bool nr_vector_insert_impl(nr_vector_t* v,
                                         size_t pos,
                                         void* element) {
  if (!nr_vector_ensure_impl(v, v->used + 1)) {
    return false;
  }

  if (pos < v->used) {
    // Insert before pos; so move [pos..used] up.
    nr_memmove(&v->elements[pos + 1], &v->elements[pos],
               sizeof(void*) * (v->used - pos));
  } else {
    // Constrain pos to v->used.
    pos = v->used;
  }

  v->elements[pos] = element;
  v->used += 1;

  return true;
}

static inline bool nr_vector_remove_impl(nr_vector_t* v,
                                         size_t pos,
                                         void** element_ptr) {
  if (pos >= v->used) {
    return false;
  }

  *element_ptr = v->elements[pos];
  if (pos < (v->used - 1)) {
    nr_memmove(&v->elements[pos], &v->elements[pos + 1],
               sizeof(void*) * (v->used - pos - 1));
  }

  v->used -= 1;
  // No real point grabbing the return value; even if it fails, there's not much
  // to report.
  nr_vector_shrink_if_necessary_impl(v);

  return true;
}

bool nr_vector_push_front(nr_vector_t* v, void* element) {
  return nr_vector_insert(v, 0, element);
}

bool nr_vector_push_back(nr_vector_t* v, void* element) {
  if (NULL == v) {
    return false;
  }

  return nr_vector_insert_impl(v, v->used, element);
}

bool nr_vector_pop_front(nr_vector_t* v, void** element_ptr) {
  return nr_vector_remove(v, 0, element_ptr);
}

bool nr_vector_pop_back(nr_vector_t* v, void** element_ptr) {
  // We want to check v->used here because otherwise we'll try to subtract one
  // from it, which will result in interesting behaviour.
  if (NULL == v || NULL == element_ptr || 0 == v->used) {
    return false;
  }

  return nr_vector_remove_impl(v, v->used - 1, element_ptr);
}

bool nr_vector_insert(nr_vector_t* v, size_t pos, void* element) {
  if (NULL == v) {
    return false;
  }

  return nr_vector_insert_impl(v, pos, element);
}

bool nr_vector_remove(nr_vector_t* v, size_t pos, void** element_ptr) {
  if (NULL == v || NULL == element_ptr) {
    return false;
  }

  return nr_vector_remove_impl(v, pos, element_ptr);
}

bool nr_vector_get_element(nr_vector_t* v, size_t pos, void** element_ptr) {
  if (NULL == v || NULL == element_ptr || pos >= v->used) {
    return false;
  }

  *element_ptr = v->elements[pos];
  return true;
}

bool nr_vector_replace(nr_vector_t* v, size_t pos, void* element) {
  if (NULL == v || pos >= v->used) {
    return false;
  }

  if (v->dtor) {
    (v->dtor)(v->elements[pos], v->dtor_userdata);
  }

  v->elements[pos] = element;
  return true;
}

typedef struct _nr_vector_sort_callback_wrapper_metadata_t {
  nr_vector_cmp_t comparator;
  void* userdata;
} nr_vector_sort_callback_wrapper_metadata_t;

static int nr_vector_sort_callback_wrapper(
    const void** a,
    const void** b,
    nr_vector_sort_callback_wrapper_metadata_t* metadata) {
  // While the values being pointed to may be NULL, the pointers provided by
  // nr_sort() should be valid in all cases.
  if (nrunlikely(NULL == a || NULL == b || NULL == metadata)) {
    return 0;
  }

  return (metadata->comparator)(*a, *b, metadata->userdata);
}

bool nr_vector_sort(nr_vector_t* v,
                    nr_vector_cmp_t comparator,
                    void* userdata) {
  nr_vector_sort_callback_wrapper_metadata_t metadata
      = {.comparator = comparator, .userdata = userdata};

  if (NULL == v || NULL == comparator) {
    return false;
  }

  nr_sort(v->elements, v->used, sizeof(void*),
          (nr_sort_cmp_t)nr_vector_sort_callback_wrapper, &metadata);

  return true;
}

bool nr_vector_iterate(nr_vector_t* v,
                       nr_vector_iter_t callback,
                       void* userdata) {
  size_t i;

  if (NULL == v || NULL == callback) {
    return false;
  }

  for (i = 0; i < v->used; i++) {
    if (false == (callback)(v->elements[i], userdata)) {
      return false;
    }
  }

  return true;
}

static bool nr_vector_find_comparator(const nr_vector_t* v,
                                      const size_t start,
                                      const ssize_t delta,
                                      const void* needle,
                                      nr_vector_cmp_t comparator,
                                      void* userdata,
                                      size_t* index_ptr) {
  size_t i;

  for (i = start; i < v->used; i += delta) {
    if (0 == (comparator)(v->elements[i], needle, userdata)) {
      if (index_ptr) {
        *index_ptr = i;
      }
      return true;
    }
  }

  return false;
}

static bool nr_vector_find_direct(const nr_vector_t* v,
                                  const size_t start,
                                  const ssize_t delta,
                                  const void* needle,
                                  size_t* index_ptr) {
  size_t i;

  for (i = start; i < v->used; i += delta) {
    if (v->elements[i] == needle) {
      if (index_ptr) {
        *index_ptr = i;
      }
      return true;
    }
  }

  return false;
}

static bool nr_vector_find(const nr_vector_t* v,
                           const size_t start,
                           const bool forward,
                           const void* needle,
                           nr_vector_cmp_t comparator,
                           void* userdata,
                           size_t* index_ptr) {
  const ssize_t delta = forward ? 1 : -1;

  // The caller must already have checked v.

  if (nrunlikely(start >= v->used)) {
    return false;
  }

  if (comparator) {
    return nr_vector_find_comparator(v, start, delta, needle, comparator,
                                     userdata, index_ptr);
  }

  return nr_vector_find_direct(v, start, delta, needle, index_ptr);
}

bool nr_vector_find_first(const nr_vector_t* v,
                          const void* needle,
                          nr_vector_cmp_t comparator,
                          void* userdata,
                          size_t* index_ptr) {
  if (nrunlikely(NULL == v)) {
    return false;
  }

  if (0 == v->used) {
    return false;
  }

  return nr_vector_find(v, 0, true, needle, comparator, userdata, index_ptr);
}

bool nr_vector_find_last(const nr_vector_t* v,
                         const void* needle,
                         nr_vector_cmp_t comparator,
                         void* userdata,
                         size_t* index_ptr) {
  if (nrunlikely(NULL == v)) {
    return false;
  }

  if (0 == v->used) {
    return false;
  }

  return nr_vector_find(v, v->used - 1, false, needle, comparator, userdata,
                        index_ptr);
}
