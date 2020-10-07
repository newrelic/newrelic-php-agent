/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * A simple vector, or expanding array.
 */
#ifndef UTIL_VECTOR_HDR
#define UTIL_VECTOR_HDR

#include <stdbool.h>
#include <stddef.h>

typedef void (*nr_vector_dtor_t)(void* element, void* userdata);

/* The vector data type. */
typedef struct _nr_vector_t {
  size_t capacity;
  size_t used;
  void** elements;

  nr_vector_dtor_t dtor;
  void* dtor_userdata;
} nr_vector_t;

/*
 * Purpose : Allocate and return a new vector.
 *
 *           A vector allocated using this function must be destroyed with
 *           nr_vector_destroy().
 *
 * Params  : 1. The initial capacity of the vector. If this is 0, a built-in
 *              default will be used.
 *           2. An optional destructor to be invoked when an element is being
 *              destroyed.
 *           3. Optional user data to be provided when the destructor is
 *              invoked.
 *
 * Returns : A newly allocated vector, or NULL on error.
 */
extern nr_vector_t* nr_vector_create(size_t initial,
                                     nr_vector_dtor_t dtor,
                                     void* dtor_userdata);

/*
 * Purpose : Initialise a vector.
 *
 *           A vector initialised using this function must be finalised with
 *           nr_vector_deinit().
 *
 * Params  : 1. The newly initialized vector.
 *           2. The initial capacity of the vector. If this is 0, a built-in
 *              default will be used.
 *           3. An optional destructor to be invoked when an element is being
 *              destroyed.
 *           4. Optional user data to be provided when the destructor is
 *              invoked.
 *
 * Returns : True if the vector was initialised successfully; false otherwise.
 */
extern bool nr_vector_init(nr_vector_t* v,
                           size_t initial,
                           nr_vector_dtor_t dtor,
                           void* dtor_userdata);

/*
 * Purpose : Destroy a vector created with nr_vector_create().
 *
 * Returns : True if the vector was destroyed successfully; false otherwise.
 */
extern bool nr_vector_destroy(nr_vector_t** v_ptr);

/*
 * Purpose : Finalise a vector initialised with nr_vector_init().
 *
 * Returns : True if the vector was finalised successfully; false otherwise.
 */
extern bool nr_vector_deinit(nr_vector_t* v);

/*
 * Purpose : Return the current capacity of a vector.
 *
 * Params  : 1. The vector.
 *
 * Returns : The capacity of the vector, or 0 on error.
 */
static inline size_t nr_vector_capacity(const nr_vector_t* v) {
  return v ? v->capacity : 0;
}

/*
 * Purpose : Return the current size of a vector.
 *
 * Params  : 1. The vector.
 *
 * Returns : The number of elements within the vector. If an error occurs, 0
 *           will be returned, but it is possible for a valid vector to have a
 *           size of 0.
 */
static inline size_t nr_vector_size(const nr_vector_t* v) {
  return v ? v->used : 0;
}

/*
 * Purpose : Ensure that a vector has room for at least the given number of
 *           elements.
 *
 *           This may be used as a hint to avoid unnecessary reallocations.
 *
 * Params  : 1. The vector.
 *           2. The minimum capacity to require.
 *
 * Returns : True if the vector was successfully allocated; false otherwise.
 */
extern bool nr_vector_ensure(nr_vector_t* v, size_t capacity);

/*
 * Purpose : Push an element to the front of a vector.
 *
 *           This is equivalent to nr_vector_insert(v, 0, element).
 *
 * Params  : 1. The vector.
 *           2. The element to push.
 *
 * Returns : True if the element was added succesfully; false otherwise.
 */
extern bool nr_vector_push_front(nr_vector_t* v, void* element);

/*
 * Purpose : Push an element to the back of a vector.
 *
 *           This is equivalent to
 *           nr_vector_insert(v, nr_vector_size(v), element).
 *
 * Params  : 1. The vector.
 *           2. The element to push.
 *
 * Returns : True if the element was added succesfully; false otherwise.
 */
extern bool nr_vector_push_back(nr_vector_t* v, void* element);

/*
 * Purpose : Pop the element at the front of a vector.
 *
 *           Note that ownership of the element passes to the caller; any
 *           destructor defined on the vector will NOT be invoked.
 *
 * Params  : 1. The vector.
 *           2. A pointer to a value that will receive the element.
 *
 * Returns : True if the element was removed succesfully; false otherwise.
 */
extern bool nr_vector_pop_front(nr_vector_t* v, void** element_ptr);

/*
 * Purpose : Pop the element at the back of a vector.
 *
 *           Note that ownership of the element passes to the caller; any
 *           destructor defined on the vector will NOT be invoked.
 *
 * Params  : 1. The vector.
 *           2. A pointer to a value that will receive the element.
 *
 * Returns : True if the element was removed succesfully; false otherwise.
 */
extern bool nr_vector_pop_back(nr_vector_t* v, void** element_ptr);

