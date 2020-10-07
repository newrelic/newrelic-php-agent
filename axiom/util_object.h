/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains general purpose abstract object storage.
 */
#ifndef UTIL_OBJECT_HDR
#define UTIL_OBJECT_HDR

#include <stdint.h>

#include "nr_axiom.h"
#include "util_buffer.h"

/*
 * We keep the generic object internals completely private.
 */
typedef struct _nrintobj_t nrobj_t;

/*
 * The different types of generic object we support.
 */
typedef enum _nrotype_t {
  NR_OBJECT_INVALID = -1,
  NR_OBJECT_NONE = 0,
  NR_OBJECT_BOOLEAN = 1,
  NR_OBJECT_INT = 4,
  NR_OBJECT_LONG = 5,
  NR_OBJECT_ULONG = 6,
  NR_OBJECT_DOUBLE = 7,
  NR_OBJECT_STRING = 8,
  NR_OBJECT_JSTRING = 9, /* Pre-formatted JSON string */
  NR_OBJECT_HASH = 10,
  NR_OBJECT_ARRAY = 11,
} nrotype_t;

/*
 * Create a blank generic object of the specified type.
 */
extern nrobj_t* nro_new(nrotype_t type);

/*
 * Delete a generic object freeing up all of its memory.
 */
extern void nro_real_delete(nrobj_t** obj);
#define nro_delete(O) nro_real_delete(&O)

/*
 * Return the type of a generic object.
 */
extern nrotype_t nro_type(const nrobj_t* obj);

/*
 * Return a duplicate of a generic object.
 */
extern nrobj_t* nro_copy(const nrobj_t* obj);

/*
 * Create a new generic object of the type, implied by the function name,
 * and call the setter to provide an initial value.
 *
 * Strings do NOT need to be duplicated when passed to this
 * function as they are duplicated internally.
 */
extern nrobj_t* nro_new_none(void);
extern nrobj_t* nro_new_boolean(int x);
extern nrobj_t* nro_new_int(int x);
extern nrobj_t* nro_new_long(int64_t x);
extern nrobj_t* nro_new_ulong(uint64_t x);
extern nrobj_t* nro_new_double(double x);
extern nrobj_t* nro_new_string(const char* x);
extern nrobj_t* nro_new_jstring(const char* x);
extern nrobj_t* nro_new_hash(void);
extern nrobj_t* nro_new_array(void);

/*
 * Object setters.
 *
 * Strings do NOT need to be duplicated when passed to this
 * function as they are duplicated internally.
 *
 * For arrays the index is 1-based NOT 0-based. An index of 0 always means "add
 * to the end of the array". For string values the string passed to this
 * function is duplicated internally, thus it is safe to pass a constant string
 * to this function.
 *
 * Returns a nr_status_t indicating success/failure.
 */
extern nr_status_t nro_set_hash(nrobj_t* obj,
                                const char* key,
                                const nrobj_t* value);
extern nr_status_t nro_set_array(nrobj_t* obj, int idx, const nrobj_t* value);

extern nr_status_t nro_set_hash_none(nrobj_t* obj, const char* key);
extern nr_status_t nro_set_hash_boolean(nrobj_t* obj,
                                        const char* key,
                                        int value);
extern nr_status_t nro_set_hash_int(nrobj_t* obj, const char* key, int value);
extern nr_status_t nro_set_hash_long(nrobj_t* obj,
                                     const char* key,
                                     int64_t value);
extern nr_status_t nro_set_hash_ulong(nrobj_t* obj,
                                      const char* key,
                                      uint64_t value);
extern nr_status_t nro_set_hash_double(nrobj_t* obj,
                                       const char* key,
                                       double value);
extern nr_status_t nro_set_hash_string(nrobj_t* obj,
                                       const char* key,
                                       const char* value);
extern nr_status_t nro_set_hash_jstring(nrobj_t* obj,
                                        const char* key,
                                        const char* value);

extern nr_status_t nro_set_array_none(nrobj_t* obj, int idx);
extern nr_status_t nro_set_array_boolean(nrobj_t* obj, int idx, int value);
extern nr_status_t nro_set_array_int(nrobj_t* obj, int idx, int value);
extern nr_status_t nro_set_array_long(nrobj_t* obj, int idx, int64_t value);
extern nr_status_t nro_set_array_ulong(nrobj_t* obj, int index, uint64_t value);
extern nr_status_t nro_set_array_double(nrobj_t* obj, int idx, double value);
extern nr_status_t nro_set_array_string(nrobj_t* obj,
                                        int idx,
                                        const char* value);
extern nr_status_t nro_set_array_jstring(nrobj_t* obj,
                                         int idx,
                                         const char* value);

/*
 * These type specific functions return the internal values of
 * a generic object. All of these functions take
 * a pointer to an integer as their last value which will be used to
 * store an error code. If this value is NULL then no error code will be
 * returned and it is up to the calling code to determine if the value
 * returned makes sense or not. In general all integral functions will
 * return -1 on error, and all string functions will return NULL. However,
 * if those are valid values, then using the error pointer is the only
 * way to determine if there was an error retrieving the value or not.
 *
 * When retrieving hash values by index, the index, like arrays, is 1
 * based, NOT 0 based like C arrays. The extra pointer in that function
 * is the name of the key at the given index.
 */
extern int nro_get_boolean(const nrobj_t* obj, nr_status_t* errp);
extern int nro_get_int(const nrobj_t* obj, nr_status_t* errp);
extern int nro_get_ival(const nrobj_t* obj, nr_status_t* errp);
extern int64_t nro_get_long(const nrobj_t* obj, nr_status_t* errp);
extern uint64_t nro_get_ulong(const nrobj_t* obj, nr_status_t* errp);
extern double nro_get_double(const nrobj_t* obj, nr_status_t* errp);
extern const char* nro_get_string(const nrobj_t* obj, nr_status_t* errp);
extern const char* nro_get_jstring(const nrobj_t* obj, nr_status_t* errp);
extern const nrobj_t* nro_get_hash_value(const nrobj_t* obj,
                                         const char* key,
                                         nr_status_t* errp);
extern const nrobj_t* nro_get_hash_value_by_index(const nrobj_t* obj,
                                                  int idx,
                                                  nr_status_t* errp,
                                                  const char** keyp);
extern const nrobj_t* nro_get_array_value(const nrobj_t* array,
                                          int idx,
                                          nr_status_t* errp);

extern int nro_get_hash_boolean(const nrobj_t* obj,
                                const char* key,
                                nr_status_t* errp);
extern int nro_get_hash_int(const nrobj_t* obj,
                            const char* key,
                            nr_status_t* errp);
extern int64_t nro_get_hash_long(const nrobj_t* obj,
                                 const char* key,
                                 nr_status_t* errp);
extern uint64_t nro_get_hash_ulong(const nrobj_t* obj,
                                   const char* key,
                                   nr_status_t* errp);
extern const char* nro_get_hash_string(const nrobj_t* obj,
                                       const char* key,
                                       nr_status_t* errp);
extern const char* nro_get_hash_jstring(const nrobj_t* obj,
                                        const char* key,
                                        nr_status_t* errp);
extern double nro_get_hash_double(const nrobj_t* obj,
                                  const char* key,
                                  nr_status_t* errp);
extern const nrobj_t* nro_get_hash_hash(const nrobj_t* obj,
                                        const char* key,
                                        nr_status_t* errp);
extern const nrobj_t* nro_get_hash_array(const nrobj_t* obj,
                                         const char* key,
                                         nr_status_t* errp);

extern int nro_get_array_boolean(const nrobj_t* obj,
                                 int key,
                                 nr_status_t* errp);
extern int nro_get_array_int(const nrobj_t* obj, int key, nr_status_t* errp);
extern int64_t nro_get_array_long(const nrobj_t* obj,
                                  int key,
                                  nr_status_t* errp);
extern uint64_t nro_get_array_ulong(const nrobj_t* obj,
                                    int key,
                                    nr_status_t* errp);
extern int nro_get_array_ival(const nrobj_t* obj, int key, nr_status_t* errp);
extern const char* nro_get_array_string(const nrobj_t* obj,
                                        int key,
                                        nr_status_t* errp);
extern const char* nro_get_array_jstring(const nrobj_t* obj,
                                         int key,
                                         nr_status_t* errp);
extern double nro_get_array_double(const nrobj_t* obj,
                                   int key,
                                   nr_status_t* errp);
extern const nrobj_t* nro_get_array_hash(const nrobj_t* obj,
                                         int key,
                                         nr_status_t* errp);
extern const nrobj_t* nro_get_array_array(const nrobj_t* obj,
                                          int key,
                                          nr_status_t* errp);

/*
 * Retrieve the size of an array or hash.
 * Returns -1 if there is some problem.
 */
extern int nro_getsize(const nrobj_t* obj);

/*
 * Iterate over the keys in a hash, calling the specified function argument
 * for each key/value pair.
 *
 * The iteration function must return NR_SUCCESS
 * for success (continue iteration), or NR_FAILURE for failure.
 *
 * The iteration stops at the first failure returned by the
 * specified function argument.
 */
typedef nr_status_t (*nrhashiter_t)(const char* key,
                                    const nrobj_t* val,
                                    void* ptr);
extern void nro_iteratehash(const nrobj_t* obj, nrhashiter_t func, void* ptr);

/*
 * Finds the given integer with the array.  Returns the index of the integer
 * if it is found (starting at one), or -1 if is not found.
 */
extern int nro_find_array_int(const nrobj_t* array, int x);

/*
 * Produce a JSON string given a generic object. The return string is allocated
 * on the heap and must be freed when appropriate.
 */
extern char* nro_to_json(const nrobj_t* obj);

/*
 * Produce a JSON string given a generic object. The string is appended to the
 * given buffer.
 */
extern nr_status_t nro_to_json_buffer(const nrobj_t* obj, nrbuf_t* buf);

/*
 * Create a generic object given a JSON string.
 *
 * Returns : A newly allocated nrobj_t or 0 if the JSON is invalid.
 */
extern nrobj_t* nro_create_from_json(const char* json);

/*
 * Create a generic object given an unterminated JSON string.
 *
 * Returns : A newly allocated nrobj_t or NULL if the JSON is invalid.
 */
extern nrobj_t* nro_create_from_json_unterminated(const char* json, int len);

/*
 * Dump an object into a string to expose object internals for testing.
 */
extern char* nro_dump(const nrobj_t* obj);

#endif /* UTIL_OBJECT_HDR */