/*
 * Purpose : Insert a value into a vector.
 *
 * Params  : 1. The vector.
 *           2. The index to insert the new element before. (For example, index
 *              0 will insert the new element at the front of the vector.) If
 *              the index is greater than the size of the vector, then the
 *              element will be appended to the vector.
 *           3. The element to insert.
 *
 * Returns : True if the element was added succesfully; false otherwise.
 */
extern bool nr_vector_insert(nr_vector_t* v, size_t pos, void* element);

/*
 * Purpose : Remove an element from a vector.
 *
 *           Note that ownership of the element passes to the caller; any
 *           destructor defined on the vector will NOT be invoked.
 *
 * Params  : 1. The vector.
 *           2. The index of the element to remove.
 *           3. A pointer to a value that will receive the element.
 *
 * Returns : True if the element was removed succesfully; false otherwise.
 */
extern bool nr_vector_remove(nr_vector_t* v, size_t pos, void** element_ptr);

/*
 * Purpose : Access an element from a vector without removing it.
 *
 *           This shorthand form is only appropriate for code that either knows
 *           NULL elements can't be set, or doesn't need to distinguish between
 *           errors and NULL values.
 *
 * Params  : 1. The vector.
 *           2. The index of the element to access.
 *
 * Returns : The element, or NULL on error.
 */
static inline void* nr_vector_get(nr_vector_t* v, size_t pos) {
  if (NULL == v || pos >= v->used) {
    return NULL;
  }

  return v->elements[pos];
}

/*
 * Purpose : Access an element from a vector without removing it.
 *
 * Params  : 1. The vector.
 *           2. The index of the element to access.
 *           3. A pointer to a value that will receive the element.
 *
 * Returns : True if the element was accessed succesfully; false otherwise.
 */
extern bool nr_vector_get_element(nr_vector_t* v,
                                  size_t pos,
                                  void** element_ptr);

/*
 * Purpose : Replace a value within a vector.
 *
 * Params  : 1. The vector.
 *           2. The index of the element to replace. If the index is greater
 *              than the size of the vector, then the call will fail, and the
 *              vector will remain unchanged.
 *           3. The element to insert.
 *
 * Returns : True if the element was replaced succesfully; false otherwise.
 */
extern bool nr_vector_replace(nr_vector_t* v, size_t pos, void* element);

typedef int (*nr_vector_cmp_t)(const void* a, const void* b, void* userdata);

/*
 * Purpose : Sort a vector in place.
 *
 * Params  : 1. The vector.
 *           2. The comparison function to use when comparing elements within
 *              the vector. See nr_sort()'s compar documentation for more
 *              detail.
 *           3. The userdata to pass to the comparison function.
 *
 * Returns : True if the vector was successfully sorted; false otherwise.
 */
extern bool nr_vector_sort(nr_vector_t* v,
                           nr_vector_cmp_t comparator,
                           void* userdata);

typedef bool (*nr_vector_iter_t)(void* element, void* userdata);

/*
 * Purpose : Iterate over a vector.
 *
 * Params  : 1. The vector.
 *           2. The function to invoke for each element within the vector. The
 *              function may return false to stop iteration immediately.
 *           3. The user data to provide to the callback function.
 *
 * Returns : True if iteration successfully completed across the entire vector;
 *           false otherwise.
 */
extern bool nr_vector_iterate(nr_vector_t* v,
                              nr_vector_iter_t callback,
                              void* userdata);

/*
 * Purpose : Find the first instance of a value in a vector.
 *
 * Params  : 1. The vector.
 *           2. The value to search for.
 *           3. The comparison function to use when comparing elements within
 *              the vector. See nr_sort()'s compar documentation for more
 *              detail. If NULL, values will be compared by pointer value.
 *           4. The user data to pass to the comparison function.
 *           5. An optional pointer to a value that will receive the key of the
 *              found value. If no value is found, this value will not be set.
 *
 * Returns : True if the value was found; false otherwise or on error.
 */
extern bool nr_vector_find_first(const nr_vector_t* v,
                                 const void* needle,
                                 nr_vector_cmp_t comparator,
                                 void* userdata,
                                 size_t* index_ptr);

/*
 * Purpose : Find the last instance of a value in a vector.
 *
 * Params  : 1. The vector.
 *           2. The value to search for.
 *           3. The comparison function to use when comparing elements within
 *              the vector. See nr_sort()'s compar documentation for more
 *              detail. If NULL, values will be compared by pointer value.
 *           4. The user data to pass to the comparison function.
 *           5. An optional pointer to a value that will receive the key of the
 *              found value. If no value is found, this value will not be set.
 *
 * Returns : True if the value was found; false otherwise or on error.
 */
extern bool nr_vector_find_last(const nr_vector_t* v,
                                const void* needle,
                                nr_vector_cmp_t comparator,
                                void* userdata,
                                size_t* index_ptr);

#endif /* UTIL_VECTOR_HDR */
